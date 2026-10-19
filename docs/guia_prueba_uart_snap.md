# Guía: probar la UART con el SNAP y el PIC

Proyecto Micro 3 · 2026-10-19 · Para después de que Claude Code termine T-03 y compile sin errores.

> **Qué se prueba:** que el PIC devuelva por la UART cada byte que le llega (eco), sin perder ni cambiar ninguno. Corresponde a las pruebas 1.5, 1.6 y 1.7 de `docs/protocolos_prueba.md`.
>
> **Qué NO está verificado:** el orden exacto de los pines del conector del SNAP y el nombre exacto de algunas opciones de MPLAB X. Confírmalos con la serigrafía de tu placa y la guía del SNAP.

## 1. Qué necesitas
- Placa con el PIC16F18426, alimentada con su fuente (3,7 V). **El módulo CP2102 va a 3,3 V, nunca a 5 V.**
- SNAP y su cable de programación (ICSP).
- Módulo USB-serie CP2102 y 2 resistencias (1 kΩ y 2,2 kΩ) del divisor del TX.
- Python con pyserial: `pip install pyserial`.
- `herramientas/prueba_uart.py` (ya está en el repositorio).

## 2. Antes de programar (5 minutos)
1. Fuente encendida: mide **VDD en el pin del PIC** (debe ser ≈3,7 V) y GND común.
2. MCLR (RA3) con su resistencia de 10 kΩ a VDD y 100 nF entre VDD y GND, como en `docs/circuito_uart.svg`.
3. El adaptador USB-serie **todavía desconectado del PIC**.

## 3. Programar con el SNAP
1. Conecta el SNAP a la placa por el conector ICSP: MCLR, VDD, GND, PGD (RA0) y PGC (RA1). Pin 1 con pin 1 (revisa la marca en tu placa).
2. En MPLAB X: clic derecho en el proyecto → *Properties* → *Hardware Tool* → **SNAP**. Si ves una opción para alimentar el circuito desde el SNAP, déjala **desactivada**: la placa usa su fuente.
3. Pulsa **Make and Program Device** (flecha verde con chip). **No uses Debug** en esta prueba.
4. Espera "Programming/Verify complete" en la ventana *Output*. Si el SNAP no detecta el chip, ve a la tabla de la sección 7.

## 4. Probar el adaptador solo (lazo)
Con el adaptador **desconectado del PIC**, une sus pines **TXD y RXD** con un cable corto. Abre una terminal a 115200 8N1 y escribe: lo que escribas debe aparecer de vuelta. Si no, el problema es el adaptador o la terminal, no el PIC. Quita el cable al terminar.

## 5. Conectar al PIC
| Desde | Hasta |
|---|---|
| Adaptador **TXD** | PIC **RC5** (RX) |
| PIC **RC4** (TX) | resistencia de 1 kΩ → nodo → adaptador **RXD**; del nodo, 2,2 kΩ a GND |
| Adaptador **GND** | GND de la placa |
| Adaptador 5V / 3V3 | **no se conectan al PIC** |

## 6. Correr la prueba
1. Cierra cualquier terminal que tenga abierto el puerto COM.
2. Mira el número de COM del adaptador (Administrador de dispositivos → Puertos).
3. En la carpeta del proyecto: `python herramientas/prueba_uart.py COM5` (cambia COM5 por el tuyo).
4. Resultado esperado:
```
1.5  eco de 1 byte:        20/20   (criterio: 20/20)
1.6  256 valores:          256/256 (criterio: 256/256)
1.7  ráfaga de 1000 bytes:  recibí 1000/1000, iguales 1000/1000, ~100 ms
TODAS PASARON
```
El tiempo de la ráfaga debe ser **al menos 87 ms** (1000 bytes a 115200 baud); si da menos, no estás probando el PIC.

Saludo de arranque: el programa manda `Micro3 listo` al encender. Para verlo, abre una terminal a 115200 y pulsa el reset (MCLR) de la placa.

Pruebas de osciloscopio (1.1 a 1.4: niveles, ancho de bit 8,75 µs, formato del byte): `docs/protocolos_prueba.md`.

## 7. Si algo falla (causas probables, de más a menos común)
| Síntoma | Qué revisar |
|---|---|
| El SNAP no encuentra el chip | VDD presente en la placa; cable ICSP al revés o flojo; MCLR con su pull-up; no estar usando RA0/RA1/RA3 para otra cosa |
| Programa bien, pero no responde nada | TX y RX cruzados (el TXD del adaptador va a RC5 del PIC); falta GND común; `main` no activó interrupciones; medir con multímetro que el TXD del adaptador en reposo esté ≈3,3 V |
| Responde, pero con bytes cambiados o basura | El PIC no está a 16 MHz (reloj mal configurado) o el PC no está a 115200 8N1 sin control de flujo |
| Pasan 1.5 y 1.6, falla 1.7 (faltan bytes) | La ISR o el buffer no alcanzan; anota **en qué byte** empieza a fallar y pásaselo a Claude Code |
| "No pude abrir COMx" | Otro programa tiene el puerto abierto, o el número de COM es otro |
| El primer byte sale raro al conectar | Es normal un glitch al conectar el cable; el script descarta lo que llegó antes de empezar |

## 8. Qué reportar
Copia la salida completa del script y dile a Claude Code (o a este chat):
- qué pruebas pasaron y cuáles fallaron, con los números;
- el síntoma exacto (qué mandaste y qué volvió);
- si usaste osciloscopio, lo que mediste.

Claude Code marca `probada` solo con ese resultado en la mano.
