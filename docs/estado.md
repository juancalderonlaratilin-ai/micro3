# Estado del proyecto (bitácora de avance)

Proyecto Micro 3 · Última actualización: 2026-11-03

> **Para qué sirve:** es lo primero que se lee al abrir una sesión nueva (Claude Code lo carga desde `CLAUDE.md`; en el chat de Claude se le pide "lee `docs/estado.md`"). Dice **dónde quedó el trabajo y qué sigue**. Las decisiones y sus motivos viven en `criterios_diseno.md`, no aquí.
>
> **Reglas para actualizarlo (quien trabaje: Claude Code, el chat o tú):**
> 1. Al terminar cada tarea: marcar lo hecho, reescribir "Siguiente paso" y agregar una fila al registro.
> 2. Mantenerlo corto (menos de 80 líneas). Lo viejo se resume o se borra.
> 3. Solo hechos comprobados. Si algo no se probó, escribir "sin probar".
> 4. No copiar decisiones de diseño: enlazar al documento donde están.

---

## Fase actual

**Fase 1:** enlace UART PC ↔ PIC (binario). T-02, T-03 y T-04 implementadas, sin probar en el chip.
**Fase 2 (iniciada el 2026-10-30):** ADC de I y Q a 1 kHz. Código recibido en un zip y fusionado en `firmware/` (T-09, sin probar); especificación en `docs/especificacion_adc.md`. El PWM sigue sin código.

## Hecho

- [x] Especificación de diseño de la UART: `docs/especificacion_diseno_uart.md`
- [x] Criterios de diseño y valores escogidos: `docs/criterios_diseno.md`
- [x] Notas verificadas del PIC16F18426 con páginas: `docs/notas_pic16f18426.md`
- [x] Protocolos de prueba por módulo: `docs/protocolos_prueba.md`
- [x] Circuito UART con el módulo CP2102 (divisor 1 kΩ + 2,2 kΩ en el TX): `docs/circuito_uart.svg`
- [x] Proyecto MPLAB X creado en `firmware/Micro_Firmware.X`
- [x] Repositorio git local (rama `master`)
- [x] Guía para configurar MCC como referencia: `docs/guia_mcc_referencia.md`
- [x] Especificación del ADC (requisitos, tiempos, jitter, SINAD > 60 dB, referencia, histograma, tramas y pruebas): `docs/especificacion_adc.md`
- [x] Especificaciones para el preinforme (requisitos de la clase, valores del UART y el ADC, bloques, diagramas de tiempo, cumplimiento): `docs/especificaciones_preinforme.md`
- [ ] Revisión guiada del código de MCC (partes 1 y 6 de 12 explicadas): `docs/revision_codigo_mcc.md`
- [x] Roles definidos: Gemini planifica y Claude Code desarrolla (`MASTER_PLAN.md`, `CLAUDE.md` y `GEMINI.md`)
- [ ] Repositorio subido a GitHub

## Estado del código

En `firmware/Micro_Firmware.X` hay código **generado por MCC en clase**, que la persona indica que sí puede usarse como base (el enunciado dice que no: confirmar con el profesor). Se revisa en `docs/revision_codigo_mcc.md`.

Las dos tareas siguientes están **implementadas, sin probar**. Compilaciones de prueba con XC8 2.40 (línea de comandos y `make` del proyecto, salida fuera de la carpeta): cero errores. Sin compilar todavía en MPLAB X.

