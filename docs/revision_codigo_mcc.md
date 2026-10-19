# Revisión guiada del código generado por MCC

Proyecto Micro 3 · Inicio: 2026-10-19 · Código revisado: `firmware/Micro_Firmware.X` (MCC, regenerado el 2026-10-19)

> **Para qué sirve:** la persona repasa el código de MCC función por función hasta entender cada línea y cada registro. Lo que haya que cambiar queda registrado aquí con su motivo, para pasárselo a Claude Code. **Claude Code: implementa solo las filas con estado `aprobada`** (la persona las aprueba).
>
> **Cómo se usa:** una parte a la vez. Una parte pasa a `entendida` solo cuando la persona lo confirma. Las explicaciones se dan en el chat; aquí solo se guarda la ruta, las decisiones y los cambios.

## Ruta (orden en que se ejecuta el código)

| Parte | Archivo y función | Estado |
|---|---|---|
| 1 | `device_config.c`: palabras de configuración (se leen en el reset, antes de la primera instrucción) | explicada, falta confirmación |
| 2 | `main.c` y `mcc.c`: `main()` y `SYSTEM_Initialize()` | pendiente |
| 3 | `mcc.c`: `PMD_Initialize()` | pendiente |
| 4 | `pin_manager.c`: `PIN_MANAGER_Initialize()` | pendiente |
| 5 | `mcc.c`: `OSCILLATOR_Initialize()` | pendiente |
| 6 | `eusart1.c`: `EUSART1_Initialize()` | pendiente |
| 7 | `interrupt_manager.c`: `INTERRUPT_InterruptManager()` | pendiente |
| 8 | `eusart1.c`: `EUSART1_Write`, `EUSART1_Read`, `EUSART1_is_*` | pendiente |
| 9 | `eusart1.c`: `EUSART1_Transmit_ISR()` | pendiente |
| 10 | `eusart1.c`: `EUSART1_Receive_ISR()` y manejadores de error | pendiente |

## Cambios para Claude Code

Estado: `propuesta` → `aprobada` (solo la persona) → `implementada, sin probar` → `probada`.

| ID | Dónde (MCC) | Qué cambiar en nuestro código | Por qué | Tarea | Estado |
|---|---|---|---|---|---|
| C-01 | `device_config.c`, `PPS1WAY = ON` | `PPS1WAY = OFF` | Con ON el candado de PPS se puede abrir y cerrar **una sola vez**; un cambio posterior de pines (p. ej. al agregar el PWM) se ignora sin avisar. Con OFF no hay esa trampa (DS p.29) | T-02 | propuesta |
| C-02 | `device_config.c`, `FCMEN = ON` | `FCMEN = OFF` | El monitor de reloj solo actúa con osciladores **externos** (DS §8.4, p.129). Con el HFINTOSC no hace nada; se quita para que la configuración no tenga líneas que no se entienden | T-02 | propuesta |
| C-03 | `device_config.c`, todas las demás | Escribirlas igual en `config.c`, con comentarios en español, **todas** (XC8 exige todas) | Son las mismas que decidimos (ver `docs/especificacion_diseno_uart.md` §11) | T-02 | propuesta |
| C-04 | `device_config.c`, `WDTE = OFF` | Dejar OFF en desarrollo; decidir antes de entregar si se activa | Un watchdog reinicia el chip si el programa se cuelga; hoy lo desactivamos para depurar | T-02 | propuesta |

## Preguntas abiertas de la revisión

- ¿Se activa el watchdog en la versión final? (C-04). Lo decide la persona con el profesor.
