#include "sen6x.h"
#include "esphome/core/log.h"
#include <vector>

namespace esphome {
namespace sen6x {

static const char *TAG = "sen6x";

// ===== CRC (Sensirion-typisch über 16-bit Wort -> 8-bit CRC) =====
uint8_t SEN6xComponent::crc8_(uint8_t a, uint8_t b) {
  // Standard-Sensirion CRC8: Poly 0x31, Init 0xFF
  uint8_t crc = 0xFF;
  auto step = [&](uint8_t data) {
    crc ^= data;
    for (int i = 0; i < 8; i++) crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : (crc << 1);
  };
  step(a);
  step(b);
  return crc;
}

// ===== Command IDs je nach Variante (Tabelle 26) =====
uint16_t SEN6xComponent::cmd_start_() const { return is_sen60_() ? 0x2152 : 0x0021; } // :contentReference[oaicite:7]{index=7}
uint16_t SEN6xComponent::cmd_stop_() const  { return is_sen60_() ? 0x3F86 : 0x0104; } // :contentReference[oaicite:8]{index=8}
uint16_t SEN6xComponent::cmd_get_data_ready_() const { return is_sen60_() ? 0xE4B8 : 0x0202; } // :contentReference[oaicite:9]{index=9}
uint16_t SEN6xComponent::cmd_read_measured_values_() const {
  switch (effective_variant_) {
    case Variant::SEN60: return 0xEC05; // :contentReference[oaicite:10]{index=10}
    case Variant::SEN63C: return 0x0471; // :contentReference[oaicite:11]{index=11}
    case Variant::SEN65:  return 0x0446; // :contentReference[oaicite:12]{index=12}
    case Variant::SEN66:  return 0x0300; // :contentReference[oaicite:13]{index=13}
    case Variant::SEN68:  return 0x0467; // :contentReference[oaicite:14]{index=14}
    default:              return 0x0471; // Fallback
  }
}
uint16_t SEN6xComponent::cmd_fan_cleaning_() const { return is_sen60_() ? 0x3730 : 0x5607; } // :contentReference[oaicite:15]{index=15}
uint16_t SEN6xComponent::cmd_device_reset_() const { return is_sen60_() ? 0x3F8D : 0xD304; } // :contentReference[oaicite:16]{index=16}
uint16_t SEN6xComponent::cmd_status_read_() const  { return is_sen60_() ? 0xE00B : 0xD206; } // :contentReference[oaicite:17]{index=17}

void SEN6xComponent::setup() {
  // I2C-Adresse setzen nach Variante: SEN60=0x6C, SEN6x=0x6B (Datenblatt) :contentReference[oaicite:18]{index=18}
  if (variant_config_ == Variant::SEN60) {
    this->set_i2c_address(0x6C);
    effective_variant_ = Variant::SEN60;
  } else if (variant_config_ != Variant::AUTO) {
    this->set_i2c_address(0x6B);
    effective_variant_ = variant_config_;
  } else {
    // AUTO: erst 0x6B probieren (SEN6x), dann 0x6C (SEN60)
    this->set_i2c_address(0x6B);
    if (!this->write(nullptr, 0)) {
      this->set_i2c_address(0x6C);
    }
    // Grobe Erkennung via Produktname (SEN6x) oder Read-Status:
    std::vector<uint16_t> dummy;
    if (read_words_crc_(cmd_status_read_(), dummy, 20)) {
      // Wenn Status-Länge wie SEN6x (32-bit), nehmen wir SEN6x; SEN60 -> 16-bit
      if (this->get_address() == 0x6B) effective_variant_ = Variant::SEN66; // als „voll“ default
      else effective_variant_ = Variant::SEN60;
    } else {
      // Fallback: Adresse bestimmt Variante
      effective_variant_ = (this->get_address() == 0x6C) ? Variant::SEN60 : Variant::SEN66;
    }
  }

  // Messung starten (initial)
  start_measurement();
}

void SEN6xComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "SEN6x family sensor");
  ESP_LOGCONFIG(TAG, "  I2C Address: 0x%02X", this->get_address());
  ESP_LOGCONFIG(TAG, "  Variant: %d", static_cast<int>(effective_variant_));
  ESP_LOGCONFIG(TAG, "  Use data-ready polling: %s", use_drdy_ ? "yes" : "no");
}

