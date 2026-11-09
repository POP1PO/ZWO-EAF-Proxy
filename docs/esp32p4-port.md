# ESP32-P4 port — one board, two USB (Gemini via the ASIAIR)

> **STATUS: EXPERIMENTAL SCAFFOLD. This does not work yet.** It compiles and the
> EAF persona runs, but the Gemini backend is a non-functional stub. Two hard
> pieces are missing (a USB host stack and the Gemini's protocol). **Help wanted:**
> anyone with an ESP32-P4 board, a Gemini focuser and an ASIAIR is invited to
> pick this up — see "How to help" below.

## Why the P4

The whole project rests on a conflict. The part that talks to the ASIAIR must be
a USB **device**. A USB-only focuser like the Gemini is itself a USB **device**,
so reaching it needs a USB **host**. On the ESP32-S3 there is a single
general-purpose USB-OTG peripheral, and the persona has already claimed it as a
device, so an S3 cannot also host the Gemini — it would need a second board.

The **ESP32-P4** removes that wall. It has two independent USB-OTG controllers,
one High-Speed and one Full-Speed (plus a separate USB-Serial-JTAG), each able to
run as host or device. That makes the whole proxy possible on **one board**:

```
                 ESP32-P4
  ASIAIR ──USB──▶ OTG #1 as DEVICE  (EAF persona, as on the S3)
                      │
                      │  (in-chip)
                      ▼
                  OTG #2 as HOST    ──USB──▶  Gemini focuser
```

No second microcontroller, no UART bridge between them. This is the cleaner
sibling of the author's RP2040 idea (PIO bit-banged second USB), which is also
unwritten.

## What this scaffold contains

- `firmware/main/backends/gemini_usbh_focuser.{h,cpp}` — a `Focuser`
  implementation that is a **stub**. It implements the interface, logs that it is
  not implemented, and reports every value as unsupported. It compiles with no
  extra components so the rest of the firmware still builds. The real USB-host
  and protocol code goes here.
- `firmware/main/Kconfig.projbuild` — a backend choice `EAF_BACKEND_GEMINI_USBH`,
  visible only when the target is the ESP32-P4.
- `firmware/main/CMakeLists.txt` — compiles the stub only on the P4 target, so
  the S3 build is untouched.
- `firmware/main/main.cpp` — wires the stub in under
  `#if CONFIG_EAF_BACKEND_GEMINI_USBH`.

Nothing here changes the S3 path. The stepper and simulated backends build and
behave exactly as before.

## What is NOT done (the real work)

Three pieces, roughly in order:

1. **Persona on the P4's device OTG.** The persona (TinyUSB device, the EAF
   emulation) is validated on the S3's OTG. On the P4 it must run on one of the
   two OTG controllers configured as a device. Expected to be mostly a target and
   PHY-selection change, but unverified on P4 silicon.
2. **USB host for the Gemini, on the other OTG.** The Gemini is a USB CDC (serial)
   device. Use ESP-IDF's USB Host stack with the `cdc_acm_host` managed component
   on the second OTG, enumerate the Gemini, open its CDC interface, and exchange
   its command bytes. The two OTG controllers must run concurrently in opposite
   roles; confirm that on real P4 hardware, as the Espressif overview does not
   spell out concurrency.
3. **The Gemini's protocol — the real unknown.** We do not yet know what the
   Gemini speaks. It ships an ASCOM driver and works with NINA, and clones in its
   price bracket often use a **Moonlite-compatible ASCII** protocol (`:cmd#`
   framing), but this is **unconfirmed**. This must be learned before the host
   backend can say anything useful. Two ways to learn it:
   - Run `host/tools/probe_ascom.py` against the Gemini on a Windows PC to confirm
     it responds through ASCOM and read its real limits (max step, temperature).
   - Capture the Gemini's USB traffic (a USB analyzer, or usbmon/Wireshark on
     Linux) while NINA or its ASCOM driver drives it, and decode the ASCII
     command set. Record findings under `docs/protocol/`, one file per finding,
     as the EAF protocol notes were.

Then fill `gemini_usbh_focuser.cpp`: map `move_to`/`position`/`halt`/`is_moving`/
`max_step`/`temperature` onto the Gemini's real commands.

## How to test / what you need

- An **ESP32-P4** board with both USB ports broken out (e.g. the Espressif
  ESP32-P4-Function-EV-Board).
- A **Gemini** focuser (the ~65 EUR ZWO EAF clone), ideally on a scope so moves
  are visible.
- An **ASIAIR** (or an ASIAIR-class "Air" camera) to be the USB host for the
  persona side.
- ESP-IDF v5.4+ with ESP32-P4 support.

Build and flash for the P4, select the Gemini USB-host backend in
`idf.py menuconfig` → "ZWO EAF persona" → "Focuser backend", wire OTG #1 to the
ASIAIR and OTG #2 to the Gemini. Today the persona should enumerate on the ASIAIR
as an EAF while the Gemini stub does nothing — that is the starting point.

## How to help

If you have the hardware, the highest-value first step is **item 3**: pin down the
Gemini's protocol and write it up under `docs/protocol/`. Everything downstream is
mechanical once the command set is known. Open an issue or PR with what you find.

The host conformance tests (`firmware/test/host/run.sh`) run with no board and are
the safety net for anything you add to the shared logic.

## References

- ESP USB peripherals per chip (S2 / S3 / P4 controller counts):
  https://docs.espressif.com/projects/esp-iot-solution/en/latest/usb/usb_overview/usb_overview.html
- ESP32-P4 USB Host:
  https://docs.espressif.com/projects/esp-usb/en/latest/esp32p4/usb_host.html
- `cdc_acm_host` (USB Host CDC-ACM) component:
  https://components.espressif.com/components/espressif/usb_host_cdc_acm
- The USB host-vs-device conflict, in this repo: `docs/ARCHITECTURE.md`.
