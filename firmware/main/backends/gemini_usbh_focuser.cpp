// Gemini focuser over USB host — SCAFFOLD, NOT IMPLEMENTED. See the header and
// docs/esp32p4-port.md. Every method is a stub so the firmware builds and the
// persona runs; none of this drives a focuser yet.
//
// To make it real (contributors welcome):
//   1. Bring up the USB Host stack on the P4's second OTG controller
//      (usb_host_install) with the `cdc_acm_host` managed component, while the
//      persona holds the first OTG as a device. Confirm both roles run at once
//      on real P4 silicon.
//   2. Enumerate the Gemini's CDC interface, set its line coding, and open a
//      read/write data pipe.
//   3. Replace the placeholders below with the Gemini's real commands. The
//      protocol is unconfirmed: many clones in this bracket use Moonlite-style
//      ASCII (":cmd#"), but this must be captured and written up under
//      docs/protocol/ before it can be trusted. host/tools/probe_ascom.py is a
//      first way to probe it on a PC.

#include "backends/gemini_usbh_focuser.h"

#include "esp_log.h"

namespace {
const char* kTag = "eaf.gemini";
bool warned = false;

void warn_once() {
  if (!warned) {
    ESP_LOGW(kTag,
             "Gemini USB-host backend is a stub: not implemented. "
             "See docs/esp32p4-port.md.");
    warned = true;
  }
}
}  // namespace

bool GeminiUsbhFocuser::begin() {
  warn_once();
  // Return false on purpose: nothing is wired, so the persona should present but
  // report no working focuser rather than pretend to move one.
  return false;
}

int32_t GeminiUsbhFocuser::position() { return 0; }

bool GeminiUsbhFocuser::moveTo(int32_t target) {
  (void)target;
  warn_once();
  return false;
}

bool GeminiUsbhFocuser::halt() {
  warn_once();
  return false;
}

bool GeminiUsbhFocuser::setPosition(int32_t position) {
  (void)position;
  return false;
}

bool GeminiUsbhFocuser::isMoving() { return false; }

int32_t GeminiUsbhFocuser::maxStep() { return 0; }

int32_t GeminiUsbhFocuser::temperature() { return kUnsupported; }

void GeminiUsbhFocuser::tick() {}
