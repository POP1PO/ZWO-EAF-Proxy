# Port a ESP32-P4 — una placa, dos USB (el Gemini a través del ASIAIR)

> Traducción al español. El original está en [esp32p4-port.md](esp32p4-port.md).

> **ESTADO: ANDAMIAJE EXPERIMENTAL. Esto todavía NO funciona.** Compila y la
> persona EAF arranca, pero el backend del Gemini es un esqueleto no funcional.
> Faltan las dos piezas difíciles (la pila de host USB y el protocolo del
> Gemini). **Se buscan colaboradores:** cualquiera con una placa ESP32-P4, un
> enfocador Gemini y un ASIAIR puede continuarlo — ver "Cómo ayudar".

## Por qué la P4

Todo el proyecto choca con un problema. La pieza que habla con el ASIAIR tiene
que ser un *dispositivo* USB. Un enfocador que solo habla por USB, como el
Gemini, es a su vez un *dispositivo* USB, así que para alcanzarlo hace falta un
*host* USB. En la ESP32-S3 hay un único USB-OTG de propósito general, y la
persona ya lo ha reclamado como dispositivo, así que una S3 no puede además hacer
de host del Gemini — necesitaría una segunda placa.

La **ESP32-P4** quita esa pared. Tiene dos controladores USB-OTG independientes,
uno de alta velocidad y otro de velocidad completa (más un USB-Serial-JTAG
aparte), y cada uno puede ser host o dispositivo. Eso hace posible todo el proxy
en **una sola placa**:

```
                 ESP32-P4
  ASIAIR ──USB──▶ OTG #1 como DISPOSITIVO  (persona EAF, igual que en la S3)
                      │
                      │  (dentro del chip)
                      ▼
                  OTG #2 como HOST          ──USB──▶  enfocador Gemini
```

Sin segundo microcontrolador ni puente UART entre ellos. Es la hermana más limpia
de la idea del RP2040 del autor (segundo USB emulado por software), que tampoco
está escrita.

## Qué contiene este andamiaje

- `firmware/main/backends/gemini_usbh_focuser.{h,cpp}` — una implementación de
  `Focuser` que es un **esqueleto**. Cumple la interfaz, avisa de que no está
  implementada y reporta todo como no soportado. Compila sin componentes extra
  para que el resto del firmware siga construyendo. Aquí va el código real de
  host USB y de protocolo.
- `firmware/main/Kconfig.projbuild` — una opción de backend
  `EAF_BACKEND_GEMINI_USBH`, visible solo cuando el objetivo es la ESP32-P4.
- `firmware/main/CMakeLists.txt` — compila el esqueleto solo en el objetivo P4,
  así el build de la S3 no se toca.
- `firmware/main/main.cpp` — lo cablea bajo `#if CONFIG_EAF_BACKEND_GEMINI_USBH`.

Nada de esto cambia el camino de la S3. El stepper y el simulado compilan y se
comportan igual que antes.

## Qué NO está hecho (el trabajo de verdad)

Tres piezas, más o menos en orden:

1. **La persona en el OTG de dispositivo de la P4.** La persona (dispositivo
   TinyUSB, la emulación del EAF) está validada en el OTG de la S3. En la P4 tiene
   que correr en uno de los dos OTG configurado como dispositivo. Se espera que
   sea sobre todo un cambio de objetivo y de selección de PHY, pero sin verificar
   en silicio P4.
2. **Host USB para el Gemini, en el otro OTG.** El Gemini es un dispositivo USB
   CDC (serie). Usar la pila de Host USB de ESP-IDF con el componente
   `cdc_acm_host` en el segundo OTG, enumerar el Gemini, abrir su interfaz CDC e
   intercambiar sus bytes de comando. Los dos OTG tienen que funcionar a la vez en
   roles opuestos; confírmalo en hardware P4 real, porque la documentación de
   Espressif no detalla la concurrencia.
3. **El protocolo del Gemini — la incógnita real.** Todavía no sabemos qué habla
   el Gemini. Trae driver ASCOM y funciona con N.I.N.A., y los clones de su gama
   suelen usar un protocolo **ASCII compatible Moonlite** (tramas `:cmd#`), pero
   está **sin confirmar**. Hay que averiguarlo antes de que el backend de host
   diga nada útil. Dos vías:
   - Correr `host/tools/probe_ascom.py` contra el Gemini en un PC Windows para
     confirmar que responde por ASCOM y leer sus límites reales (recorrido máximo,
     temperatura).
   - Capturar el tráfico USB del Gemini (un analizador USB, o usbmon/Wireshark en
     Linux) mientras N.I.N.A. o su driver ASCOM lo mueve, y decodificar el juego
     de comandos ASCII. Anotar los hallazgos en `docs/protocol/`, un fichero por
     hallazgo, como se hizo con el protocolo del EAF.

Luego rellenar `gemini_usbh_focuser.cpp`: mapear `move_to`/`position`/`halt`/
`is_moving`/`max_step`/`temperature` sobre los comandos reales del Gemini.

## Cómo probar / qué hace falta

- Una placa **ESP32-P4** con los dos puertos USB accesibles (p. ej. la
  ESP32-P4-Function-EV-Board de Espressif).
- Un enfocador **Gemini** (el clon del EAF de ZWO de ~65 €), mejor sobre un
  telescopio para ver los movimientos.
- Un **ASIAIR** (o una cámara "Air" de la misma familia) que haga de host USB del
  lado de la persona.
- ESP-IDF v5.4+ con soporte de ESP32-P4.

Compila y graba para la P4, elige el backend de host USB del Gemini en
`idf.py menuconfig` → "ZWO EAF persona" → "Focuser backend", conecta el OTG #1 al
ASIAIR y el OTG #2 al Gemini. Hoy, la persona debería enumerarse en el ASIAIR como
un EAF mientras el esqueleto del Gemini no hace nada — ese es el punto de partida.

## Cómo ayudar

Si tienes el hardware, el primer paso de más valor es el **punto 3**: fijar el
protocolo del Gemini y documentarlo en `docs/protocol/`. Todo lo demás es
mecánico una vez se conoce el juego de comandos. Abre una incidencia o un PR con
lo que encuentres.

Las pruebas de conformidad del host (`firmware/test/host/run.sh`) corren sin placa
y son la red de seguridad de todo lo que añadas a la lógica común.

## Referencias

- Periféricos USB de ESP por chip (controladores en S2 / S3 / P4):
  https://docs.espressif.com/projects/esp-iot-solution/en/latest/usb/usb_overview/usb_overview.html
- Host USB en ESP32-P4:
  https://docs.espressif.com/projects/esp-usb/en/latest/esp32p4/usb_host.html
- Componente `cdc_acm_host` (USB Host CDC-ACM):
  https://components.espressif.com/components/espressif/usb_host_cdc_acm
- El conflicto host-vs-dispositivo del USB, en este repo: `docs/ARCHITECTURE.md`.
