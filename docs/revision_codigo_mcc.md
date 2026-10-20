# Revisión guiada del código generado por MCC

Proyecto Micro 3 · Inicio: 2026-10-19 · Código revisado: `firmware/Micro_Firmware.X` (MCC, regenerado el 2026-10-19)

> **Para qué sirve:** la persona repasa el código de MCC función por función hasta entender cada línea y cada registro. Lo que haya que cambiar queda registrado aquí con su motivo, para pasárselo a Claude Code. **Claude Code: implementa solo las filas con estado `aprobada`** (la persona las aprueba).
>
> **Cómo se usa:** una parte a la vez. Una parte pasa a `entendida` solo cuando la persona lo confirma. Las explicaciones se dan en el chat con el formato de `MASTER_PLAN.md` ("Cómo se explica el código"); aquí solo se guarda la ruta, las decisiones y los cambios.

## Mapa: qué ventana de MCC genera qué archivo

| Ventana o módulo de MCC | Qué configura | Archivo y función generados | Parte |
|---|---|---|---|
| System Module (`CONFIG1` a `CONFIG5`) | Bits de configuración | `device_config.c` | 1 |
| System Module (arranque) | Orden de inicialización | `main.c`, `mcc.c`: `SYSTEM_Initialize()` | 2 |
| PMD | Qué periféricos tienen reloj | `mcc.c`: `PMD_Initialize()` | 3 |
| Pin Module (Pin Manager) | Pines, PPS, entrada/salida, analógico/digital | `pin_manager.c`: `PIN_MANAGER_Initialize()` | 4 |
| Reloj (en el `.mc3` se llama `INTERNAL OSCILLATOR`) | Frecuencia del reloj | `mcc.c`: `OSCILLATOR_Initialize()` | 5 |
| EUSART1 | UART: baud, buffers, interrupciones | `eusart1.c` y `eusart1.h` | 6, 8, 9, 10 |
| Interrupt Module | Rutina de interrupción única | `interrupt_manager.c` y `.h` | 7 |
| RESET, WWDT | Reset y watchdog | no vi archivos propios en el proyecto | — |

Los nombres de módulo salen del `.mc3`; en qué pestaña de la ventana está cada casilla no lo he visto.

## Ruta (orden en que se ejecuta el código)

| Parte | Archivo y función | Estado |
|---|---|---|
| 1 | `device_config.c`: palabras de configuración (se leen en el reset, antes de la primera instrucción) | explicada, falta confirmación |
| 2 | `main.c` y `mcc.c`: `main()` y `SYSTEM_Initialize()` | pendiente |
| 3 | `mcc.c`: `PMD_Initialize()` | pendiente |
| 4 | `pin_manager.c`: `PIN_MANAGER_Initialize()` | pendiente |
| 5 | `mcc.c`: `OSCILLATOR_Initialize()` | pendiente |
| 6 | `eusart1.c`: `EUSART1_Initialize()` (revisada antes que 2–5 por decisión de la persona; la UART también necesita las partes 4 y 5) | explicada, falta confirmación |
| 7 | `interrupt_manager.c`: `INTERRUPT_InterruptManager()` | pendiente |
| 8 | `eusart1.c`: `EUSART1_is_*`, `get_last_status`, `Read`, `Write` | explicada, falta confirmación |
| 9 | `eusart1.c`: `EUSART1_Transmit_ISR()` | pendiente |
| 10 | `eusart1.c`: `EUSART1_Receive_ISR()` y manejadores de error | pendiente |

## Cambios para Claude Code

Orden de trabajo y comportamiento exacto de cada función: `docs/instrucciones_claude_code_uart.md`. C-05 lo hace la persona en MCC antes (y después no se vuelve a pulsar *Generate*).

Estado: `propuesta` → `aprobada` (solo la persona) → `implementada, sin probar` → `probada`.