- **T-03** (C-06 a C-10; C-05, buffers 64/32, ya estaba): `eusart1.c/.h` con buffers circulares sin contador, `Write`/`Read` sin espera, `tx_DAT` (todo o nada) y `tx_string`; `interrupt_manager.c` llama directo a las ISR; `main.c` hace eco y manda `Micro3 listo`. Agregados que la instrucción no pedía: `#error` si un buffer no es potencia de 2, `EUSART1_get_overflow()` y el contador `echoBytesLost`.
- **T-02** (C-01, C-02): `device_config.c` con `PPS1WAY = OFF` y `FCMEN = OFF` (comprobado en el `.hex`). El `.mc3` sigue en ON/ON: **no pulsar *Generate*** en MCC.
- **T-05 retirada (2026-10-21):** el código de prueba del LED de RA5 (`heartbeat.c/.h`, rama de Timer4 en `interrupt_manager.c`, llamada en `main.c`) se quitó a petición de la persona; `TRISA` = 0x3F y `ANSELA` = 0x37 volvieron al valor de MCC. Compilación de prueba XC8 2.40: 6 avisos `(520)`, 511 de 16384 palabras de programa y 166 de 2048 bytes de datos. Los datos verificados de Timer2/4/6 siguen en `docs/notas_pic16f18426.md` §9.3.
- **T-04** (protocolo de tramas con CRC-8, `implementada, sin probar`): `crc8.c/.h` (CRC-8 bit a bit; vectores de la especificación §3 calculados en Python con `herramientas/crc8_vectores.py`) y `protocol.c/.h` con `frame_send`, parser (`ESPERA_A5`, `ESPERA_5A`, `CUERPO`), PING → PONG, ERROR 1 y 2 con resincronización, y ERROR 3 (overflow) y 4 (framing/overrun). `main.c` con `MODO_ECO` (0 = protocolo, 1 = eco crudo). Archivos nuevos registrados en `configurations.xml` y `Makefile-default.mk`; `eusart1.*`, `interrupt_manager.c`, `pin_manager.c`, `mcc.c` y `device_config.c` no se tocaron. Compilación de prueba XC8 2.40 (línea de comandos): 0 errores; con `MODO_ECO` = 0 quedan 5 avisos `(520)` de `eusart1.c` y `PIN_MANAGER_IOC`; 840 de 16384 palabras de programa y 212 de 2048 bytes de datos. El C no se ejecutó en ningún lado (no hay compilador de PC); la lógica del parser se simuló en Python con 8 casos.
- **T-09** (ADC de I y Q a 1 kHz y trama `DATA_IQ`; `implementada, sin probar`): el código llegó en `Micro_Firmware.zip` (2026-10-30) y **ya está copiado a `firmware/Micro_Firmware.X`** (16 archivos; el zip solo agregaba sobre lo que había). Trae `adc_iq.c/.h` (Timer0 dispara el ADC cada 1 ms; I en RC0, Q en RC1; **sin promedio desde el 2026-11-03**: cada par es una sola muestra, 1000 tramas `DATA_IQ`/s ≈ 78 % de la línea), `adc_prueba.c/.h` (texto para el Serial Plotter) y versiones de `protocol.*`, `main.c`, `interrupt_manager.c`, `adcc.*` y `tmr0.*` que incluyen todo lo de T-02, T-03 y T-04. No se compiló en esta sesión. **Atención:** su `main.c` tiene `MODO_PRUEBA_ADC 1` (solo manda texto para el Serial Plotter, sin protocolo): para volver al protocolo hay que ponerlo en 0. La aprobación de T-09 y `docs/instrucciones_claude_code_adc.md` (que el código cita, con D-3 y las pruebas 4.9, 4.11 y 4.12) no se encontraron.

## Sin confirmar

1. Baud pedido por el profesor: 115200 (lo escogido) o 125 000 (lo que dejó el MCC de clase).
2. Si el entregable final puede usar MCC: la persona dice que sí; el enunciado dice que no. Confirmarlo con el profesor.
3. Que el chip sea `PIC16F18426` y no el `LF` (máximo 3,6 V), y que el PIC se alimente de los 3,7 V.
4. Marcado y pines del módulo CP2102 (jumper de nivel en 3,3 V; TXD en reposo ≈3,3 V con el multímetro).
5. Que los pines propuestos coincidan con la placa.
6. Cómo se mide el requisito **SINAD > 60 dB** (ADC con senoidal casi a escala completa, lo adoptado, o señal real de ±185 mV, donde un 12 bits ideal da 54 dB) y a qué frecuencias. De dónde salen los 352 µV del profesor (son 12 bits y 5 V; con 3,7 V son 261 µV). Otras dudas de la clase (jitter < 10 ns y si aplica a Q, «modular y ver los 10 Hz», precisión del auto-cero): `docs/especificaciones_preinforme.md` §10.2.
7. Unidad de `ADACQ` = 160: 10 µs (lo que guarda MCC), 40 µs (Fig. 32-2 del datasheet) o 320 µs (texto del registro). Se mide con RC2 (prueba 4.16); la fórmula está en `docs/especificacion_adc.md` §3.1.
8. Modelo e impedancia de salida (`Zout`) del opa de I y Q de PCB2 (el datasheet del MCP604 ya está en `docs/MCP601-2.PDF` y no da `Zout`; `.gitignore` ignora `docs/*.pdf`) y si `Vref` queda en `VDD` o pasa a FVR o externa: `docs/especificacion_adc.md` §3.2 y §7.1.

## Siguiente paso

