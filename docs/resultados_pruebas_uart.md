# Resultados de pruebas: UART (Fase 1)

Proyecto Micro 3 · Primera sesión de pruebas: 2026-10-19 · Procedimiento: `docs/guia_prueba_uart_snap.md` · Criterios: `docs/protocolos_prueba.md`

> **Cómo se usa:** una fila por prueba, con lo que se vio. Solo se marca `pasó` lo que la persona confirmó. Lo que no se ha medido queda en "pendiente". Cuando se repita una prueba, se agrega una fila nueva, no se borra la anterior.

## Condiciones de la sesión

| Dato | Valor |
|---|---|
| Código | Eco de la UART con buffers circulares (tarea T-03, estado `implementada, sin probar`) |
| Compilador | MPLAB X 6, XC8 2.40 |
| Versión de git | pendiente de anotar (`git rev-parse --short HEAD`) |
| Chip | PIC16F18426, programado con SNAP |
| Alimentación de la placa | 3,7 V (fuente de laboratorio) |
| Adaptador USB-serie | módulo CP2102, puerto **COM6**; TX del PIC por divisor 1 kΩ + 2,2 kΩ |
| Terminal | PuTTY, 115200 8N1, sin control de flujo, sin eco local |
| Tamaño del programa (Dashboard de MPLAB X) | Memoria de datos: 166 B de 2048 (8 %). Programa: 565 palabras de 16 384 (3 %). Checksum `0xC99F`, CRC32 `0xC7DC1207`. Optimización: *Disabled*. SNAP serie `BUR184673014` |
| Python / `prueba_uart.py` | Python 3.12 + pyserial; `python herramientas\prueba_uart.py COM6` (PuTTY cerrado) |

## Pruebas hechas

| ID | Qué se verificó | Cómo | Resultado | Observaciones |
|---|---|---|---|---|
| — | Compilación | *Clean and Build* en MPLAB X | **pasó** (compiló) | La persona no anotó el número de warnings. Pendiente anotarlo |
| 0.2 | Programación con SNAP | *Make and Program Device* | **pasó** | La persona confirmó que cargó el código. No se copió el mensaje de MPLAB X |
| 1.18 | Saludo tras el reinicio | Terminal abierta, se apaga y enciende la placa | **pasó** | Apareció `Micro3 listo`. Confirma también el reloj de 16 MHz y el baud de 115200 (si no, saldrían caracteres erróneos), las interrupciones y `tx_string` |
| 1.5 | Eco de un byte (informal, con PuTTY) | Se escribió `hola` con y sin el cable RXD del adaptador | **pasó** | Con el cable puesto, `hola` volvió. Con el RXD desconectado no apareció nada, así que el eco venía del PIC y no de PuTTY |
| 1.5 | Eco de 1 byte (formal, script) | `prueba_uart.py COM6`, byte 0x41 | **pasó**, 20/20 | Criterio 20/20 (2026-10-19) |
| 1.6 | Los 256 valores | `prueba_uart.py COM6`, de 0x00 a 0xFF uno por uno | **pasó**, 256/256 | Criterio 256/256. Los datos binarios pasan sin alterarse (incluye 0x00, 0x0D, 0x0A y 0xFF) |
| 1.7 | Ráfaga de 1000 bytes | `prueba_uart.py COM6`, 1000 bytes seguidos | **pasó**, 1000/1000 iguales | Tardó 141 ms (mínimo teórico 87 ms). La diferencia es la latencia del USB y del PC, no pérdida. Sin pérdida con el buffer RX de 32 bytes |
| — | Ráfaga aproximada | Se pegó un texto de unos 400 caracteres en PuTTY | **pasó** a simple vista | Volvió completo, con acentos. No se comparó byte a byte. La segunda pegada seguida está por confirmar |
| 0.3 | Identificación del chip | MPLAB X → *Window > Dashboard* → *Refresh Debug Tool Status* | **pasó** | Device Id `0x30D2` (coincide con PIC16F18426). Device Id Revision `0x2002`. SNAP: Boot Version 01.01.55. La hoja de erratas consultada (80000799C) solo documenta la revisión A1 (`0x8005`) y no lista `0x2002`, así que no sé qué letra corresponde. Esa hoja no trae erratas de UART, oscilador ni PPS. Corrección: antes se había escrito "A4, 0x2004" como esperado y no estaba verificado |
| 0.5 | LED de estado (T-05) | LED con resistencia en RA5 (patilla 2); conteo con cronómetro | **pasó** (reporte de la persona) | 10 parpadeos en unos 10 s. No se anotó el tiempo exacto; el criterio es 1 s ± 5 % |
| 1.5, 1.6, 1.7 | Repetición con el código nuevo (T-02: `PPS1WAY` y `FCMEN` en OFF; T-05: LED) | `1..5 \| ForEach-Object { python herramientas\prueba_uart.py COM6 }` | **pasó** 5 de 5 corridas | Cada corrida: 20/20, 256/256, 1000/1000 y `TODAS PASARON`. Ráfaga de 1000 bytes en 140 ms. Confirma que T-02 no rompió la UART y que T-05 no la afecta |

## Observaciones sin número de prueba

- El LED azul de RXD del módulo parpadea al arrancar la placa (el saludo saliendo del PIC). El LED rojo fijo es el indicador de alimentación del módulo (comportamiento típico; no confirmado en la hoja del módulo).
- PuTTY no baja de línea al recibir un `CR`: el eco de Enter deja el cursor al inicio de la última línea y lo siguiente se sobrescribe. No es pérdida de datos. Se arregla activando *Terminal → Implicit LF in every incoming CR*.

## Pruebas pendientes

| ID | Qué falta | Nota |
|---|---|---|
| 0.1 | Medir VDD en los pines 1 y 14 del PIC | Esperado 3,6 a 3,8 V |
| 0.6 | Consumo de corriente | Anotar el valor |
| 1.1 a 1.4 | Capa física con osciloscopio y multímetro (1.4: nivel en RC5 ≥ 1,73 V) | La 1.4 se puede hacer ya con el multímetro |
| 1.17 | Desconectar y reconectar el USB | Se puede hacer ya con PuTTY |
| 1.12 | Que el LED siga parpadeando mientras se transmiten datos | Las 5 corridas ya pasaron; falta que la persona diga qué hizo el LED durante ellas |
| 0.4, 1.9 a 1.11, 1.13, 1.15, 1.16 | Falta código de prueba (LED, pin RC2, contadores visibles, versión sin lectura) | Ver `docs/guia_prueba_uart_snap.md` |