| ID | Dónde (MCC) | Qué cambiar en nuestro código | Por qué | Tarea | Estado |
|---|---|---|---|---|---|
| C-01 | `device_config.c`, `PPS1WAY = ON` | `PPS1WAY = OFF` | Con ON el candado de PPS se puede abrir y cerrar **una sola vez**; un cambio posterior de pines (p. ej. al agregar el PWM) se ignora sin avisar. Con OFF no hay esa trampa (DS p.29) | T-02 | implementada, sin probar (2026-10-19) |
| C-02 | `device_config.c`, `FCMEN = ON` | `FCMEN = OFF` | El monitor de reloj solo actúa con osciladores **externos** (DS §8.4, p.129). Con el HFINTOSC no hace nada; se quita para que la configuración no tenga líneas que no se entienden | T-02 | implementada, sin probar (2026-10-19) |
| C-03 | `device_config.c`, todas las demás | Escribirlas igual en `config.c`, con comentarios en español, **todas** (XC8 exige todas) | Son las mismas que decidimos (ver `docs/especificacion_diseno_uart.md` §11) | T-02 | rechazada (2026-10-19): la persona no ve necesario comentar ese archivo; `device_config.c` queda como lo generó MCC salvo C-01, C-02 |
| C-05 | `eusart1.c` líneas 56–57, `EUSART1_TX_BUFFER_SIZE` y `EUSART1_RX_BUFFER_SIZE` (en MCC: ventana EUSART1, tamaños de buffer por software) | TX = 64 y RX = 32 | Con 8 no cabe ni una trama y el margen es 0,6 ms. Motivos y cuentas: `docs/criterios_diseno.md` §6.1 | T-03 | propuesta |
| C-06 | `eusart1.c`, los punteros `EUSART1_Tx/RxDefaultInterruptHandler` y las funciones `EUSART1_SetTx/RxInterruptHandler` | Quitarlos: `interrupt_manager.c` llama directo a `EUSART1_Transmit_ISR()` y `EUSART1_Receive_ISR()` | Los punteros solo sirven para cambiar la función de aviso en tiempo de ejecución. Nosotros nunca la cambiamos: son 4 líneas y un salto indirecto de más dentro de la interrupción | T-03 | implementada, sin probar (2026-10-19) |
| C-07 | `EUSART1_Read()` (líneas 151–171) | Que no espere: si no hay byte devuelve `false`; si hay, lo entrega por puntero y devuelve `true` (`uart_rx_read(uint8_t *byte)` de la especificación §6) | El `while(0 == eusart1RxCount)` cuelga el programa si el PC no manda nada. El enunciado prohíbe esperas bloqueantes | T-03 | implementada, sin probar (2026-10-19) |
| C-08 | `EUSART1_Write()` (líneas 173–194) | Que no espere: si el buffer está lleno devuelve `false`. Sobre esa base, `tx_DAT(const char*, uint16_t)` y `tx_string(const char*)` | El `while(0 == eusart1TxBufferRemaining)` cuelga el programa si el buffer se llena. Además se piden `tx_DAT` y `tx_string` | T-03 | implementada, sin probar (2026-10-19) |
| C-09 | Contadores `RxCount` y `TxBufferRemaining` (Read, Write y las dos ISR) | Quitarlos: buffer lleno cuando `head + 1 == tail`, vacío cuando `head == tail`; cada índice lo mueve un solo lado | Con contador, main y la ISR modifican la misma variable y por eso MCC apaga `RC1IE`/`TX1IE` un instante. Sin contador no hace falta (`docs/criterios_diseno.md` §6) | T-03 | implementada, sin probar (2026-10-19) |
| C-10 | `EUSART1_is_tx_done()` | Devolver `true` solo si el buffer de TX está vacío **y** `TRMT` = 1 | `TRMT` solo mira el registro de desplazamiento; con bytes aún en el buffer podría decir "terminó" | T-03 | implementada, sin probar (2026-10-19) |
| C-04 | `device_config.c`, `WDTE = OFF` | Dejar el watchdog apagado. No hay cambio de código: ya está en OFF | Un watchdog reinicia el chip si el programa se cuelga; la persona decide apagarlo | — | aprobada (2026-10-19), sin cambio de código |

Para implementar C-01 y C-02 (T-02): solo cambiar esas dos líneas de `device_config.c` a mano (`PPS1WAY = OFF`, `FCMEN = OFF`) y actualizar el comentario de cada una. Después **no** pulsar *Generate* en MCC; si hubiera que hacerlo, antes se ponen las mismas dos opciones en el `.mc3` (ventana System Module). El `.mc3` todavía dice ON/ON.

## Preguntas abiertas de la revisión

- ¿Se confirma con el profesor que el watchdog quede apagado en la versión final? La persona decidió apagarlo (C-04); si el profesor pide activarlo, el `main` tendría que reiniciar el contador (`CLRWDT`) en cada vuelta del bucle.
