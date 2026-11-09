"""ASCOM focuser backend (Windows host only).

Drives any ASCOM-compatible focuser through its installed driver, over COM. This
is how the Gemini — which ships an ASCOM driver and works with NINA — can be
driven from the host today, with no microcontroller in the path. It is a host
bench backend, not a firmware one: ASCOM is a Windows COM interface and cannot be
ported to the ESP32-S3. Use it to prove the Focuser contract against real
hardware and to learn a focuser's real limits before writing its firmware
backend.

Requires `pywin32` and the ASCOM Platform. Implements the `Focuser` protocol in
`zwoproxy.focuser`.
"""

from __future__ import annotations

from ..focuser import UNSUPPORTED


class AscomFocuser:
    """Implements `Focuser` over an ASCOM driver reached via COM.

    `progid` is the driver's ProgID (e.g. the Gemini's ASCOM focuser id). Pass
    None to raise the ASCOM Chooser at `begin()` so the user can pick it.
    """

    def __init__(self, progid: str | None = None) -> None:
        self._progid = progid
        self._dev = None
        self._absolute = True

    def _choose(self) -> str | None:
        import win32com.client

        chooser = win32com.client.Dispatch("ASCOM.Utilities.Chooser")
        chooser.DeviceType = "Focuser"
        picked = chooser.Choose(self._progid or "")
        return picked or None

    def begin(self) -> bool:
        try:
            import win32com.client
        except ImportError as exc:  # pragma: no cover - environment dependent
            raise RuntimeError(
                "pywin32 is required for the ASCOM backend: pip install pywin32"
            ) from exc

        if self._progid is None:
            self._progid = self._choose()
            if not self._progid:
                return False  # user cancelled the Chooser

        self._dev = win32com.client.Dispatch(self._progid)
        try:
            self._dev.Connected = True
        except Exception:
            return False
        if not bool(self._dev.Connected):
            return False
        try:
            self._absolute = bool(self._dev.Absolute)
        except Exception:
            self._absolute = True
        return True

    def position(self) -> int:
        # Position is only meaningful on an absolute focuser; a relative one has
        # no notion of it, so report 0 and let the caller drive by deltas.
        if not self._absolute:
            return 0
        return int(self._dev.Position)

    def move_to(self, target: int) -> bool:
        if target < 0:
            return False
        try:
            limit = self.max_step()
            if limit != UNSUPPORTED and target > limit:
                return False
            if self._absolute:
                self._dev.Move(int(target))
            else:
                self._dev.Move(int(target) - self.position())
            return True
        except Exception:
            return False

    def halt(self) -> bool:
        try:
            self._dev.Halt()
            return True
        except Exception:
            # Some drivers do not implement Halt. The caller must treat a False
            # here as "cannot stop on command", which matters for safety.
            return False

    def set_position(self, position: int) -> bool:
        # ASCOM has no "redefine current position" (no sync/zero in IFocuserV3),
        # so the EAF's zero cannot be forwarded to an ASCOM focuser.
        return False

    def is_moving(self) -> bool:
        try:
            return bool(self._dev.IsMoving)
        except Exception:
            return False

    def max_step(self) -> int:
        try:
            return int(self._dev.MaxStep)
        except Exception:
            return UNSUPPORTED

    def temperature(self) -> int:
        try:
            return round(float(self._dev.Temperature) * 100)
        except Exception:
            # TemperatureNotImplemented, or no probe: let the persona substitute.
            return UNSUPPORTED

    def tick(self) -> None:
        """ASCOM drivers run their own motion; nothing to pump here."""

    def close(self) -> None:
        if self._dev is not None:
            try:
                self._dev.Connected = False
            except Exception:
                pass
