# ZWO-EAF-Proxy

> Traducción al español del README. El original está en [README.md](README.md).
> Guía de montaje y flasheo paso a paso en castellano: [GUIA-ES.md](GUIA-ES.md).

Hace que un enfocador que no es de ZWO aparezca ante un ASIAIR como un ZWO EAF.

Un microcontrolador se enumera en el bus USB del ASIAIR usando la identidad y el
protocolo del EAF, traduce los comandos de enfoque que llegan y mueve el
enfocador real por el otro lado. Tiene que ser un *dispositivo* USB, lo que
requiere silicio USB nativo, no un puente USB-serie.

**Se admiten dos microcontroladores en paralelo**, para que construyas con lo que
tengas. El ESP32-S3 está validado: un ASIAIR real lo ha aceptado y lo ha movido.
El RP2040 está planificado y sin probar; su PIO puede emular por software un
segundo puerto USB, lo que reduce un diseño de dos placas a una.

Solo el punto de entrada depende del chip. Los descriptores, el protocolo y la
interfaz del enfocador son comunes y sus pruebas compilan sin toolchain
embebido. Ver [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

Cómo se llega al enfocador depende de lo que este exponga. Un stepper sobre una
ULN2003 se mueve directamente desde la placa, y cualquier cosa con un puerto
serie accesible también necesita una sola placa. Un enfocador que solo habla por
USB necesita una segunda placa haciendo de host USB, porque el S3 tiene un único
periférico USB y el lado de la persona ya lo ha reclamado. Una placa basada en
RP2040 podría resolverlo cuando se pruebe.

```
                     sonda de temperatura DS18B20
                            │
                            v
  ASIAIR ──USB──>  XIAO ESP32-S3  ──GPIO──>  ULN2003 y un stepper
  botón cero ──>   (persona EAF)   ──UART──>  enfocador con puerto serie
                                              myFocuserPro2
                            │
                            └────UART──>  XIAO ESP32-S3  ──host USB──>  enfocador solo-USB
                                          (placa B)                     Pinefeat CEF
```

La sonda de temperatura no es adorno. El ASIAIR usa la temperatura del enfocador
para compensar el autofoco, y un objetivo Canon EF sobre un Pinefeat no tiene
sensor alguno, así que el proxy aporta uno real.

El botón de cero es opcional y pone la posición actual a cero, lo mismo que hace
el cero del propio ASIAIR. Importa donde el enfocador se puede mover a mano: un
tubo de enfoque interno no admite final de carrera, así que recuperar un origen
perdido significa llevarlo al extremo cero y declararlo.

## Estado

El protocolo del EAF está decodificado y un ESP32-S3 que lo emula es aceptado y
movido por un ASIAIR real. En el proyecto original no había ningún enfocador
conectado: el emulador movía uno simulado. **En este fork se ha añadido el
backend de stepper directo** (ULN2003 + 28BYJ-48).

- [x] Capturar un EAF real hablando con un ASIAIR (Cynthion, vía `zwoproxy capture`)
- [x] Decodificar el transporte: descriptores, formato de los informes HID, trama de las transferencias de control
- [x] Modelar el juego de comandos: posición, movimiento absoluto, parada, temperatura, identidad, serie
- [x] Emular el dispositivo en ESP32-S3 y lograr que el ASIAIR lo enumere
- [x] El ASIAIR lo mueve: firmware 3.8.2, modelo EAFN, serie, temperatura, movimientos, parada
- [x] Prueba de reorientación Pinefeat: la pseudo-parada funciona, 11-12 pasos sobre un margen de 353
- [x] Backend de stepper sobre ULN2003: el primer proxy completo en una sola placa — *código completo, probado en el PC; falta validarlo en hardware real*
- [ ] Backend de host myFocuserPro2
- [ ] Backend Pinefeat (necesita una segunda placa para host USB, o una placa basada en RP2040)
- [ ] Backend Gemini
- [ ] Sonda de temperatura en el proxy, para backends que no la tienen
- [ ] Prueba en lazo cerrado contra el autofoco del ASIAIR

El protocolo está en [docs/protocol/](docs/protocol/) y el emulador en
[specs/features/001-eaf-persona-emulator.md](specs/features/001-eaf-persona-emulator.md).
El backend de stepper está en
[specs/features/003-direct-stepper-backend.md](specs/features/003-direct-stepper-backend.md).

El backlash y los límites de pasos no están modelados en el protocolo, y ningún
campo de límite aparece en ninguna captura (el backend de stepper sí recorta el
recorrido por su cuenta).

Facedancer sobre una Cynthion presenta el mismo emulador desde el escritorio y es
útil contra macOS y Linux, pero un ASIAIR no enumera nada de lo que Facedancer
presenta, ni siquiera el dispositivo de ejemplo del propio Facedancer.

## Restricción de hardware

La pieza tiene que ser un dispositivo en el bus USB del ASIAIR, no un puente
USB-serie. Eso exige silicio USB nativo. Dentro de la familia ESP32 significa
**ESP32-S3** o S2; el ESP32 clásico y el C3 no pueden. La placa en uso es una
Seeed Studio XIAO ESP32S3, y es la que un ASIAIR real ha movido.

El S3 tiene un único periférico USB y la persona lo reclama, así que los backends
no pueden usar USB. Dos de los tres enfocadores objetivo son de metal sellado sin
UART donde pinchar, lo que convierte el transporte del backend en una cuestión de
hardware abierta, no un detalle.

Un RP2040 lo esquivaría: su PIO puede emular por software un segundo puerto USB,
reduciendo un diseño de dos placas a una. Nada se ha probado en él. Ver
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Diseño

Dos ejes independientes que se encuentran en una interfaz por tipo de
dispositivo. Añadir un enfocador es un backend, y ese es el único eje que crece
aquí. Un proxy de rueda de filtros va en su propio repositorio, reutilizando esta
separación. Todo el detalle en [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

```
            persona                 núcleo                  backend
ASIAIR --USB--> emulación EAF --> interfaz Focuser --> Pinefeat / Gemini / myFP2
                emulación EFW --> interfaz FilterWheel --> (otro repositorio)
```

La columna de backend es el plan. En el original solo estaba escrito el enfocador
simulado; este fork añade además el **stepper directo**.

La misma interfaz existe dos veces: en el firmware y en el módulo Python del host.
Eso permite ejercitar el protocolo sin grabar nada.

## Disposición del repositorio

- `firmware/` proyecto ESP-IDF, pila de dispositivo TinyUSB
- `host/` paquete Python gestionado con uv: decodificación de capturas, modelos
  del protocolo y la persona sobre Facedancer
- `docs/ARCHITECTURE.md` diseño y el conflicto host-vs-dispositivo del USB
- `docs/optical-trains.md` ajustes por tren óptico a registrar, y cómo medirlos
- `docs/protocol/` notas de ingeniería inversa, un fichero por hallazgo
- `captures/` capturas pcap, excluidas de git (grandes y con números de serie)

## Enfocadores objetivo

El stepper directo ya está escrito; el resto es el plan.

| Backend | Dónde | Transporte | Estado |
|---|---|---|---|
| Stepper sobre ULN2003 | firmware | cuatro GPIO, sin controlador | **implementado** (código completo, probado en PC) |
| Pinefeat CEF | firmware | USB CDC, caja sellada | objetivo principal: foco y diafragma de un objetivo Canon EF |
| Gemini EAF | firmware | caja sellada, necesita host USB | objetivo igual: el aparato que motivó el proyecto |
| myFocuserPro2 | host, luego firmware | pincho UART, placa casera | el último por utilidad |

Mover un stepper directamente no necesita controlador alguno, lo que lo hace la
vía más barata a un proxy completo en una sola placa y el primer backend a
escribir. Para compilarlo y cablearlo, ver [GUIA-ES.md](GUIA-ES.md).

## Herramientas del host

```bash
cd host && uv sync && uv run pytest
```

`zwoproxy capture` graba el tráfico USB a través de la Cynthion; `zwoproxy
persona` presenta el emulador desde el escritorio. Lee
[docs/protocol/capturing.md](docs/protocol/capturing.md) antes de capturar, en
especial el aviso sobre VBUS.

## Firmware

ESP-IDF v5.5, activado por shell para que no quede en el PATH global:

```bash
. /ruta/a/esp-idf/export.sh
cd firmware && idf.py set-target esp32s3 && idf.py build
idf.py -p PUERTO flash
```

El número de serie, la versión de firmware y el comportamiento del enfocador se
configuran en `idf.py menuconfig`, bajo "ZWO EAF persona"; ahí también se elige el
backend (simulado o stepper). Así, ningún número de serie de una unidad concreta
queda compilado desde este repositorio.

La conformidad de descriptores y protocolo corre en el PC, sin placa y sin
ESP-IDF, contra los mismos bytes capturados que usa el lado Python (incluye las
pruebas del backend de stepper):

```bash
firmware/test/host/run.sh
```

**Una vez grabado, la persona reclama el periférico USB nativo y el
USB-Serial-JTAG desaparece.** Para volver a grabar hay que mantener BOOT pulsado
mientras se reinicia. Es el mismo comportamiento que muestra el EAF real.

## Licencia

Apache-2.0. Ver [LICENSE](LICENSE) y [NOTICE](NOTICE).

## Nota legal

La ingeniería inversa de una interfaz con el único fin de la interoperabilidad
está permitida en la UE por la Directiva 2009/24/CE art. 6. Este proyecto
reimplementa una interfaz; no redistribuye firmware, binarios del SDK ni código
de ZWO.

ZWO, ASIAIR y EAF son marcas de ZWO. Este proyecto no está afiliado, respaldado
ni soportado por ellos.
