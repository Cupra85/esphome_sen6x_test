#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "sen6x.h"

namespace esphome {
namespace sen6x {

template<typename... Ts> class StartFanAction : public Action<Ts...> {
 public:
  explicit StartFanAction(SEN5XComponent *sen6x) : sen6x_(sen6x) {}
  void play(Ts... x) override { this->sen6x_->start_fan_cleaning(); }
 protected:
  SEN5XComponent *sen6x_;
};

template<typename... Ts> class ResetAction : public Action<Ts...> {
 public:
  explicit ResetAction(SEN5XComponent *sen6x) : sen6x_(sen6x) {}
  void play(Ts... x) override { this->sen6x_->device_reset(); }
 protected:
  SEN5XComponent *sen6x_;
};

template<typename... Ts> class ReadStatusAction : public Action<Ts...> {
 public:
  explicit ReadStatusAction(SEN5XComponent *sen6x) : sen6x_(sen6x) {}
  void play(Ts... x) override {
    uint32_t st=0; if (this->sen6x_->read_device_status(st)) { /* optional logging */ }
  }
 protected:
  SEN5XComponent *sen6x_;
};

template<typename... Ts> class StartMeasAction : public Action<Ts...> {
 public:
  explicit StartMeasAction(SEN5XComponent *sen6x) : sen6x_(sen6x) {}
  void play(Ts... x) override { this->sen6x_->start_continuous_measurement(); }
 protected:
  SEN5XComponent *sen6x_;
};

template<typename... Ts> class StopMeasAction : public Action<Ts...> {
 public:
  explicit StopMeasAction(SEN5XComponent *sen6x) : sen6x_(sen6x) {}
  void play(Ts... x) override { this->sen6x_->stop_measurement(); }
 protected:
  SEN5XComponent *sen6x_;
};

template<typename... Ts> class HeaterOnAction : public Action<Ts...> {
 public:
  explicit HeaterOnAction(SEN5XComponent *sen6x) : sen6x_(sen6x) {}
  void play(Ts... x) override { this->sen6x_->activate_sht_heater(); }
 protected:
  SEN5XComponent *sen6x_;
};

}  // namespace sen6x
}  // namespace esphome
