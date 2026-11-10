// A focuser driven straight from four GPIO into a ULN2003 and a 28BYJ-48. The
// whole proxy on one board: no controller, no second microcontroller, no serial
// dialect. All motion logic lives in StepperCore; this class is the ESP-IDF
// glue — pin writes, NVS persistence, timing, and the optional zero button.
// See specs/features/003-direct-stepper-backend.md.
#pragma once

#include <stdint.h>

#include "backends/stepper_core.h"
#include "focuser.h"

// The caller passes the four pins by their board label; activation order is the
// silkscreen order IN1, IN2, IN3, IN4. invert swaps which table direction is "outward" when the motor
// is wired or geared the other way. zero_gpio < 0 disables the button.
struct StepperConfig {
  int in1_gpio;
  int in2_gpio;
  int in3_gpio;
  int in4_gpio;
  int32_t max_travel;    // clamp, and the value maxStep() reports
  int32_t rate_sps;      // half-steps per second, the rate ceiling
  int32_t start_position;  // used only when NVS holds nothing yet
  bool invert;
  int zero_gpio;         // active-low button to current-position-zero; < 0 = none
};

class StepperFocuser : public Focuser {
 public:
  explicit StepperFocuser(const StepperConfig& config);

  bool begin() override;
  int32_t position() override { return core_.position(); }
  bool moveTo(int32_t target) override;
  bool halt() override;
  bool setPosition(int32_t position) override;
  bool isMoving() override { return core_.isMoving(); }
  int32_t maxStep() override { return core_.maxTravel(); }

  // No probe on this backend. The persona substitutes its configured value, or a
  // temperature sensor wired to the proxy once that feature exists.
  int32_t temperature() override { return kUnsupported; }

  // Emit the due half-steps toward target, de-energise on arrival, persist once
  // on settle, and poll the zero button. Non-blocking: called from loop().
  void tick() override;

 private:
  void writePhase(const stepper::Coils& coils);
  void deEnergise();
  void persist();
  void pollZeroButton();

  StepperConfig config_;
  stepper::StepperCore core_;
  int pins_[4];          // activation order: {IN1, IN2, IN3, IN4}
  int64_t last_us_ = 0;
  int64_t last_step_us_ = 0;  // when the most recent half-step was written
  bool was_moving_ = false;
  bool button_down_ = false;
  int64_t button_changed_us_ = 0;
  bool nvs_ready_ = false;
};
