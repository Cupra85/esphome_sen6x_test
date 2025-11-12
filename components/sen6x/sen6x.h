#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace sen6x {

// =========================
//   SEN6x-Varianten
// =========================
enum class Variant : uint8_t {
  AUTO = 0,
  SEN60,
  SEN63C,
  SEN65,
  SEN66,
  SEN68,
};

// =========================
//   Datensatzstruktur
// =========================
struct SEN6xValues {
  float pm1_0 = NAN;
  float pm2_5 = NAN;
  float pm4_0 = NAN;
  float pm10  = NAN;
  float nc0_5 = NAN;
  float nc1_0 = NAN;
  float nc2_5 = NAN;
  float nc4_0 = NAN;
  float nc10  = NAN;
  float rh = NAN;
  float t = NAN;
  float voc_index = NAN;
  float nox_index = NAN;
  float co2 = NAN;
};

// =========================
//   Hauptklasse
// =========================
class SEN6xComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  SEN6xComponent() = default;

  void setup() override;
  void update() override;
  void dump_config() override;

  // High-level-Befehle (für Buttons / Automationen)
  void start_measurement();
  void stop_measurement();
  void start_fan_cleaning();
  void device_reset();
  void clear_status_sen6x();

  // Setter
  void set_variant(Variant v) { this->variant_config_ = v; }
  void set_update_every(bool use_drdy) { this->use_drdy_ = use_drdy; }

 protected:
  // --- interne Helper ---
  bool send_cmd16(uint16_t cmd);
  bool write_words_with_crc_(uint16_t cmd, const std::vector<uint16_t> &words);
  bool read_bytes_(uint16_t cmd, uint8_t *buf, size_t len, uint32_t exec_delay_ms);
  bool read_words_crc_(uint16_t cmd, std::vector<uint16_t> &out, uint32_t exec_delay_ms);
  uint8_t crc8_(uint8_t a, uint8_t b);

  // --- Parser ---
  bool parse_sen60_values_bytes_(const std::vector<uint8_t> &b);
  bool parse_measured_values_(const std::vector<uint16_t> &w);
  void read_and_log_status_();

  // --- Kommandos nach SEN6x-Datenblatt ---
  uint16_t cmd_start_() const;
  uint16_t cmd_stop_() const;
  uint16_t cmd_get_data_ready_() const;
  uint16_t cmd_read_measured_values_() const;
  uint16_t cmd_read_number_concentration_() const { return 0x0316; }
  uint16_t cmd_fan_cleaning_() const;
  uint16_t cmd_device_reset_() const;
  uint16_t cmd_status_read_() const;

  // --- interne Statusflags ---
  bool is_sen60_() const { return effective_variant_ == Variant::SEN60; }
  bool is_sen6x_() const { return true; }

  // --- Felder ---
  Variant variant_config_{Variant::AUTO};
  Variant effective_variant_{Variant::AUTO};
  bool measurement_started_{false};
  bool use_drdy_{true};
  SEN6xValues values_;
};

}  // namespace sen6x
}  // namespace esphome
