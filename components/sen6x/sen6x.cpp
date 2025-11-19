#include "sen6x.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome {
namespace sen6x {

static const char *const TAG = "sen6x";

//static const uint16_t SEN5X_CMD_AUTO_CLEANING_INTERVAL = 0x8004; //not used with Sen6X
static const uint16_t SEN5X_CMD_GET_DATA_READY_STATUS = 0x0202;
static const uint16_t SEN5X_CMD_GET_FIRMWARE_VERSION = 0xD100; //works but not documented
static const uint16_t SEN5X_CMD_GET_PRODUCT_NAME = 0xD014;  // return 0 bytes
static const uint16_t SEN5X_CMD_GET_SERIAL_NUMBER = 0xD033;
static const uint16_t SEN5X_CMD_NOX_ALGORITHM_TUNING = 0x60E1;
static const uint16_t SEN5X_CMD_READ_MEASUREMENT = 0x0300; //SEN66 only!
static const uint16_t SEN5X_CMD_RHT_ACCELERATION_MODE = 0x60F7;
static const uint16_t SEN5X_CMD_START_CLEANING_FAN = 0x5607;
static const uint16_t SEN5X_CMD_START_MEASUREMENTS = 0x0021;
static const uint16_t SEN5X_CMD_START_MEASUREMENTS_RHT_ONLY = 0x0037;
static const uint16_t SEN5X_CMD_STOP_MEASUREMENTS = 0x0104;
static const uint16_t SEN5X_CMD_TEMPERATURE_COMPENSATION = 0x60B2;
static const uint16_t SEN5X_CMD_VOC_ALGORITHM_STATE = 0x6181;
static const uint16_t SEN5X_CMD_VOC_ALGORITHM_TUNING = 0x60D0;
static const uint16_t SEN6X_CMD_RESET = 0xD304;
static const uint16_t SEN6X_CMD_READ_NUMBER_CONCENTRATION = 0x0316;

void SEN5XComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up sen6x...");

  this->set_timeout(2000, [this]() {

    if (!this->write_command(SEN5X_CMD_GET_DATA_READY_STATUS)) {
      ESP_LOGE(TAG, "Failed to write data ready status command");
      this->mark_failed();
      return;
    }

    uint16_t raw_read_status;
    if (!this->read_data(raw_read_status)) {
      ESP_LOGE(TAG, "Failed to read data ready status");
      this->mark_failed();
      return;
    }

    uint32_t stop_measurement_delay = 0;
    if (raw_read_status) {
      ESP_LOGD(TAG, "Sensor has data available, stopping periodic measurement / reset");
      if (!this->write_command(SEN6X_CMD_RESET)) {
        ESP_LOGE(TAG, "Failed to stop measurements / reset");
        this->mark_failed();
        return;
      }
      stop_measurement_delay = 1200;
    }

    this->set_timeout(stop_measurement_delay, [this]() {
      uint16_t raw_serial_number[3];
      if (!this->get_register(SEN5X_CMD_GET_SERIAL_NUMBER, raw_serial_number, 3, 20)) {
        ESP_LOGE(TAG, "Failed to read serial number");
        this->error_code_ = SERIAL_NUMBER_IDENTIFICATION_FAILED;
        this->mark_failed();
        return;
      }
      this->serial_number_[0] = static_cast<bool>(uint16_t(raw_serial_number[0]) & 0xFF);
      this->serial_number_[1] = static_cast<uint16_t>(raw_serial_number[0] & 0xFF);
      this->serial_number_[2] = static_cast<uint16_t>(raw_serial_number[1] >> 8);
      ESP_LOGD(TAG, "Serial number %02d.%02d.%02d", serial_number_[0], serial_number_[1], serial_number_[2]);

      uint16_t raw_product_name[16];
      if (!this->get_register(SEN5X_CMD_GET_PRODUCT_NAME, raw_product_name, 16, 20)) {
        ESP_LOGE(TAG, "Failed to read product name");
        this->error_code_ = PRODUCT_NAME_FAILED;
        this->mark_failed();
        return;
      }

      const uint16_t *current_int = raw_product_name;
      char current_char;
      uint8_t max = 16;
      do {
        current_char = *current_int >> 8;
        if (current_char) {
          product_name_.push_back(current_char);
          current_char = *current_int & 0xFF;
          if (current_char)
            product_name_.push_back(current_char);
        }
        current_int++;
      } while (current_char && --max);

      Sen5xType sen6x_type = UNKNOWN;
      if (product_name_ == "SEN50") {
        sen6x_type = SEN50;
      } else {
        if (product_name_ == "SEN54") {
          sen6x_type = SEN54;
        } else {
          if (product_name_ == "SEN55") {
            sen6x_type = SEN55;
          }
        }
        if (product_name_ == "SEN66" || product_name_ == "") {
          ESP_LOGD(TAG, "Productname for real: %s", product_name_.c_str());
          sen6x_type = SEN55; 
        }
        ESP_LOGD(TAG, "Productname %s", product_name_.c_str());
      }

      // Disable unsupported features for lower models
      if (this->humidity_sensor_ && sen6x_type == SEN50) {
        ESP_LOGE(TAG, "Humidity unsupported on SEN50");
        this->humidity_sensor_ = nullptr;
      }
      if (this->temperature_sensor_ && sen6x_type == SEN50) {
        ESP_LOGE(TAG, "Temperature unsupported on SEN50");
        this->temperature_sensor_ = nullptr;
      }
      if (this->voc_sensor_ && sen6x_type == SEN50) {
        ESP_LOGE(TAG, "VOC unsupported on SEN50");
        this->voc_sensor_ = nullptr;
      }
      if (this->nox_sensor_ && sen6x_type != SEN55) {
        ESP_LOGE(TAG, "NOx unsupported on this model");
        this->nox_sensor_ = nullptr;
      }

      if (!this->get_register(SEN5X_CMD_GET_FIRMWARE_VERSION, this->firmware_version_, 20)) {
        ESP_LOGE(TAG, "Failed to read firmware version");
        this->error_code_ = FIRMWARE_FAILED;
        this->mark_failed();
        return;
      }
      this->firmware_version_ >>= 8;
      ESP_LOGD(TAG, "Firmware version %d", this->firmware_version_);

      if (this->temperature_compensation_.has_value()) {
        this->write_temperature_compensation_(this->temperature_compensation_.value());
        delay(20);
      }

      auto cmd = SEN5X_CMD_START_MEASUREMENTS_RHT_ONLY;
      if (this->pm_1_0_sensor_ || this->pm_2_5_sensor_ || this->pm_4_0_sensor_ || this->pm_10_0_sensor_ || this->pm_0_10_sensor_) {
        cmd = SEN5X_CMD_START_MEASUREMENTS;
      }

      if (!this->write_command(cmd)) {
        ESP_LOGE(TAG, "Error starting continuous measurements.");
        this->error_code_ = MEASUREMENT_INIT_FAILED;
        this->mark_failed();
        return;
      }
      initialized_ = true;
      ESP_LOGD(TAG, "Sensor initialized");
    });
  });
}

