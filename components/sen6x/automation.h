#pragma once
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "sen6x.h"

namespace esphome {
namespace sen6x {

// ======================
//  Actions for SEN6x
// ======================

// Start fan cleaning
class StartFanAction : public Action<> {
 public:
  explicit StartFanAction(SEN6xComponent *sen6x) : sen6x_(sen6x) {}
  void play(ActionContext &) override { this->sen6x_->start_fan_cleaning(); }

 protected:
  SEN6xComponent *sen6x_;
};

// Device reset
class DeviceResetAction : public Action<> {
 public:
  explicit DeviceResetAction(SEN6xComponent *sen6x) : sen6x_(sen6x) {}
  void play(ActionContext &) override { this->sen6x_->device_reset(); }

 protected:
  SEN6xComponent *sen6x_;
};

// Start measurement
class StartMeasurementAction : public Action<> {
 public:
  explicit StartMeasurementAction(SEN6xComponent *sen6x) : sen6x_(sen6x) {}
  void play(ActionContext &) override { this->sen6x_->start_measurement(); }

 protected:
  SEN6xComponent *sen6x_;
};

// Stop measurement
class StopMeasurementAction : public Action<> {
 public:
  explicit StopMeasurementAction(SEN6xComponent *sen6x) : sen6x_(sen6x) {}
  void play(ActionContext &) override { this->sen6x_->stop_measurement(); }

 protected:
  SEN6xComponent *sen6x_;
};

}  // namespace sen6x
}  // namespace esphome
