// The stepping logic: phase table, clamping, counting and pacing. Runs on the
// build host with no board. Everything the motor could get wrong — turning the
// wrong way, overrunning travel, miscounting, ignoring the rate — is decided in
// StepperCore and checked here. The GPIO and NVS glue in stepper_focuser.cpp
// has no logic of its own to test.
#include <cstdio>

#include "backends/stepper_core.h"

namespace {
int failures = 0;

void check(const char* name, bool ok) {
  printf(ok ? "  ok    %s\n" : "  FAIL  %s\n", name);
  if (!ok) ++failures;
}

bool samePhase(const stepper::Coils& c, uint8_t a, uint8_t b, uint8_t cc, uint8_t d) {
  return c.level[0] == a && c.level[1] == b && c.level[2] == cc && c.level[3] == d;
}

// Drive a move to completion the way tick() does: due() then that many
// stepOnce(). Returns the number of half-steps emitted.
int runMove(stepper::StepperCore& core, int32_t target, int32_t rate) {
  core.setTarget(target);
  int emitted = 0;
  for (int guard = 0; core.isMoving() && guard < 1000000; ++guard) {
    const int32_t n = core.due(1000, rate);  // one 1 ms tick
    for (int32_t i = 0; i < n; ++i) {
      core.stepOnce();
      ++emitted;
    }
  }
  return emitted;
}
}  // namespace

int main() {
  printf("stepper core\n");

  {  // The half-step table is the canonical 28BYJ-48 sequence, in order.
    using stepper::HalfStepPhase;
    check("phase 0 = A", samePhase(HalfStepPhase(0), 1, 0, 0, 0));
    check("phase 1 = AB", samePhase(HalfStepPhase(1), 1, 1, 0, 0));
    check("phase 2 = B", samePhase(HalfStepPhase(2), 0, 1, 0, 0));
    check("phase 3 = BC", samePhase(HalfStepPhase(3), 0, 1, 1, 0));
    check("phase 4 = C", samePhase(HalfStepPhase(4), 0, 0, 1, 0));
    check("phase 5 = CD", samePhase(HalfStepPhase(5), 0, 0, 1, 1));
    check("phase 6 = D", samePhase(HalfStepPhase(6), 0, 0, 0, 1));
    check("phase 7 = DA", samePhase(HalfStepPhase(7), 1, 0, 0, 1));
    check("phase wraps mod 8", samePhase(HalfStepPhase(8), 1, 0, 0, 0));

    // No phase releases every coil: the off state is explicit, never a table row.
    bool any_blank = false;
    for (uint8_t p = 0; p < 8; ++p) {
      const stepper::Coils c = HalfStepPhase(p);
      if (!(c.level[0] || c.level[1] || c.level[2] || c.level[3])) any_blank = true;
    }
    check("no all-off phase in the table", !any_blank);
    const stepper::Coils off = stepper::CoilsOff();
    check("CoilsOff releases all four", samePhase(off, 0, 0, 0, 0));
  }

  {  // Travel is clamped at construction and on every target.
    stepper::StepperCore a(-50, 1000);
    check("negative start clamps to 0", a.position() == 0);
    stepper::StepperCore b(9999, 1000);
    check("start past travel clamps to max", b.position() == 1000);

    stepper::StepperCore c(0, 1000);
    c.setTarget(5000);
    check("target past travel clamps to max", c.target() == 1000);
    c.setTarget(-5);
    check("negative target clamps to 0", c.target() == 0);
  }

  {  // A forward move lands exactly on target, emits one half-step per unit, and
     // advances the phase the same way.
    stepper::StepperCore core(0, 10000);
    const int emitted = runMove(core, 5, 1000);
    check("forward move emits target steps", emitted == 5);
    check("forward move lands on target", core.position() == 5);
    check("stopped on arrival", !core.isMoving());
    check("phase advanced by the step count", core.phase() == (5 & 7));
  }

  {  // A backward move counts down and walks the phase backward.
    stepper::StepperCore core(10, 10000);
    const int emitted = runMove(core, 7, 1000);
    check("backward move emits the gap", emitted == 3);
    check("backward move lands on target", core.position() == 7);
    // started at phase 0, three steps back: 0 -> 7 -> 6 -> 5.
    check("phase retreated by the step count", core.phase() == 5);
  }

  {  // Eight forward half-steps return the phase to where it began.
    stepper::StepperCore core(0, 10000);
    runMove(core, 8, 1000);
    check("a full 8-phase cycle returns to phase 0", core.phase() == 0);
  }

  {  // due() paces to the rate, carries leftover time, and never exceeds the gap.
    stepper::StepperCore core(0, 1000);
    core.setTarget(10);  // isMoving, 10 remaining, nothing stepped yet
    check("no time elapsed yields no steps", core.due(0, 1000) == 0);
    check("one interval yields one step", core.due(1000, 1000) == 1);
    check("half an interval yields none", core.due(500, 1000) == 0);
    check("the other half completes one", core.due(500, 1000) == 1);
    check("a huge elapsed is capped at the remaining gap", core.due(100000, 1000) == 10);

    stepper::StepperCore idle(5, 1000);  // position == target
    check("an idle axis is never due", idle.due(100000, 1000) == 0);
  }

  {  // setPosition redefines the count without moving, and clamps.
    stepper::StepperCore core(500, 1000);
    core.setPosition(0);
    check("setPosition redefines the count", core.position() == 0);
    check("setPosition clears motion", !core.isMoving());
    core.setPosition(5000);
    check("setPosition clamps into travel", core.position() == 1000);
  }

  {  // halt stops where it is; the position it is told is irrelevant here.
    stepper::StepperCore core(0, 10000);
    core.setTarget(1000);
    core.due(1000, 1000);
    core.stepOnce();  // now at 1, still heading for 1000
    core.halt();
    check("halt stops at the current position", core.target() == core.position());
    check("halt leaves it not moving", !core.isMoving());
  }

  printf(failures ? "\n%d failed\n" : "\nall passed\n", failures);
  return failures ? 1 : 0;
}
