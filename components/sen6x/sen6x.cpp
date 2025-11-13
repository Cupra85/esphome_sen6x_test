#include "sen6x.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome {
namespace sen6x {

static const char *const TAG = "sen6x";

// === Commands (SEN6x Datasheet 4.8) ===
static const uint16_t CMD_START_MEAS                 = 0x0021; // 4.8.1
static const uint16_t CMD_STOP_MEAS                  = 0x0104; // 4.8.3 (SEN6x)
static const uint16_t CMD_GET_DATA_READY             = 0x0202; // 4.8.5

// SEN66 measured/raw
static const uint16_t CMD_READ_MEAS_SEN66            = 0x0300; // 4.8.10
static const uint16_t CMD_READ_RAW_SEN66             = 0x0405; // 4.8.14

// Number concentration
static const uint16_t CMD_READ_NUM_CONC              = 0x0316; // 4.8.15

// Identification / status
static const uint16_t CMD_GET_PRODUCT_NAME           = 0xD014; // 4.8.18
static const uint16_t CMD_GET_SERIAL_STRING          = 0xD033; // 4.8.19
static const uint16_t CMD_READ_DEVICE_STATUS         = 0xD206; // 4.8.21
static const uint16_t CMD_READ_AND_CLEAR_STATUS      = 0xD210; // 4.8.23
static const uint16_t CMD_GET_VERSION                = 0xD100; // 4.8.24
static const uint16_t CMD_RESET                      = 0xD304; // 4.8.25

// Maintenance
static const uint16_t CMD_START_FAN_CLEANING         = 0x5607; // 4.8.27
static const uint16_t CMD_SHT_HEATER_ON              = 0x6765; // 4.8.29
static const uint16_t CMD_SHT_HEATER_MEAS            = 0x6790; // 4.8.30

// ---------------------------------------------------------------------------

bool SEN5XComponent::read_words_u16_(uint16_t cmd, uint16_t *buf, uint8_t words, uint16_t exec_time_ms) {
  if (!this->write_command(cmd)) return false;
  delay(exec_time_ms);
  return this->get_register(cmd, buf, words, exec_time_ms);
}

bool SEN5XComponent::read_string32_(uint16_t cmd, std::string &out, uint8_t words, uint16_t exec_time_ms) {
  uint16_t tmp[32] = {0};
  if (!this->read_words_u16_(cmd, tmp, words, exec_time_ms)) return false;
  out.clear();
  for (uint8_t i = 0; i < words; i++) {
    char hi = char(tmp[i] >> 8);
    char lo = char(tmp[i] & 0xFF);
    if (hi) out.push_back(hi); else break;
    if (lo) out.push_back(lo); else break;
  }
  return true;
}

void SEN5XComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SEN6x (SEN66)...");
  // Warte bis Idle (Datenblatt: ~100ms)
  this->set_timeout(200, [this]() {
    // Produktname & Serial
    std::string prod;
    if (this->get_product_name(prod)) {
      this->product_name_ = prod;
      ESP_LOGI(TAG, "Product: %s", prod.c_str());
      if (this->product_text_) this->product_text_->publish_state(prod);
    } else {
      ESP_LOGW(TAG, "Product name read failed");
    }
    std::string serial;
    if (this->get_serial_number_string(serial)) {
      ESP_LOGI(TAG, "Serial: %s", serial.c_str());
      if (this->serial_text_) this->serial_text_->publish_state(serial);
    }

    // Firmware Version
    uint8_t maj=0, min=0;
    if (this->get_version(maj, min)) {
      char buf[16]; snprintf(buf, sizeof(buf), "%u.%u", maj, min);
      ESP_LOGI(TAG, "FW Version: %s", buf);
      if (this->version_text_) this->version_text_->publish_state(buf);
    }

    initialized_ = true;
  });
}

