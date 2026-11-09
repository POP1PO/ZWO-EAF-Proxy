// Gemini focuser over USB host — SCAFFOLD, NOT IMPLEMENTED.
//
// Target: ESP32-P4, which has two USB-OTG controllers. One runs the EAF persona
// as a USB device (to the ASIAIR); this backend is meant to run on the other as
// a USB host and drive a Gemini focuser over USB CDC. See docs/esp32p4-port.md.
//
// Today this is a stub: it implements the Focuser interface, reports everything
// as unsupported, and begin() returns false so it is obvious nothing is wired.
// The two missing pieces are the USB-host CDC transport and the Gemini's command
// protocol, which is not yet known. Help wanted.
#pragma once

#include <stdint.h>

#include "focuser.h"

class GeminiUsbhFocuser : public Focuser {
 public:
  GeminiUsbhFocuser() = default;

  bool begin() override;
  int32_t position() override;
  bool moveTo(int32_t target) override;
  bool halt() override;
  bool setPosition(int32_t position) override;
  bool isMoving() override;
  int32_t maxStep() override;
  int32_t temperature() override;
  void tick() override;
};
