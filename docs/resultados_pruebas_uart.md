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
| Python / `prueba_uart.py` | todavía no instalado |

## Pruebas hechas

| ID | Qué se verificó | Cómo | Resultado | Observaciones |
|---|---|---|---|---|
| — | Compilación | *Clean and Build* en MPLAB X | **pasó** (compiló) | La persona no anotó el número de warnings. Pendiente anotarlo |
| 0.2 | Programación con SNAP | *Make and Program Device* | **pasó** | La persona confirmó que cargó el código. No se copió el mensaje de MPLAB X |
| 1.18 | Saludo tras el reinicio | Terminal abierta, se apaga y enciende la placa | **pasó** | Apareció `Micro3 listo`. Confirma también el reloj de 16 MHz y el baud de 115200 (si no, saldrían caracteres erróneos), las interrupciones y `tx_string` |
| 1.5 | Eco de un byte (informal, con PuTTY) | Se escribió `hola` con y sin el cable RXD del adaptador | **pasó** | Con el cable puesto, `hola` volvió. Con el RXD desconectado no apareció nada, así que el eco venía del PIC y no de PuTTY |
| — | Ráfaga aproximada | Se pegó un texto de unos 400 caracteres en PuTTY | **pasó** a simple vista | Volvió completo, con acentos. No se comparó byte a byte. La segunda pegada seguida está por confirmar |

## Observaciones sin número de prueba

- El LED azul de RXD del módulo parpadea al arrancar la placa (el saludo saliendo del PIC). El LED rojo fijo es el indicador de alimentación del módulo (comportamiento típico; no confirmado en la hoja del módulo).
- PuTTY no baja de línea al recibir un `CR`: el eco de Enter deja el cursor al inicio de la última línea y lo siguiente se sobrescribe. No es pérdida de datos. Se arregla activando *Terminal → Implicit LF in every incoming CR*.

## Pruebas pendientes

| ID | Qué falta | Nota |
|---|---|---|
| 0.1 | Medir VDD en los pines 1 y 14 del PIC | Esperado 3,6 a 3,8 V |
| 0.3 | Revisión de silicio (MPLAB X → *Window > Dashboard*) | Esperada A4 (ID `0x2004`) |
| 0.6 | Consumo de corriente | Anotar el valor |
| 1.1 a 1.4 | Capa física con osciloscopio y multímetro (1.4: nivel en RC5 ≥ 1,73 V) | La 1.4 se puede hacer ya con el multímetro |
| 1.5 formal | 20 de 20 con el script | Necesita Python |
| 1.6 | Los 256 valores | Necesita Python |
| 1.7 | Ráfaga exacta de 1000 bytes (≥ 87 ms) | Necesita Python |
| 1.17 | Desconectar y reconectar el USB | Se puede hacer ya con PuTTY |
| 0.4, 0.5, 1.9 a 1.13, 1.15, 1.16 | Falta código de prueba (LED, pin RC2, contadores visibles, versión sin lectura) | Ver `docs/guia_prueba_uart_snap.md` |