void SEN6xComponent::update() {
  if (!measurement_started_) return;

  // optional Data-Ready abfragen (SEN6x 0x0202, SEN60 0xE4B8) :contentReference[oaicite:19]{index=19}
  bool ready = true;
  if (use_drdy_) {
    std::vector<uint16_t> dr;
    if (read_words_crc_(cmd_get_data_ready_(), dr, is_sen60_() ? 1 : 20)) {
      // SEN6x: 1 bool in Byte 1 (wir lesen als uint16 Wort #0); SEN60: Bitfeld ungleich 0
      if (is_sen60_()) ready = (dr.size() > 0 && (dr[0] & 0x07FF) != 0);
      else             ready = (dr.size() > 0 && ((dr[0] & 0x00FF) == 0x0001));
    }
  }
  if (!ready) return;

  if (is_sen60_()) {
    // SEN60 „Read Measured Values“ liefert PM + Number Concentration zusammen (0xEC05) :contentReference[oaicite:20]{index=20}
    const uint16_t cmd = cmd_read_measured_values_();
    // SEN60 definiert 9 Wörter (mit CRC je Wort als Bytes) -> wir lesen rohes Bytearray und parsen
    std::vector<uint8_t> bytes(27, 0);
    if (read_bytes_(cmd, bytes.data(), bytes.size(), 1)) {
      parse_sen60_values_bytes_(bytes);
    }
  } else {
    // SEN6x: Messwerte (variiert nach Modell) :contentReference[oaicite:21]{index=21}
    std::vector<uint16_t> w;
    if (read_words_crc_(cmd_read_measured_values_(), w, 20)) {
      parse_measured_values_(w);
    }
    // Optional: Number concentration separat (0x0316) :contentReference[oaicite:22]{index=22}
    std::vector<uint16_t> nc;
    if (read_words_crc_(cmd_read_number_concentration_(), nc, 20) && nc.size() >= 10) {
      values_.nc0_5 = nc[0] / 10.0f;
      values_.nc1_0 = nc[1] / 10.0f;
      values_.nc2_5 = nc[2] / 10.0f;
      values_.nc4_0 = nc[3] / 10.0f;
      values_.nc10  = nc[4] / 10.0f;
    }
  }

  // Status (optional ins Log)
  read_and_log_status_();
}

// ===== High-level Aktionen (Buttons) =====
void SEN6xComponent::start_measurement() {
  if (send_cmd16(cmd_start_())) {
    measurement_started_ = true;
    ESP_LOGI(TAG, "Measurement started");
  }
}
void SEN6xComponent::stop_measurement() {
  if (send_cmd16(cmd_stop_())) {
    measurement_started_ = false;
    ESP_LOGI(TAG, "Measurement stopped");
  }
}
void SEN6xComponent::start_fan_cleaning() {
  if (send_cmd16(cmd_fan_cleaning_())) {
    ESP_LOGI(TAG, "Fan cleaning triggered");
  }
}
void SEN6xComponent::device_reset() {
  if (send_cmd16(cmd_device_reset_())) {
    measurement_started_ = false;
    ESP_LOGW(TAG, "Device reset sent");
  }
}
void SEN6xComponent::clear_status_sen6x() {
  if (!is_sen6x_()) return;
  std::vector<uint16_t> dummy;
  read_words_crc_(cmd_status_readclear_(), dummy, 20);
}

// ===== Low-level I2C Helpers =====
bool SEN6xComponent::send_cmd16(uint16_t cmd) {
  uint8_t frame[2] = { static_cast<uint8_t>((cmd >> 8) & 0xFF), static_cast<uint8_t>(cmd & 0xFF) };
  auto err = this->write(frame, 2);
  if (err) return true;
  ESP_LOGW(TAG, "I2C send 0x%04X failed", cmd);
  return false;
}

bool SEN6xComponent::write_words_with_crc_(uint16_t cmd, const std::vector<uint16_t> &words) {
  std::vector<uint8_t> buf;
  buf.push_back((cmd >> 8) & 0xFF);
  buf.push_back(cmd & 0xFF);
  for (auto w : words) {
    uint8_t msb = (w >> 8) & 0xFF, lsb = w & 0xFF;
    buf.push_back(msb); buf.push_back(lsb);
    buf.push_back(crc8_(msb, lsb));
  }
  return this->write(buf.data(), buf.size());
}

bool SEN6xComponent::read_bytes_(uint16_t cmd, uint8_t *buf, size_t len, uint32_t exec_delay_ms) {
  if (!send_cmd16(cmd)) return false;
  delay(exec_delay_ms);
  auto ok = this->read(buf, len);
  if (!ok) ESP_LOGW(TAG, "I2C read bytes for 0x%04X failed", cmd);
  return ok;
}

bool SEN6xComponent::read_words_crc_(uint16_t cmd, std::vector<uint16_t> &out, uint32_t exec_delay_ms) {
  // Wir lesen konservativ bis 64 Bytes (genug für alle „read measured values“ Varianten)
  uint8_t tmp[64];
  if (!read_bytes_(cmd, tmp, sizeof(tmp), exec_delay_ms)) return false;
  // Wir akzeptieren auch, wenn das Gerät weniger Daten liefert -> bytecount via this->available() gibt es hier nicht,
  // daher versuchen wir, bis zum letzten 3er-Block (MSB,LSB,CRC) zu dekodieren.
  out.clear();
  size_t n = 0;
  // Heuristik: finde das größte Vielfache von 3, aber max 60
  size_t max = 60;
  for (size_t i = 0; i + 2 < max; i += 3) {
    uint8_t msb = tmp[i], lsb = tmp[i+1], c = tmp[i+2];
    if (crc8_(msb, lsb) != c) {
      // wenn CRC nicht stimmt, stoppen wir (manche Controller ignorieren CRC laut Datenblatt erlaubt)
      break;
    }
    uint16_t w = (static_cast<uint16_t>(msb) << 8) | lsb;
    out.push_back(w);
    n = i + 3;
  }
  return !out.empty();
}

