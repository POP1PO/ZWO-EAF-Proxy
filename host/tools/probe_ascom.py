"""Prueba un enfocador ASCOM (p. ej. el Gemini) desde el proxy, en el PC.

Qué hace: conecta con tu enfocador por su driver ASCOM, te enseña su estado
(posición, recorrido máximo, temperatura) y, si lo confirmas, lo mueve un poco y
lo devuelve a su sitio. Así compruebas que el backend del proxy maneja tu
enfocador real, sin XIAO, sin ASIAIR y sin Cynthion.

Requisitos: Windows, ASCOM Platform y pywin32 (`pip install pywin32`).
Cierra N.I.N.A. u otro programa que tenga el enfocador conectado antes de
ejecutarlo, o habrá conflicto.

Uso:
    python probe_ascom.py                 # te sale el selector ASCOM
    python probe_ascom.py --progid X      # usa ese driver directamente
    python probe_ascom.py --delta 50      # mueve 50 pasos en vez de 100
    python probe_ascom.py --solo-leer     # solo lee, no mueve nada
"""

from __future__ import annotations

import argparse
import pathlib
import sys
import time

# Permite ejecutar el script sin instalar el paquete: añade host/src al path.
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "src"))

from zwoproxy.backends.ascom import AscomFocuser  # noqa: E402
from zwoproxy.focuser import UNSUPPORTED  # noqa: E402


def fmt_temp(centi: int) -> str:
    return "no disponible" if centi == UNSUPPORTED else f"{centi / 100:.2f} °C"


def fmt_max(limit: int) -> str:
    return "desconocido" if limit == UNSUPPORTED else str(limit)


def wait_until_stopped(foc: AscomFocuser, timeout_s: float = 60.0) -> None:
    start = time.monotonic()
    while foc.is_moving():
        print(f"    moviendo... posición {foc.position()}")
        time.sleep(0.5)
        if time.monotonic() - start > timeout_s:
            print("    (tiempo de espera agotado; intento parar)")
            foc.halt()
            return


def main() -> int:
    parser = argparse.ArgumentParser(description="Prueba de enfocador ASCOM")
    parser.add_argument("--progid", default=None, help="ProgID del driver ASCOM")
    parser.add_argument("--delta", type=int, default=100, help="pasos a mover en la prueba")
    parser.add_argument("--solo-leer", action="store_true", help="no mover, solo leer")
    args = parser.parse_args()

    foc = AscomFocuser(args.progid)
    print("Conectando con el enfocador ASCOM...")
    try:
        if not foc.begin():
            print("No se pudo conectar (¿cancelaste el selector, o el driver falló?).")
            return 1
    except RuntimeError as exc:
        print(exc)
        return 1

    if getattr(foc, "_progid", None):
        print(f"Driver: {foc._progid}")

    start_pos = foc.position()
    limit = foc.max_step()
    print("\nEstado del enfocador:")
    print(f"  Posición actual : {start_pos}")
    print(f"  Recorrido máx.  : {fmt_max(limit)}")
    print(f"  Temperatura     : {fmt_temp(foc.temperature())}")
    print(f"  En movimiento   : {'sí' if foc.is_moving() else 'no'}")

    if args.solo_leer:
        print("\nModo solo-lectura: no se mueve nada. Listo.")
        foc.close()
        return 0

    # Movimiento de prueba, pequeño y reversible, con confirmación.
    delta = abs(args.delta)
    up = start_pos + delta
    if limit != UNSUPPORTED and up > limit:
        up = start_pos - delta  # cerca del tope: mueve hacia dentro
    if up < 0:
        print("\nNo hay margen seguro para mover desde aquí. Usa --solo-leer o cambia la posición.")
        foc.close()
        return 1

    print(
        f"\nVoy a mover de {start_pos} a {up} (±{delta} pasos) y volver a {start_pos}."
    )
    print("Asegúrate de que el enfocador puede moverse sin chocar.")
    if input("¿Continúo? (s/N): ").strip().lower() != "s":
        print("Cancelado. No se ha movido nada.")
        foc.close()
        return 0

    print(f"\n-> Moviendo a {up}...")
    if not foc.move_to(up):
        print("El driver rechazó el movimiento.")
        foc.close()
        return 1
    wait_until_stopped(foc)
    print(f"   Posición tras el movimiento: {foc.position()}")

    print(f"\n-> Volviendo a {start_pos}...")
    foc.move_to(start_pos)
    wait_until_stopped(foc)
    print(f"   Posición final: {foc.position()}")

    ok = abs(foc.position() - start_pos) <= 2
    print("\n" + ("Prueba correcta: el enfocador se movió y volvió a su sitio."
                   if ok else
                   "Atención: no ha vuelto exactamente a la posición inicial; revísalo."))
    foc.close()
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
