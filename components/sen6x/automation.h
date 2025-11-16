#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "sen6x.h"

namespace esphome {
namespace sen6x {

// --- Fan Cleaning ---
template<typename... Ts> class StartFanAction : public Action<Ts...> {
 public:
  explicit StartFanAction(SEN5XComponent *sen6x) : sen6x_(sen6x) {}
  void play(Ts... x) override { this->sen6x_->start_fan_cleaning(); }
 protected:
  SEN5XComponent *sen6x_;
};

// --- Stop Measurement ---
template<typename... Ts> class StopMeasurementAction : public Action<Ts...> {
 public:
  explicit StopMeasurementAction(SEN5XComponent *sen6x) : sen6x_(sen6x) {}
  void play(Ts... x) override { this->sen6x_->stop_measurement(); }
 protected:
  SEN5XComponent *sen6x_;
};

// --- Start Measurement ---
template<typename... Ts> class StartMeasurementAction : public Action<Ts...> {
 public:
  explicit StartMeasurementAction(SEN5XComponent *sen6x) : sen6x_(sen6x) {}
  void play(Ts... x) override { this->sen6x_->start_measurement(); }
 protected:
  SEN5XComponent *sen6x_;
};

}  // namespace sen6x
}  // namespace esphome