// ===== Parser =====
bool SEN6xComponent::parse_sen60_values_bytes_(const std::vector<uint8_t> &b) {
  if (b.size() < 27) return false;
  auto U16 = [&](int i)->uint16_t { return (uint16_t(b[i]) << 8) | b[i+1]; };
  values_.pm1_0 = U16(0)  / 10.0f;
  values_.pm2_5 = U16(3)  / 10.0f;
  values_.pm4_0 = U16(6)  / 10.0f;
  values_.pm10  = U16(9)  / 10.0f;
  values_.nc0_5 = U16(12) / 10.0f;
  values_.nc1_0 = U16(15) / 10.0f;
  values_.nc2_5 = U16(18) / 10.0f;
  values_.nc4_0 = U16(21) / 10.0f;
  values_.nc10  = U16(24) / 10.0f;
  return true;
}

bool SEN6xComponent::parse_measured_values_(const std::vector<uint16_t> &w) {
  // Gemeinsamer Teil: PM1/2.5/4/10 zuerst (je /10) :contentReference[oaicite:23]{index=23}
  if (w.size() < 12) return false;
  values_.pm1_0 = (w[0] == 0xFFFF) ? NAN : w[0] / 10.0f;
  values_.pm2_5 = (w[1] == 0xFFFF) ? NAN : w[1] / 10.0f;
  values_.pm4_0 = (w[2] == 0xFFFF) ? NAN : w[2] / 10.0f;
  values_.pm10  = (w[3] == 0xFFFF) ? NAN : w[3] / 10.0f;

  // Danach variieren die Felder je Modell:
  switch (effective_variant_) {
    case Variant::SEN63C:
      // RH (/100), T (/200), CO2 (ppm) :contentReference[oaicite:24]{index=24}
      values_.rh =  (w[4] == 0x7FFF) ? NAN : w[4] / 100.0f;
      values_.t  =  (w[5] == 0x7FFF) ? NAN : w[5] / 200.0f;
      values_.co2 = (w[6] == 0x7FFF) ? NAN : (int16_t)w[6];
      break;
    case Variant::SEN65:
      // RH, T, VOC (/10), NOx (/10) :contentReference[oaicite:25]{index=25}
      values_.rh =  (w[4] == 0x7FFF) ? NAN : w[4] / 100.0f;
      values_.t  =  (w[5] == 0x7FFF) ? NAN : w[5] / 200.0f;
      values_.voc_index = (w[6] == 0x7FFF) ? NAN : (int16_t)w[6] / 10.0f;
      values_.nox_index = (w[7] == 0x7FFF) ? NAN : (int16_t)w[7] / 10.0f;
      break;
    case Variant::SEN66:
      // RH, T, VOC, NOx, CO2 :contentReference[oaicite:26]{index=26}
      values_.rh =  (w[4] == 0x7FFF) ? NAN : w[4] / 100.0f;
      values_.t  =  (w[5] == 0x7FFF) ? NAN : w[5] / 200.0f;
      values_.voc_index = (w[6] == 0x7FFF) ? NAN : (int16_t)w[6] / 10.0f;
      values_.nox_index = (w[7] == 0x7FFF) ? NAN : (int16_t)w[7] / 10.0f;
      values_.co2 = (w[8] == 0xFFFF) ? NAN : w[8];
      break;
    case Variant::SEN68:
      // RH, T, VOC, NOx (HCHO in anderem Kommando; hier identisch wie 66 ohne CO2) :contentReference[oaicite:27]{index=27}
      values_.rh =  (w[4] == 0x7FFF) ? NAN : w[4] / 100.0f;
      values_.t  =  (w[5] == 0x7FFF) ? NAN : w[5] / 200.0f;
      values_.voc_index = (w[6] == 0x7FFF) ? NAN : (int16_t)w[6] / 10.0f;
      values_.nox_index = (w[7] == 0x7FFF) ? NAN : (int16_t)w[7] / 10.0f;
      break;
    default: break;
  }
  return true;
}

void SEN6xComponent::read_and_log_status_() {
  std::vector<uint16_t> w;
  if (!read_words_crc_(cmd_status_read_(), w, is_sen60_() ? 1 : 20)) return;
  if (is_sen60_()) {
    uint16_t status = (w.empty() ? 0 : w[0]);
    ESP_LOGD(TAG, "SEN60 status=0x%04X", status);
  } else {
    if (w.size() >= 2) {
      uint32_t st = (uint32_t(w[0]) << 16) | w[1];
      ESP_LOGD(TAG, "SEN6x status=0x%08lX", (unsigned long)st);
    }
  }
}

}  // namespace sen6x
}  // namespace esphome
