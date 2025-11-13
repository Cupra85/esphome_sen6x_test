#pragma once

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/button/button.h"
#include "esphome/components/sensirion_common/i2c_sensirion.h"
#include "esphome/core/application.h"

namespace esphome {
namespace sen6x {

enum ERRORCODE {
  COMMUNICATION_FAILED,
  SERIAL_NUMBER_IDENTIFICATION_FAILED,
  MEASUREMENT_INIT_FAILED,
  PRODUCT_NAME_FAILED,
  FIRMWARE_FAILED,
  UNKNOWN
};

const uint32_t SHORTEST_BASELINE_STORE_INTERVAL = 10800;
const uint32_t MAXIMUM_STORAGE_DIFF = 50;

struct Sen5xBaselines {
  int32_t state0;
  int32_t state1;
} PACKED;

struct GasTuning {
  uint16_t index_offset;
  uint16_t learning_time_offset_hours;
  uint16_t learning_time_gain_hours;
  uint16_t gating_max_duration_minutes;
  uint16_t std_initial;
  uint16_t gain_factor;
};

struct TemperatureCompensation {
  int16_t offset;
  int16_t normalized_offset_slope;
  uint16_t time_constant;
};

class SEN5XComponent : public PollingComponent, public sensirion_common::SensirionI2CDevice {
 public:
  float get_setup_priority() const override { return setup_priority::DATA; }
  void setup() override;
  void dump_config() override;
  void update() override;

  enum Sen5xType { SEN50, SEN54, SEN55, UNKNOWN };

  // Standard (schon vorhanden)
  void set_pm_1_0_sensor(sensor::Sensor *v) { pm_1_0_sensor_ = v; }
  void set_pm_2_5_sensor(sensor::Sensor *v) { pm_2_5_sensor_ = v; }
  void set_pm_4_0_sensor(sensor::Sensor *v) { pm_4_0_sensor_ = v; }
  void set_pm_10_0_sensor(sensor::Sensor *v) { pm_10_0_sensor_ = v; }
  void set_voc_sensor(sensor::Sensor *v) { voc_sensor_ = v; }
  void set_nox_sensor(sensor::Sensor *v) { nox_sensor_ = v; }
  void set_humidity_sensor(sensor::Sensor *v) { humidity_sensor_ = v; }
  void set_temperature_sensor(sensor::Sensor *v) { temperature_sensor_ = v; }
  void set_co2_sensor(sensor::Sensor *v) { co2_sensor_ = v; }

  // NEU: Number concentration
  void set_nc_03_sensor(sensor::Sensor *v) { nc_03_sensor_ = v; }
  void set_nc_05_sensor(sensor::Sensor *v) { nc_05_sensor_ = v; }
  void set_nc_10_sensor(sensor::Sensor *v) { nc_10_sensor_ = v; }
  void set_nc_25_sensor(sensor::Sensor *v) { nc_25_sensor_ = v; }
  void set_nc_40_sensor(sensor::Sensor *v) { nc_40_sensor_ = v; }
  void set_nc_100_sensor(sensor::Sensor *v) { nc_100_sensor_ = v; }

  // NEU: Raw values
  void set_raw_rh_x100_sensor(sensor::Sensor *v) { raw_rh_x100_sensor_ = v; }
  void set_raw_t_x200_sensor(sensor::Sensor *v) { raw_t_x200_sensor_ = v; }
  void set_raw_sraw_voc_sensor(sensor::Sensor *v) { raw_sraw_voc_sensor_ = v; }
  void set_raw_sraw_nox_sensor(sensor::Sensor *v) { raw_sraw_nox_sensor_ = v; }
  void set_raw_co2_sensor(sensor::Sensor *v) { raw_co2_sensor_ = v; }

  // NEU: TextSensoren
  void set_status_text_sensor(text_sensor::TextSensor *v) { status_text_ = v; }
  void set_version_text_sensor(text_sensor::TextSensor *v) { version_text_ = v; }
  void set_serial_text_sensor(text_sensor::TextSensor *v) { serial_text_ = v; }
  void set_product_text_sensor(text_sensor::TextSensor *v) { product_text_ = v; }

  // Settings
  void set_store_baseline(bool v) { store_baseline_ = v; }

  void set_voc_algorithm_tuning(uint16_t index_offset, uint16_t learning_time_offset_hours,
                                uint16_t learning_time_gain_hours, uint16_t gating_max_duration_minutes,
                                uint16_t std_initial, uint16_t gain_factor) {
    GasTuning tuning_params{index_offset, learning_time_offset_hours, learning_time_gain_hours,
                            gating_max_duration_minutes, std_initial, gain_factor};
    voc_tuning_params_ = tuning_params;
  }
  void set_nox_algorithm_tuning(uint16_t index_offset, uint16_t learning_time_offset_hours,
                                uint16_t learning_time_gain_hours, uint16_t gating_max_duration_minutes,
                                uint16_t gain_factor) {
    GasTuning tuning_params{index_offset, learning_time_offset_hours, learning_time_gain_hours,
                            gating_max_duration_minutes, 50, gain_factor};
    nox_tuning_params_ = tuning_params;
  }
  void set_temperature_compensation(float offset, float normalized_offset_slope, uint16_t time_constant) {
    TemperatureCompensation temp_comp;
    temp_comp.offset = offset * 200;
    temp_comp.normalized_offset_slope = normalized_offset_slope * 10000;
    temp_comp.time_constant = time_constant;
    temperature_compensation_ = temp_comp;
  }

