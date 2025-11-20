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
static const uint16_t SEN5X_CMD_START_CLEANING_FAN = 0x5607;
static const uint16_t SEN5X_CMD_START_MEASUREMENTS = 0x0021;
static const uint16_t SEN5X_CMD_STOP_MEASUREMENTS = 0x0104;
static const uint16_t SEN5X_CMD_TEMPERATURE_COMPENSATION = 0x60B2;
static const uint16_t SEN5X_CMD_VOC_ALGORITHM_STATE = 0x6181;
static const uint16_t SEN5X_CMD_VOC_ALGORITHM_TUNING = 0x60D0;
static const uint16_t SEN6X_CMD_RESET = 0xD304;
static const uint16_t SEN6X_CMD_READ_NUMBER_CONCENTRATION = 0x0316;


void SEN5XComponent::update() {
  if (!initialized_) {
    return;
  }

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

    float pm_1_0 = (measurements[0] == 0xFFFF) ? NAN : (measurements[0] / 10.0);
    float pm_2_5 = (measurements[1] == 0xFFFF || measurements[0] == 0xFFFF) ? NAN : ((measurements[1] - measurements[0]) / 10.0);
    float pm_4_0 = (measurements[2] == 0xFFFF || measurements[1] == 0xFFFF) ? NAN : ((measurements[2] - measurements[1]) / 10.0);
    float pm_10_0 = (measurements[3] == 0xFFFF || measurements[2] == 0xFFFF) ? NAN : ((measurements[3] - measurements[2]) / 10.0);
    float pm_0_10 = (measurements[3] == 0xFFFF) ? NAN : (measurements[3] / 10.0);
    float humidity = (measurements[4] == 0xFFFF) ? NAN : (measurements[4] / 100.0);
    float temperature = (measurements[5] == 0xFFFF) ? NAN : ((int16_t) measurements[5] / 200.0);
    float voc = (measurements[6] == 0x7FFF) ? NAN : (measurements[6] / 10.0);
    float nox = (measurements[7] == 0x7FFF) ? NAN : (measurements[7] / 10.0);
    float co2 = (measurements[8] == 0xFFFF) ? NAN : (measurements[8]);

    // --- WICHTIG: 50ms warten, vorher liefert 0x0316 NACK ---
    this->set_timeout(50, [this, pm_1_0, pm_2_5, pm_4_0, pm_10_0, pm_0_10, humidity, temperature, voc, nox, co2]() {

      uint16_t nc05, nc10, nc25, nc40, nc100;
      if (this->read_number_concentration(&nc05, &nc10, &nc25, &nc40, &nc100)) {
        if (this->nc_0_5_sensor_ != nullptr) this->nc_0_5_sensor_->publish_state(nc05);
        if (this->nc_1_0_sensor_ != nullptr) this->nc_1_0_sensor_->publish_state(nc10);
        if (this->nc_2_5_sensor_ != nullptr) this->nc_2_5_sensor_->publish_state(nc25);
        if (this->nc_4_0_sensor_ != nullptr) this->nc_4_0_sensor_->publish_state(nc40);
        if (this->nc_10_0_sensor_ != nullptr) this->nc_10_0_sensor_->publish_state(nc100);
      }

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
  });
}

bool SEN5XComponent::read_number_concentration(uint16_t *nc05, uint16_t *nc10,
                                               uint16_t *nc25, uint16_t *nc40,
                                               uint16_t *nc100) {

  uint16_t raw[5];

  if (!this->write_command(SEN6X_CMD_READ_NUMBER_CONCENTRATION)) {
    this->status_set_warning();
    ESP_LOGE(TAG, "Error writing Number Concentration command (0x0316), err=%d",
             this->last_error_);
    return false;
  }

  if (!this->read_data(raw, 5)) {
    this->status_set_warning();
    ESP_LOGE(TAG, "Error reading Number Concentration values (0x0316), err=%d",
             this->last_error_);
    return false;
  }

  *nc05  = raw[0];
  *nc10  = raw[1];
  *nc25  = raw[2];
  *nc40  = raw[3];
  *nc100 = raw[4];
  return true;
}

}  // namespace sen6x
}  // namespace esphome
