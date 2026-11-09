# Backends

One file per real focuser. Each implements `Focuser` (firmware/main/include/focuser.h)
and nothing else. A backend never knows which persona is upstream.

To add one: implement `Focuser` (directly, or via `SerialFocuser` for a UART
backend), implement the required methods, and report `kUnsupported` for anything
the hardware genuinely cannot answer.

## Implemented

- **Direct stepper** (`stepper_focuser.*`) — a 28BYJ-48 on a ULN2003, driven
  straight from four GPIO. The whole proxy on one board. All motion logic is in
  the dependency-free `stepper_core.h`, host-tested in
  `firmware/test/host/test_stepper.cpp`; `stepper_focuser.cpp` is only the GPIO,
  NVS, timing and zero-button glue. Select it under `idf.py menuconfig` →
  "ZWO EAF persona" → "Focuser backend". See
  `specs/features/003-direct-stepper-backend.md`.

## Planned, in the order they are worth doing:

1. **myFocuserPro2** — your own board, protocol is open and documented, UART is
   tappable. Start here: it validates the whole chain with no unknowns on the
   backend side.
2. **Optec Gemini** — RS-232 native, needs a level shifter, ASCII protocol.
3. **Pinefeat CEF** — USB CDC only. Blocked on the USB conflict in
   docs/ARCHITECTURE.md.
