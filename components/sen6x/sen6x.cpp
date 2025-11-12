#include "sen6x.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <vector>

namespace esphome {
namespace sen6x {

static const char *TAG = "sen6x";

// ===== CRC =====
uint8_t SEN6xComponent::crc8_(uint8_t a, uint8_t b) {
  uint8_t crc = 0xFF;
  auto step = [&](uint8_t data) {
    crc ^= data;
    for (int i = 0; i < 8; i++)
      crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : (crc << 1);
  };
  step(a);
  step(b);
  return crc;
}

// ===== Command IDs =====
uint16_t SEN6xComponent::cmd_start_() const { return is_sen60_() ? 0x2152 : 0x0021; }
uint16_t SEN6xComponent::cmd_stop_() const { return is_sen60_() ? 0x3F86 : 0x0104; }
uint16_t SEN6xComponent::cmd_get_data_ready_() const { return is_sen60_() ? 0xE4B8 : 0x0202; }
uint16_t SEN6xComponent::cmd_read_measured_values_() const {
  switch (effective_variant_) {
    case Variant::SEN60: return 0xEC05;
    case Variant::SEN63C: return 0x0471;
    case Variant::SEN65: return 0x0446;
    case Variant::SEN66: return 0x0300;
    case Variant::SEN68: return 0x0467;
    default: return 0x0471;
  }
}
uint16_t SEN6xComponent::cmd_fan_cleaning_() const { return is_sen60_() ? 0x3730 : 0x5607; }
uint16_t SEN6xComponent::cmd_device_reset_() const { return is_sen60_() ? 0x3F8D : 0xD304; }
uint16_t SEN6xComponent::cmd_status_read_() const { return is_sen60_() ? 0xE00B : 0xD206; }

// ===== Setup / Config =====
void SEN6xComponent::setup() {
  this->set_i2c_address(0x6B);
  effective_variant_ = variant_config_;
  start_measurement();
}

void SEN6xComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "SEN6x sensor");
  ESP_LOGCONFIG(TAG, "  I2C Address: 0x%02X", this->get_i2c_address());
  ESP_LOGCONFIG(TAG, "  Variant: %d", static_cast<int>(effective_variant_));
}

// ===== Update loop =====
void SEN6xComponent::update() {
  if (!measurement_started_) return;
  bool ready = true;
  if (use_drdy_) {
    std::vector<uint16_t> dr;
    if (read_words_crc_(cmd_get_data_ready_(), dr, 20))
      ready = (dr.size() > 0);
  }
  if (!ready) return;
  std::vector<uint16_t> w;
  if (read_words_crc_(cmd_read_measured_values_(), w, 20)) {
    parse_measured_values_(w);
  }
  read_and_log_status_();
}

// ===== Actions =====
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
  if (send_cmd16(cmd_fan_cleaning_()))
    ESP_LOGI(TAG, "Fan cleaning triggered");
}
void SEN6xComponent::device_reset() {
  if (send_cmd16(cmd_device_reset_())) {
    measurement_started_ = false;
    ESP_LOGW(TAG, "Device reset sent");
  }
}
void SEN6xComponent::clear_status_sen6x() {
  std::vector<uint16_t> dummy;
  read_words_crc_(cmd_status_read_(), dummy, 20);  // ✅ korrigiert
}

// ===== Low-level I2C =====
bool SEN6xComponent::send_cmd16(uint16_t cmd) {
  uint8_t frame[2] = {uint8_t(cmd >> 8), uint8_t(cmd & 0xFF)};
  return this->write(frame, 2);
}
bool SEN6xComponent::write_words_with_crc_(uint16_t cmd, const std::vector<uint16_t> &words) {
  std::vector<uint8_t> buf;
  buf.push_back((cmd >> 8) & 0xFF);
  buf.push_back(cmd & 0xFF);
  for (auto w : words) {
    uint8_t msb = (w >> 8) & 0xFF, lsb = w & 0xFF;
    buf.push_back(msb);
    buf.push_back(lsb);
    buf.push_back(crc8_(msb, lsb));
  }
  return this->write(buf.data(), buf.size());
}
bool SEN6xComponent::read_bytes_(uint16_t cmd, uint8_t *buf, size_t len, uint32_t exec_delay_ms) {
  if (!send_cmd16(cmd)) return false;
  vTaskDelay(pdMS_TO_TICKS(exec_delay_ms));   // ✅ ESP-IDF delay
  return this->read(buf, len);
}
bool SEN6xComponent::read_words_crc_(uint16_t cmd, std::vector<uint16_t> &out, uint32_t exec_delay_ms) {
  uint8_t tmp[64];
  if (!read_bytes_(cmd, tmp, sizeof(tmp), exec_delay_ms)) return false;
  out.clear();
  for (size_t i = 0; i + 2 < sizeof(tmp); i += 3) {
    if (crc8_(tmp[i], tmp[i + 1]) != tmp[i + 2]) break;
    out.push_back((uint16_t(tmp[i]) << 8) | tmp[i + 1]);
  }
  return !out.empty();
}

// ===== Parser =====
bool SEN6xComponent::parse_sen60_values_bytes_(const std::vector<uint8_t> &) { return true; }
bool SEN6xComponent::parse_measured_values_(const std::vector<uint16_t> &w) {
  if (w.empty()) return false;
  ESP_LOGD(TAG, "Read %u words", (unsigned) w.size());
  return true;
}
void SEN6xComponent::read_and_log_status_() {
  std::vector<uint16_t> w;
  if (!read_words_crc_(cmd_status_read_(), w, 20)) return;
  if (w.size() >= 1)
    ESP_LOGD(TAG, "Status word: 0x%04X", w[0]);
}

}  // namespace sen6x
}  // namespace esphome
