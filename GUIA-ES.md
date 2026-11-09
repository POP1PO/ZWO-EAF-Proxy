# Guía en castellano — proxy EAF con enfocador stepper (XIAO ESP32-S3)

Esta guía acompaña al proyecto **zwo-eaf-proxy** (de Gilles Morain, licencia
Apache-2.0) con el **backend de stepper directo añadido** para mover un motor
28BYJ-48 por una placa ULN2003. El resto del repositorio (la "persona" que hace
que un ASIAIR vea un EAF de ZWO, los documentos de protocolo y las pruebas) está
en inglés; esta guía cubre lo que necesitas para compilarlo, grabarlo y
cablearlo.

> Aviso honesto: la lógica del movimiento está probada en el PC, pero **el
> movimiento real del motor y la aceptación por tu ASI585 Air no están
> verificados**: eso solo se comprueba con tu hardware delante.

---

## 1. Qué necesitas

- Un NUC/PC con **ESP-IDF v5.5** instalado y **conexión a internet** (al compilar
  descarga solo el componente `esp_tinyusb`).
- Una placa **Seeed Studio XIAO ESP32-S3** (o cualquier ESP32-S3 con el USB
  nativo en el conector). Un Arduino Nano clásico **no sirve** para esto.
- Tu **ULN2003 + motor 28BYJ-48** (el de 5 V) y unos cables.

---

## 2. Instalar ESP-IDF v5.5 (una sola vez)

Sigue la guía oficial de Espressif para Windows:
https://docs.espressif.com/projects/esp-idf/es/v5.5/esp32s3/get-started/

Lo más cómodo es el **instalador de Windows de ESP-IDF v5.5**, que te deja un
acceso directo "ESP-IDF 5.5 CMD" o "ESP-IDF PowerShell". Abre esa consola: ya
tiene el entorno cargado (`export.bat`/`export.ps1` hecho por ti).

---

## 3. Compilar

En la consola de ESP-IDF, dentro de este proyecto:

```
cd firmware
idf.py set-target esp32s3
idf.py menuconfig
```

En `menuconfig`, entra en **"ZWO EAF persona"**:

1. **Focuser backend** → elige **"Direct stepper on ULN2003 (28BYJ-48)"**.
2. Entra en **"Direct stepper backend"** y ajusta:
   - **GPIO IN1..IN4**: por defecto 2, 3, 4, 5. En la XIAO S3 esos son los pads
     **D1, D2, D3, D4** (ver cableado abajo). Mételos por su etiqueta de la placa
     ULN2003; el firmware ya los reordena a la secuencia correcta.
   - **Travel (half-steps)**: el recorrido de tu enfocador en pasos. Por defecto
     60000. Ponlo al recorrido real para no estrellar el motor contra el final.
   - **Rate**: velocidad máxima en medios pasos/segundo (1000 por defecto, ~15
     rpm; no subas sin motivo).
   - **Invert direction**: actívalo solo si, al probar, el motor gira al revés.
   - **Zero button GPIO**: -1 (desactivado) salvo que pongas un botón de cero.

Guarda y sal de `menuconfig`. Luego:

```
idf.py build
```

---

## 4. Grabar la XIAO

Conecta la XIAO por USB-C. El puerto en Windows será un **COMx** (míralo en el
Administrador de dispositivos si dudas):

```
idf.py -p COM5 flash monitor
```

(Cambia COM5 por el tuyo. `monitor` abre el log por si quieres verlo; sal con
Ctrl+].)

> Importante: en cuanto grabas, la persona reclama el USB nativo y el puerto
> serie de depuración desaparece. Para **volver a grabar**, mantén pulsado el
> botón **BOOT** mientras conectas/reinicias la XIAO. Es el mismo comportamiento
> que un EAF real.

---

## 5. Cableado (XIAO S3 ↔ ULN2003 ↔ 28BYJ-48)

| XIAO (pad / GPIO) | ULN2003 |
|---|---|
| D1 / GPIO2 | IN1 |
| D2 / GPIO3 | IN2 |
| D3 / GPIO4 | IN3 |
| D4 / GPIO5 | IN4 |
| 5V | `+` (alimentación motor) |
| GND | `-` (GND) |

- El **motor 28BYJ-48** se enchufa en su conector blanco de la placa ULN2003.
- **GND común** entre la XIAO y la ULN2003 es obligatorio.
- Si el motor **solo zumba y no gira**, suele ser masa común o falta de 5 V, no
  el orden de fases (el firmware ya lo corrige).
- Si gira **en sentido contrario** al que quieres, activa "Invert direction" en
  `menuconfig` o cambia el conector del motor.

---

## 6. Cómo probar (en orden, de más barato a definitivo)

1. **Sin placa, en el NUC**: ejecuta las pruebas de lógica
   `firmware/test/host/run.sh` (necesita un compilador C++). Valida protocolo y
   stepping. No prueba la cámara.
2. **XIAO en un PC Windows**: instala el **driver ASCOM del EAF de ZWO**, abre su
   panel y apúntalo a la XIAO. Si conecta y lee datos, has pasado la lógica de
   reconocimiento de ZWO. Es el test más fuerte sin el ASIAIR.
3. **Definitivo, con la ASI585 Air**: enchufa la XIAO a un puerto USB de la
   cámara y mira si la app la lista como EAF. Solo esto lo confirma.

---

## 7. Cosas que debes saber de este backend

- **Recorrido limitado**: los objetivos se recortan a [0, travel]. El protocolo
  no lleva tope; esto protege el mecanismo. Ajústalo a tu enfocador.
- **En reposo desenergiza** las bobinas (ni calor ni consumo; el tubo se puede
  girar a mano, lo que descuadra la cuenta → recupera poniendo a cero).
- **La posición se guarda** en memoria no volátil (NVS) y sobrevive a apagones;
  solo se pierde si mueves el enfocador a mano con la placa apagada.
- **El toggle "Reverse" del ASIAIR** se almacena pero **no se aplica al motor**
  en esta versión. La dirección se fija con "Invert direction" o por el conector.
- **Temperatura**: este backend no la mide; la persona reporta el valor
  configurado como sustituto (una sonda DS18B20 es una función futura).

---

## 8. Ficheros añadidos por el backend

- `firmware/main/include/backends/stepper_core.h` — lógica pura (tabla de medio
  paso, clamp, conteo, temporización), probada en el PC.
- `firmware/main/include/backends/stepper_focuser.h` y
  `firmware/main/backends/stepper_focuser.cpp` — la cola ESP-IDF (GPIO, NVS,
  reloj, botón de cero).
- `firmware/test/host/test_stepper.cpp` — pruebas de la lógica anterior.
- Cambios en `Kconfig.projbuild`, `CMakeLists.txt`, `main.cpp`, `run.sh` y
  `backends/README.md` para seleccionar y compilar el backend.

---

## 9. Subirlo a tu propio GitHub (opcional)

1. En GitHub, haz un **fork** de `gmorain/zwo-eaf-proxy`.
2. En este proyecto:
   ```
   git remote set-url origin https://github.com/TU_USUARIO/zwo-eaf-proxy
   git add -A
   git commit -m "Backend de stepper directo (ULN2003 / 28BYJ-48)"
   git push origin HEAD
   ```
   (El commit es tuyo y va a tu nombre; no lleva ninguna línea de atribución
   añadida.)