void SEN5XComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "sen6x (SEN66):");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    switch (this->error_code_) {
      case COMMUNICATION_FAILED: ESP_LOGW(TAG, "Communication failed!"); break;
      case MEASUREMENT_INIT_FAILED: ESP_LOGW(TAG, "Measurement init failed!"); break;
      case PRODUCT_NAME_FAILED: ESP_LOGW(TAG, "Product name failed!"); break;
      case FIRMWARE_FAILED: ESP_LOGW(TAG, "Firmware version failed!"); break;
      case SERIAL_NUMBER_IDENTIFICATION_FAILED: ESP_LOGW(TAG, "Serial read failed!"); break;
      default: break;
    }
  }
}

void SEN5XComponent::update() {
  if (!initialized_) return;

  // READ status (TextSensor Hex)
  uint32_t st = 0;
  if (this->read_device_status(st) && this->status_text_) {
    char hex[11];
    snprintf(hex, sizeof(hex), "0x%08" PRIX32, st);
    this->status_text_->publish_state(hex);
  }

  // Get Data Ready -> Measured + NumberConc + Raw
  bool ready=false;
  if (this->get_data_ready(ready) && ready) {
    // Measured SEN66
    uint16_t pm1=0, pm25=0, pm4=0, pm10=0;
    int16_t rh_x100=0, t_x200=0;
    uint16_t voc_idx=0, nox_idx=0;
    int16_t co2_ppm=0;
    if (this->read_measured_values_sen66(pm1, pm25, pm4, pm10, rh_x100, t_x200, voc_idx, nox_idx, co2_ppm)) {
      if (pm_1_0_sensor_) pm_1_0_sensor_->publish_state(pm1 / 1.0f);
      if (pm_2_5_sensor_) pm_2_5_sensor_->publish_state(pm25 / 1.0f);
      if (pm_4_0_sensor_) pm_4_0_sensor_->publish_state(pm4 / 1.0f);
      if (pm_10_0_sensor_) pm_10_0_sensor_->publish_state(pm10 / 1.0f);
      if (humidity_sensor_) humidity_sensor_->publish_state(rh_x100 / 100.0f);
      if (temperature_sensor_) temperature_sensor_->publish_state(t_x200 / 200.0f);
      if (voc_sensor_) voc_sensor_->publish_state(voc_idx);
      if (nox_sensor_) nox_sensor_->publish_state(nox_idx);
      if (co2_sensor_) co2_sensor_->publish_state(co2_ppm);
    }

    // Number concentration
    uint16_t nc03=0,nc05=0,nc10=0,nc25=0,nc40=0,nc100=0;
    if (this->read_number_concentration(nc03, nc05, nc10, nc25, nc40, nc100)) {
      if (nc_03_sensor_) nc_03_sensor_->publish_state(nc03 / 10.0f);
      if (nc_05_sensor_) nc_05_sensor_->publish_state(nc05 / 10.0f);
      if (nc_10_sensor_) nc_10_sensor_->publish_state(nc10 / 10.0f);
      if (nc_25_sensor_) nc_25_sensor_->publish_state(nc25 / 10.0f);
      if (nc_40_sensor_) nc_40_sensor_->publish_state(nc40 / 10.0f);
      if (nc_100_sensor_) nc_100_sensor_->publish_state(nc100 / 10.0f);
    }

    // Raw values
    uint16_t sraw_voc=0, sraw_nox=0, co2_raw=0;
    if (this->read_raw_values_sen66(rh_x100, t_x200, sraw_voc, sraw_nox, co2_raw)) {
      if (raw_rh_x100_sensor_) raw_rh_x100_sensor_->publish_state((float) rh_x100);
      if (raw_t_x200_sensor_) raw_t_x200_sensor_->publish_state((float) t_x200);
      if (raw_sraw_voc_sensor_) raw_sraw_voc_sensor_->publish_state((float) sraw_voc);
      if (raw_sraw_nox_sensor_) raw_sraw_nox_sensor_->publish_state((float) sraw_nox);
      if (raw_co2_sensor_) raw_co2_sensor_->publish_state((float) co2_raw);
    }
  }
}

// ---------------- SEND / READ impl ----------------

bool SEN5XComponent::start_continuous_measurement() { return this->write_command(CMD_START_MEAS); }
bool SEN5XComponent::stop_measurement()              { return this->write_command(CMD_STOP_MEAS); }
bool SEN5XComponent::device_reset()                  { if (!this->write_command(CMD_RESET)) return false; delay(1200); return true; }
bool SEN5XComponent::start_fan_cleaning()            { return this->write_command(CMD_START_FAN_CLEANING); }
bool SEN5XComponent::activate_sht_heater()           { return this->write_command(CMD_SHT_HEATER_ON); }