1. La persona: cerrar y volver a abrir el proyecto en MPLAB X (se quitaron `heartbeat.c/.h`), *Clean and Build* (anotar los warnings), programar con el SNAP y correr las pruebas de `docs/protocolos_prueba.md` (guía: `docs/guia_prueba_uart_snap.md`). T-03: 1.5, 1.6, 1.7, 1.9, 1.13 y 1.14. T-02: repetir 1.5, 1.6 y 1.7. El resultado decide `probada` o `bloqueada`. T-04: *Clean and Build* con `MODO_ECO` = 0 y pruebas 2.1 a 2.13 de `docs/especificacion_protocolo.md` §8 y `docs/protocolos_prueba.md` Módulo 2; hace falta `herramientas/prueba_protocolo.py` (sin escribir; la 2.11 necesita el depurador y la 2.12 es opcional). Para repetir 1.5, 1.6 y 1.7 hay que poner `MODO_ECO` = 1.
2. Decidir (planificador/persona): si se acepta que queden avisos `(520)` (el criterio de T-03 dice "sin warnings"); cómo se hacen 1.9 y 1.14, que el eco no cubre (no llama a `tx_DAT` y `main` siempre lee el RX); y el siguiente módulo, que ya se decidió el 2026-10-21: **T-04** (protocolo con CRC-8, aprobada) y después T-06/T-07 (PWM, propuestas). Ver `MASTER_PLAN.md`, `docs/especificacion_protocolo.md` y `docs/especificacion_pwm.md`.
3. Pruebas 0.5 y 1.12 (LED en RA5): sin hacer hasta que haya otro código de prueba.
4. Seguir la revisión del código de MCC (`docs/revision_codigo_mcc.md`), parte por parte. T-01 (mover MCC a `referencia_mcc/`) queda en espera. La prueba 0.4 (reloj) sigue sin código de prueba.
5. ADC: la persona agrega `adc_iq.c`, `adc_prueba.c`, `adcc.c` y `tmr0.c` al proyecto de MPLAB X si no están (la carpeta no tiene `nbproject/`), compila con `MODO_PRUEBA_ADC` = 0 y corre 4.1 a 4.8 y 4.10 a 4.17 de `docs/protocolos_prueba.md` (**SINAD > 60 dB en la 4.15**). La 4.18 (Serial Plotter) se hace con `MODO_PRUEBA_ADC` = 1. Con el promedio quitado, revisar en la 4.14 que `adcParesPerdidos` y `adcTramasPerdidas` sigan en 0 a 1000 tramas/s.

## Registro de sesiones