void SEN5XComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "sen6x:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGW(TAG, "Sensor failed to initialize!");
  }
  ESP_LOGCONFIG(TAG, "  Productname: %s", this->product_name_.c_str());
  ESP_LOGCONFIG(TAG, "  Firmware version: %d", this->firmware_version_);
  ESP_LOGCONFIG(TAG, "  Serial number %02d.%02d.%02d", serial_number_[0], serial_number_[1], serial_number_[2]);

  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "PM  ≤1.0", this->pm_1_0_sensor_);
  LOG_SENSOR("  ", "PM  1.0-2.5", this->pm_2_5_sensor_);
  LOG_SENSOR("  ", "PM  2.5-4.0", this->pm_4_0_sensor_);
  LOG_SENSOR("  ", "PM 4.0-10.0", this->pm_10_0_sensor_);
  LOG_SENSOR("  ", "PM ≤10.0", this->pm_0_10_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "VOC", this->voc_sensor_);
  LOG_SENSOR("  ", "NOx", this->nox_sensor_);
  LOG_SENSOR("  ", "CO2", this->co2_sensor_);
}

void SEN5XComponent::update() {
  if (!initialized_)
    return;

  if (!this->write_command(SEN5X_CMD_READ_MEASUREMENT)) {
    this->status_set_warning();
    ESP_LOGD(TAG, "write error read measurement (%d)", this->last_error_);
    return;
  }

  this->set_timeout(20, [this]() {
    uint16_t measurements[9];

    if (!this->read_data(measurements, 9)) {
      this->status_set_warning();
      ESP_LOGD(TAG, "read data error (%d)", this->last_error_);
      return;
    }

    float pm_1_0 = measurements[0] / 10.0;
    if (measurements[0] == 0xFFFF) pm_1_0 = NAN;
    float pm_2_5 = (measurements[1] - measurements[0]) / 10.0;
    if (measurements[1] == 0xFFFF || measurements[0] == 0xFFFF) pm_2_5 = NAN;
    float pm_4_0 = (measurements[2] - measurements[1]) / 10.0;
    if (measurements[2] == 0xFFFF || measurements[1] == 0xFFFF) pm_4_0 = NAN;
    float pm_10_0 = (measurements[3] - measurements[2]) / 10.0;
    if (measurements[3] == 0xFFFF || measurements[2] == 0xFFFF) pm_10_0 = NAN;
    float pm_0_10 = measurements[3] / 10.0;
    if (measurements[3] == 0xFFFF) pm_0_10 = NAN;
    float humidity = measurements[4] / 100.0;
    if (measurements[4] == 0xFFFF) humidity = NAN;
    float temperature = (int16_t) measurements[5] / 200.0;
    if (measurements[5] == 0xFFFF) temperature = NAN;
    float voc = measurements[6] / 10.0;
    if (measurements[6] == 0x7FFF) voc = NAN;
    float nox = measurements[7] / 10.0;
    if (measurements[7] == 0x7FFF) nox = NAN;
    float co2 = measurements[8];
    if (measurements[8] == 0xFFFF) co2 = NAN;

    // NEW: READ NUMBER CONCENTRATION VALUES
    uint16_t nc05, nc10, nc25, nc40, nc100;
    if (this->read_number_concentration(&nc05, &nc10, &nc25, &nc40, &nc100)) {
      if (this->nc_0_5_sensor_ != nullptr) this->nc_0_5_sensor_->publish_state(nc05);
      if (this->nc_1_0_sensor_ != nullptr) this->nc_1_0_sensor_->publish_state(nc10);
      if (this->nc_2_5_sensor_ != nullptr) this->nc_2_5_sensor_->publish_state(nc25);
      if (this->nc_4_0_sensor_ != nullptr) this->nc_4_0_sensor_->publish_state(nc40);
      if (this->nc_10_0_sensor_ != nullptr) this->nc_10_0_sensor_->publish_state(nc100);
    }

    // PUBLISH STANDARD MEASUREMENTS
    if (this->pm_1_0_sensor_ != nullptr) this->pm_1_0_sensor_->publish_state(pm_1_0);
    if (this->pm_2_5_sensor_ != nullptr) this->pm_2_5_sensor_->publish_state(pm_2_5);
    if (this->pm_4_0_sensor_ != nullptr) this->pm_4_0_sensor_->publish_state(pm_4_0);
    if (this->pm_10_0_sensor_ != nullptr) this->pm_10_0_sensor_->publish_state(pm_10_0);
    if (this->pm_0_10_sensor_ != nullptr) this->pm_0_10_sensor_->publish_state(pm_0_10);
    if (this->temperature_sensor_ != nullptr) this->temperature_sensor_->publish_state(temperature);
    if (this->humidity_sensor_ != nullptr) this->humidity_sensor_->publish_state(humidity);
    if (this->voc_sensor_ != nullptr) this->voc_sensor_->publish_state(voc);
    if (this->nox_sensor_ != nullptr) this->nox_sensor_->publish_state(nox);
    if (this->co2_sensor_ != nullptr) this->co2_sensor_->publish_state(co2);
    this->status_clear_warning();
  });
}

