// Pure stepping logic for a unipolar stepper on a ULN2003, with no ESP-IDF, no
// GPIO and no I/O. StepperFocuser owns one of these and does nothing but turn
// its coil patterns into pin writes, so everything that can be wrong about the
// motion — phase order, clamping, counting, pacing — is decided here and tested
// on the build host. See specs/features/003-direct-stepper-backend.md.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace stepper {

// The four coil outputs for one phase, in ACTIVATION order: index 0 is the coil
// energised first, not the pin labelled IN1 on the board. StepperFocuser holds
// its pins in the same order, so coils[i] drives pins_[i] directly.
struct Coils {
  uint8_t level[4];
};

// Eight-phase half-step sequence for a 28BYJ-48, the canonical order that turns
// it smoothly. Columns are coils A, B, C, D in activation order. On this board
// that activation order is the silkscreen labels IN1, IN3, IN2, IN4 — the middle
// two swap — which is exactly what HBG3 passes to AccelStepper's HALF4WIRE
// constructor. Wired in silkscreen order instead, the motor only buzzes.
//
//   phase : A B C D
//     0     1 0 0 0
//     1     1 1 0 0
//     2     0 1 0 0
//     3     0 1 1 0
//     4     0 0 1 0
//     5     0 0 1 1
//     6     0 0 0 1
//     7     1 0 0 1
inline Coils HalfStepPhase(uint8_t phase) {
  static const uint8_t table[8][4] = {
      {1, 0, 0, 0}, {1, 1, 0, 0}, {0, 1, 0, 0}, {0, 1, 1, 0},
      {0, 0, 1, 0}, {0, 0, 1, 1}, {0, 0, 0, 1}, {1, 0, 0, 1},
  };
  const uint8_t p = phase & 7;
  return Coils{{table[p][0], table[p][1], table[p][2], table[p][3]}};
}

// All coils off. The gearbox holds position unpowered, so the motor is
// de-energised whenever it is not moving: no heat, and the tube turns by hand on
// a visual scope. The cost is that a hand-turn drifts the count with nothing to
// detect it, recovered by zeroing. A real EAF behaves the same way.
inline Coils CoilsOff() { return Coils{{0, 0, 0, 0}}; }

// Position, target and phase for one axis. Units are half-steps throughout, the
// same space the EAF reports, so nothing is scaled: position() upstream is this
// counter verbatim. Travel is clamped to [0, max_travel]; the protocol carries
// no limit and a real ASIAIR was seen accepting a move to 17000 against a device
// rated 5760, so an unclamped target would grind the mechanism into its end stop
// and lose steps silently.
class StepperCore {
 public:
  StepperCore(int32_t position, int32_t max_travel)
      : max_travel_(max_travel < 0 ? 0 : max_travel),
        position_(Clamp(position)),
        target_(position_) {}

  int32_t position() const { return position_; }
  int32_t target() const { return target_; }
  int32_t maxTravel() const { return max_travel_; }
  uint8_t phase() const { return phase_; }
  bool isMoving() const { return position_ != target_; }

  // moveTo: clamp the target into travel and head for it. Direction is implied
  // by target versus position, exactly as the EAF protocol does it.
  void setTarget(int32_t target) { target_ = Clamp(target); }

  // Halt: stop where we are. The position the host sent with a halt is stale and
  // ignored; target becomes the current position so motion ceases immediately.
  void halt() {
    target_ = position_;
    accum_us_ = 0;
  }

  // Redefine the current position without moving — the host's zero. There is no
  // encoder, so the counter is the only authority and this assigns it.
  void setPosition(int32_t position) {
    position_ = Clamp(position);
    target_ = position_;
    accum_us_ = 0;
  }

  // How many half-steps are due after `elapsed_us`, given `rate_sps` half-steps
  // per second. Leftover time accumulates so a slow rate still makes progress
  // between calls, and the count never exceeds the steps remaining to target.
  int32_t due(int64_t elapsed_us, int32_t rate_sps) {
    if (!isMoving() || rate_sps <= 0 || elapsed_us <= 0) {
      if (!isMoving()) accum_us_ = 0;
      return 0;
    }
    accum_us_ += elapsed_us;
    const int64_t interval_us = 1000000 / rate_sps;
    if (interval_us <= 0) return remaining();  // absurdly fast rate: all of it
    int64_t n = accum_us_ / interval_us;
    accum_us_ -= n * interval_us;
    const int32_t rem = remaining();
    if (n > rem) n = rem;
    return static_cast<int32_t>(n);
  }

  // Advance one half-step toward target and return the coils to energise.
  // Advancing the count raises the phase, retreating lowers it, so the sequence
  // walks the table forward or backward and the motor turns the matching way.
  // Caller must have confirmed isMoving(); a no-op call would still return a
  // powered phase, which the idle path must avoid.
  Coils stepOnce() {
    if (position_ < target_) {
      ++position_;
      phase_ = static_cast<uint8_t>((phase_ + 1) & 7);
    } else if (position_ > target_) {
      --position_;
      phase_ = static_cast<uint8_t>((phase_ + 7) & 7);  // -1 mod 8
    }
    return HalfStepPhase(phase_);
  }

 private:
  int32_t remaining() const {
    const int32_t d = target_ - position_;
    return d < 0 ? -d : d;
  }
  int32_t Clamp(int32_t v) const {
    if (v < 0) return 0;
    if (v > max_travel_) return max_travel_;
    return v;
  }

  int32_t max_travel_;
  int32_t position_;
  int32_t target_;
  uint8_t phase_ = 0;
  int64_t accum_us_ = 0;  // sub-interval time carried between due() calls
};

}  // namespace stepper