  // SEND
  bool start_continuous_measurement();   // 0x0021 (4.8.1)
  bool stop_measurement();               // 0x0104 (4.8.3) - SEN6x
  bool device_reset();                   // 0xD304 (4.8.25)
  bool start_fan_cleaning();             // 0x5607 (4.8.27)
  bool activate_sht_heater();            // 0x6765 (4.8.29)

  // READ
  bool get_data_ready(bool &ready); // 0x0202 (4.8.5)

  // Measured values SEN66 (4.8.10)
  bool read_measured_values_sen66(uint16_t &pm1, uint16_t &pm25, uint16_t &pm4, uint16_t &pm10,
                                  int16_t &rh_x100, int16_t &t_x200,
                                  uint16_t &voc_idx, uint16_t &nox_idx, int16_t &co2_ppm);

  // Raw values SEN66 (4.8.14)
  bool read_raw_values_sen66(int16_t &rh_x100, int16_t &t_x200,
                             uint16_t &sraw_voc, uint16_t &sraw_nox, uint16_t &co2_ppm);

  // Number concentration (4.8.15)
  bool read_number_concentration(uint16_t &nc03_x10, uint16_t &nc05_x10,
                                 uint16_t &nc10_x10, uint16_t &nc25_x10,
                                 uint16_t &nc40_x10, uint16_t &nc100_x10);

  // Identification / status
  bool get_product_name(std::string &name);                 // 0xD014 (4.8.18)
  bool get_serial_number_string(std::string &serial);       // 0xD033 (4.8.19)
  bool read_device_status(uint32_t &status);                // 0xD206 (4.8.21)
  bool read_and_clear_device_status(uint32_t &status);      // 0xD210 (4.8.23)
  bool get_version(uint8_t &fw_major, uint8_t &fw_minor);   // 0xD100 (4.8.24)
  bool get_sht_heater_measurements(int16_t &rh_x100, int16_t &t_x200); // 0x6790 (4.8.30)

 protected:
  // Helpers
  bool write_tuning_parameters_(uint16_t i2c_command, const GasTuning &tuning);
  bool write_temperature_compensation_(const TemperatureCompensation &compensation);
  bool read_words_u16_(uint16_t cmd, uint16_t *buf, uint8_t words, uint16_t exec_time_ms = 20);
  bool read_string32_(uint16_t cmd, std::string &out, uint8_t words, uint16_t exec_time_ms = 20);

  // State
  ERRORCODE error_code_{UNKNOWN};
  bool initialized_{false};

  // Standard outputs
  sensor::Sensor *pm_1_0_sensor_{nullptr};
  sensor::Sensor *pm_2_5_sensor_{nullptr};
  sensor::Sensor *pm_4_0_sensor_{nullptr};
  sensor::Sensor *pm_10_0_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *voc_sensor_{nullptr};
  sensor::Sensor *nox_sensor_{nullptr};
  sensor::Sensor *co2_sensor_{nullptr};

  // Number concentration
  sensor::Sensor *nc_03_sensor_{nullptr};
  sensor::Sensor *nc_05_sensor_{nullptr};
  sensor::Sensor *nc_10_sensor_{nullptr};
  sensor::Sensor *nc_25_sensor_{nullptr};
  sensor::Sensor *nc_40_sensor_{nullptr};
  sensor::Sensor *nc_100_sensor_{nullptr};

  // Raw values
  sensor::Sensor *raw_rh_x100_sensor_{nullptr};
  sensor::Sensor *raw_t_x200_sensor_{nullptr};
  sensor::Sensor *raw_sraw_voc_sensor_{nullptr};
  sensor::Sensor *raw_sraw_nox_sensor_{nullptr};
  sensor::Sensor *raw_co2_sensor_{nullptr};

  // Text sensors
  text_sensor::TextSensor *status_text_{nullptr};
  text_sensor::TextSensor *version_text_{nullptr};
  text_sensor::TextSensor *serial_text_{nullptr};
  text_sensor::TextSensor *product_text_{nullptr};

  // Identification
  std::string product_name_;
  uint8_t serial_number_[4]{};
  uint16_t firmware_version_{0};

  // Baseline/Prefs
  Sen5xBaselines voc_baselines_storage_{};
  bool store_baseline_{false};
  uint32_t seconds_since_last_store_{0};
  ESPPreferenceObject pref_;
  optional<GasTuning> voc_tuning_params_;
  optional<GasTuning> nox_tuning_params_;
  optional<TemperatureCompensation> temperature_compensation_;
};

}  // namespace sen6x
}  // namespace esphome