bool SEN5XComponent::read_number_concentration(uint16_t *nc05, uint16_t *nc10,
                                               uint16_t *nc25, uint16_t *nc40,
                                               uint16_t *nc100) {
  uint8_t raw[5 * 3];
  if (!this->read_bytes(SEN6X_CMD_READ_NUMBER_CONCENTRATION, raw, sizeof(raw))) {
    this->status_set_warning();
    ESP_LOGE(TAG, "Error reading number concentration values (%d)", this->last_error_);
    return false;
  }

  auto get_val = [&](int idx) -> uint16_t {
    return (raw[idx] << 8) | raw[idx + 1];
  };

  *nc05  = get_val(0);
  *nc10  = get_val(3);
  *nc25  = get_val(6);
  *nc40  = get_val(9);
  *nc100 = get_val(12);

  return true;
}

bool SEN5XComponent::start_measurement() {
  if (!write_command(SEN5X_CMD_START_MEASUREMENTS)) {
    this->status_set_warning();
    ESP_LOGE(TAG, "write error start measurement (%d)", this->last_error_);
    return false;
  } else {
    ESP_LOGD(TAG, "Measurement started");
  }
  this->is_measuring_ = true;
  return true;
}

bool SEN5XComponent::stop_measurement() {
  if (!write_command(SEN5X_CMD_STOP_MEASUREMENTS)) {
    this->status_set_warning();
    ESP_LOGE(TAG, "write error stop measurement (%d)", this->last_error_);
    return false;
  } else {
    ESP_LOGD(TAG, "Measurement stopped");
  }
  this->is_measuring_ = false;
  return true;
}

bool SEN5XComponent::start_fan_cleaning() {
  if (!write_command(SEN5X_CMD_START_CLEANING_FAN)) {
    this->status_set_warning();
    ESP_LOGE(TAG, "write error start fan (%d)", this->last_error_);
    return false;
  } else {
    ESP_LOGD(TAG, "Fan auto clean started");
  }
  return true;
}

}  // namespace sen6x
}  // namespace esphome
