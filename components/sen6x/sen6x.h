#pragma once
#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace sen6x {

// Familien-Typen
enum class Variant {
  AUTO = 0,
  SEN60,
  SEN63C,
  SEN65,
  SEN66,
  SEN68,
  SEN6X_UNKNOWN
};

struct Sen6xValues {
  // PM (immer vorhanden)
  float pm1_0 = NAN, pm2_5 = NAN, pm4_0 = NAN, pm10 = NAN;
  // Number concentration (SEN6x via separate read; SEN60 in main read)
  float nc0_5 = NAN, nc1_0 = NAN, nc2_5 = NAN, nc4_0 = NAN, nc10 = NAN;
  // RH/T, VOC/NOx, CO2
  float rh = NAN, t = NAN, voc_index = NAN, nox_index = NAN, co2 = NAN;
};

class SEN6xComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  void set_variant(Variant v) { variant_config_ = v; }
  void set_update_every(bool use_data_ready) { use_drdy_ = use_data_ready; }

  // Buttons / Aktionen
  void start_measurement();
  void stop_measurement();
  void start_fan_cleaning();
  void device_reset();
  void clear_status_sen6x(); // Read&Clear (nur SEN6x)

  // Optional: Getter für letzte Messwerte
  const Sen6xValues &values() const { return values_; }

  // Auto-Erkennung + Initialisierung
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Messzyklus
  void update() override;

 protected:
  // Low-level Helfer
  bool send_cmd16(uint16_t cmd);
  bool write_words_with_crc_(uint16_t cmd, const std::vector<uint16_t> &words);
  bool read_bytes_(uint16_t cmd, uint8_t *buf, size_t len, uint32_t exec_delay_ms);
  bool read_words_crc_(uint16_t cmd, std::vector<uint16_t> &out, uint32_t exec_delay_ms);
  uint8_t crc8_(uint8_t a, uint8_t b); // CRC für 16-Bit Wort (8-Bit CRC)
  bool is_sen60_() const { return effective_variant_ == Variant::SEN60; }
  bool is_sen6x_() const { return effective_variant_ == Variant::SEN63C || effective_variant_ == Variant::SEN65 || effective_variant_ == Variant::SEN66 || effective_variant_ == Variant::SEN68; }

  // Kommandos (IDs laut Datenblatt Tabelle 26)
  uint16_t cmd_start_() const;
  uint16_t cmd_stop_() const;
  uint16_t cmd_get_data_ready_() const;
  uint16_t cmd_read_measured_values_() const;
  uint16_t cmd_read_number_concentration_() const { return 0x0316; } // SEN6x :contentReference[oaicite:4]{index=4}
  uint16_t cmd_fan_cleaning_() const;
  uint16_t cmd_device_reset_() const;
  uint16_t cmd_status_read_() const;
  uint16_t cmd_status_readclear_() const { return 0xD210; } // nur SEN6x :contentReference[oaicite:5]{index=5}
  uint16_t cmd_get_product_name_() const { return 0xD014; } // SEN6x :contentReference[oaicite:6]{index=6}

  // Parser
  bool parse_measured_values_(const std::vector<uint16_t> &w);
  bool parse_sen60_values_bytes_(const std::vector<uint8_t> &b);

  // Status
  void read_and_log_status_();

  Variant variant_config_ = Variant::AUTO;
  Variant effective_variant_ = Variant::SEN6X_UNKNOWN;
  bool use_drdy_{true};
  Sen6xValues values_;
  bool measurement_started_{false};
};

}  // namespace sen6x
}  // namespace esphome