| Fecha | Quién | Qué se hizo | Resultado |
|---|---|---|---|
| 2026-10-16 | Chat de Claude | Especificación, criterios, protocolos de prueba y circuito UART | Documentos en `docs/`. Sin código |
| 2026-10-19 | Chat de Claude | Roles Gemini (planificador) y Claude Code (desarrollador): `CLAUDE.md` y `GEMINI.md` cargan `MASTER_PLAN.md`. Versiones MPLAB X 6 y XC8 2.40. Guía para configurar MCC. Formato fijo para explicar código. MCC permitido según la persona; T-01 en espera | Archivos escritos. Sin probar que cada herramienta cargue el contexto. MCC por confirmar con el profesor |
| 2026-10-19 | Chat de Claude | Revisión de `eusart1.c` regenerado (16 MHz, SP1BRG=34 correctos; buffers 8/8; 9 puntos a no copiar, `docs/guia_mcc_referencia.md` §8). Revisión parte por parte: parte 1 explicada, C-01 a C-04 propuestos; mapa de ventanas de MCC a archivos. Instrucciones `docs/instrucciones_claude_code_uart.md`; T-03 aprobada (C-06 a C-10) | Solo lectura, sin compilar. Falta C-05 en MCC y la implementación |
| 2026-10-19 | Chat de Claude | Pruebas UART con Python: 1.5 (20/20), 1.6 (256/256), 1.7 (1000/1000, 141 ms) pasaron; 1.18 y 0.3 pasaron (ID `0x30D2`, revisión `0x2002`). Detalle en `docs/resultados_pruebas_uart.md`. La persona aprobó C-01, C-02 y C-04 (watchdog apagado) y rechazó C-03; T-02 reducida y aprobada | Faltan 0.1, 1.4 y 1.17 (la persona los difiere). T-03 aún `implementada, sin probar` en 1.9, 1.13 y 1.14 |
| 2026-10-19 | Claude Code | T-03 en 5 pasos revisados uno a uno: `eusart1.c/.h`, `interrupt_manager.c`, `main.c` de eco (C-06 a C-10); buffers 64/32 ya presentes; archivos en ISO-8859-1 con CRLF. T-02 reducida: `device_config.c` con `PPS1WAY = OFF` y `FCMEN = OFF` (C-01, C-02); nada más cambió | Las dos `implementada, sin probar`. XC8 2.40: sin errores, 6 avisos (520); el `.hex` muestra los dos bits de T-02 en 0. Sin MPLAB X ni chip. Sin commit |
| 2026-10-19 y 21 | Claude Code | T-05 en 4 pasos (RA5 como salida, Timer4 en `heartbeat.c/.h`, rama de TMR4 en `interrupt_manager.c`, llamada en `main.c`; datos en `notas_pic16f18426.md` §3 y §9.3). El 2026-10-21 se retiró todo: archivos borrados y referencias quitadas de `main.c`, `interrupt_manager.c`, `pin_manager.c`, `configurations.xml` y `Makefile-default.mk` | Antes de retirar: `make` sin errores, 6 avisos (520), `T4CON` 0xF4, `T4CLKCON` 0x01, `T4PR` 124. Después: compila con XC8 2.40 (6 avisos 520). Sin MPLAB X ni chip. Sin commit |
| 2026-10-21 | Chat de Claude | Especificaciones `docs/especificacion_pwm.md` y `docs/especificacion_protocolo.md` (pines, eventos, tareas, pruebas). Tareas T-04 (aprobada), T-06, T-07 y T-08 (propuestas) y cambios CP-1 a CP-5 en `MASTER_PLAN.md`. Decisiones de la persona anotadas en `criterios_diseno.md` §10 | Solo documentos, sin código. Faltan en el chip las pruebas de T-02 y T-03 |
| 2026-10-21 | Claude Code | T-04 en 5 pasos: `crc8.c/.h`, `protocol.c/.h` (frame_send, parser, PING → PONG), `MODO_ECO` en `main.c`, ERROR 1 y 2 con resincronización (`resync`), ERROR 3 y 4, `parser_reset`; archivos registrados en `configurations.xml` y `Makefile-default.mk` | `implementada, sin probar`. XC8 2.40 (línea de comandos): sin avisos nuevos. Sin MPLAB X ni chip. Sin commit |
| 2026-10-30 | Chat de Claude | Zip del ADC fusionado: 16 archivos copiados a `firmware/Micro_Firmware.X` (el zip solo agregaba; `crc8.*`, `device_config.*`, `eusart1.*`, `interrupt_manager.h` y `pin_manager.c` ya eran idénticos). Nuevo `docs/especificacion_adc.md`. Requisito **SINAD del ADC > 60 dB** en `MASTER_PLAN.md`, `docs/especificacion_diseno_uart.md` y `docs/criterios_diseno.md` (§1, §10, §11). Tipos `0x20`, `0xA0`, `0xA1` en `docs/especificacion_protocolo.md`; pruebas 4.10 a 4.18 en `docs/protocolos_prueba.md`; C-11, C-12 y partes 11 y 12 en `docs/revision_codigo_mcc.md`; datos del ADCC con páginas en `docs/notas_pic16f18426.md` §9.2. Después: fuente de disparo (§2.1), fórmula de `ADACQ` (§3.1), impedancia del opa (§3.2) y origen de `Vref` (§7.1) en `docs/especificacion_adc.md`; nuevo `docs/especificaciones_preinforme.md` (requisitos de la clase, tabla de valores, bloques, diagramas de tiempo, cumplimiento) | Documentos y firmware. Sin compilar en MPLAB X ni medir: no hay resultados del ADC. Sin commit |
| 2026-11-03 | Claude (cowork) | Se quitó el promedio de 5 del ADC a petición de la persona (reemplaza D-1): `ADC_N_PROMEDIO` eliminado de `adc_iq.h`; la ISR de `adc_iq.c` entrega cada par (I, Q) sin sumar y `adc_iq_get` ya no divide; `adc_prueba.c/.h` cuenta pares directamente (sigue 1 línea cada 50 pares); comentarios de `main.c`. Archivos en Windows-1252 con CRLF | Revisión de sintaxis con gcc y cabeceras simuladas: sin errores. Sin compilar con XC8 ni probar en el chip. Docs actualizados en la misma sesión: `MASTER_PLAN.md`, `criterios_diseno.md`, `especificacion_adc.md`, `especificacion_protocolo.md`, `especificacion_diseno_uart.md`, `protocolos_prueba.md` y `especificaciones_preinforme.md`. La prueba 6.6 (< 50 % de la línea) ya no se cumple: queda para decidir |