bool SEN5XComponent::get_data_ready(bool &ready) {
  uint16_t w[1];
  if (!this->read_words_u16_(CMD_GET_DATA_READY, w, 1, 20)) return false;
  ready = (uint8_t)(w[0] & 0xFF);
  return true;
}

bool SEN5XComponent::read_measured_values_sen66(uint16_t &pm1, uint16_t &pm25, uint16_t &pm4, uint16_t &pm10,
                                                int16_t &rh_x100, int16_t &t_x200,
                                                uint16_t &voc_idx, uint16_t &nox_idx, int16_t &co2_ppm) {
  uint16_t b[10];
  if (!this->read_words_u16_(CMD_READ_MEAS_SEN66, b, 10, 20)) return false;
  pm1=b[0]; pm25=b[1]; pm4=b[2]; pm10=b[3];
  rh_x100=(int16_t)b[4]; t_x200=(int16_t)b[5];
  voc_idx=b[6]; nox_idx=b[7]; co2_ppm=(int16_t)b[8];
  return true;
}

bool SEN5XComponent::read_raw_values_sen66(int16_t &rh_x100, int16_t &t_x200,
                                           uint16_t &sraw_voc, uint16_t &sraw_nox, uint16_t &co2_ppm) {
  uint16_t b[5];
  if (!this->read_words_u16_(CMD_READ_RAW_SEN66, b, 5, 20)) return false;
  rh_x100=(int16_t)b[0]; t_x200=(int16_t)b[1]; sraw_voc=b[2]; sraw_nox=b[3]; co2_ppm=b[4];
  return true;
}

bool SEN5XComponent::read_number_concentration(uint16_t &nc03_x10, uint16_t &nc05_x10,
                                               uint16_t &nc10_x10, uint16_t &nc25_x10,
                                               uint16_t &nc40_x10, uint16_t &nc100_x10) {
  uint16_t b[6];
  if (!this->read_words_u16_(CMD_READ_NUM_CONC, b, 6, 20)) return false;
  nc03_x10=b[0]; nc05_x10=b[1]; nc10_x10=b[2]; nc25_x10=b[3]; nc40_x10=b[4]; nc100_x10=b[5];
  return true;
}

bool SEN5XComponent::get_product_name(std::string &name) { return this->read_string32_(CMD_GET_PRODUCT_NAME, name, 16, 20); }
bool SEN5XComponent::get_serial_number_string(std::string &serial) { return this->read_string32_(CMD_GET_SERIAL_STRING, serial, 16, 20); }

bool SEN5XComponent::read_device_status(uint32_t &status) {
  uint16_t w[2];
  if (!this->read_words_u16_(CMD_READ_DEVICE_STATUS, w, 2, 20)) return false;
  status = (uint32_t(w[0]) << 16) | w[1];
  return true;
}
bool SEN5XComponent::read_and_clear_device_status(uint32_t &status) {
  uint16_t w[2];
  if (!this->read_words_u16_(CMD_READ_AND_CLEAR_STATUS, w, 2, 20)) return false;
  status = (uint32_t(w[0]) << 16) | w[1];
  return true;
}
bool SEN5XComponent::get_version(uint8_t &fw_major, uint8_t &fw_minor) {
  uint16_t w[1];
  if (!this->read_words_u16_(CMD_GET_VERSION, w, 1, 20)) return false;
  fw_major = uint8_t(w[0] >> 8);
  fw_minor = uint8_t(w[0] & 0xFF);
  return true;
}
bool SEN5XComponent::get_sht_heater_measurements(int16_t &rh_x100, int16_t &t_x200) {
  uint16_t w[2];
  if (!this->read_words_u16_(CMD_SHT_HEATER_MEAS, w, 2, 20)) return false;
  rh_x100 = (int16_t)w[0]; t_x200=(int16_t)w[1];
  return true;
}

}  // namespace sen6x
}  // namespace esphome
