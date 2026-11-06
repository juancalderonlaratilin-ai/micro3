# Estado del proyecto (bitácora de avance)

Proyecto Micro 3 · Última actualización: 2026-11-05 (revisión de coherencia de los `.md` contra el código; sin cambios de código)

> **Para qué sirve:** es lo primero que se lee al abrir una sesión nueva (Claude Code lo carga desde `CLAUDE.md`; en el chat de Claude se le pide "lee `docs/estado.md`"). Dice **dónde quedó el trabajo y qué sigue**. Las decisiones y sus motivos viven en `criterios_diseno.md`, no aquí.
>
> **Reglas para actualizarlo (quien trabaje: Claude Code, el chat o tú):**
> 1. Al terminar cada tarea: marcar lo hecho, reescribir "Siguiente paso" y agregar una fila al registro.
> 2. Mantenerlo corto (menos de 80 líneas). Lo viejo se resume o se borra.
> 3. Solo hechos comprobados. Si algo no se probó, escribir "sin probar".
> 4. No copiar decisiones de diseño: enlazar al documento donde están.

---

## Fase actual

**Fase 1:** enlace UART PC ↔ PIC: eco probado el 2026-10-19.
**Fase 2 (actual):** ADC de I y Q a 1 kHz con envío en **texto ASCII** (`xxxx,xxxx\r\n`, 500 líneas/s; decisión de la persona del 2026-11-04). La persona confirmó que el modo ASCII funciona; faltan las mediciones.
**Cambio de enfoque del profesor (2026-11-04):** el auto-cero con PWM pasa a Macro. En Micro 3, cada par del ADC fija el duty de un PWM por canal y un filtro pasa bajos devuelve la señal analógica (`docs/especificacion_pwm.md`); código en `pwm.c/.h` (T-06), compilado con XC8 2.40 (2026-11-04), sin probar en el chip.
**En una línea:** Timer0 (1 ms) dispara el ADC; cada disparo convierte I y Q (1 kHz por canal, 1000 pares/s); con cada par se actualizan PWM6/RA2 (I) y PWM7/RA4 (Q) sobre Timer2 (15,625 kHz, 10 bits); la UART manda 1 de cada 2 pares (500 líneas/s). **Sin filtro anti-aliasing** en esta entrega. Tabla completa: «Qué tiene esta entrega» en `MASTER_PLAN.md`.

## Hecho

- [x] Especificación de diseño de la UART: `docs/especificacion_diseno_uart.md`
- [x] Criterios de diseño y valores escogidos: `docs/criterios_diseno.md`
- [x] Notas verificadas del PIC16F18426 con páginas: `docs/notas_pic16f18426.md`
- [x] Protocolos de prueba por módulo: `docs/protocolos_prueba.md`
- [x] Circuito UART con el módulo CP2102 (divisor 1 kΩ + 2,2 kΩ en el TX): `docs/circuito_uart.svg`
- [x] Proyecto MPLAB X creado en `firmware/Micro_Firmware.X`
- [x] Repositorio git local (rama `master`)
- [x] Guía para configurar MCC como referencia: `docs/guia_mcc_referencia.md`
- [x] Especificación del ADC (requisitos, tiempos, jitter, SINAD > 60 dB, referencia, histograma, envío de datos y pruebas): `docs/especificacion_adc.md`
- [x] Especificaciones para el preinforme (requisitos de la clase, valores del UART y el ADC, bloques, diagramas de tiempo, cumplimiento): `docs/especificaciones_preinforme.md`
- [ ] Revisión guiada del código de MCC (partes 1 y 6 de 12 explicadas): `docs/revision_codigo_mcc.md`
- [x] Roles definidos: Gemini planifica y Claude Code desarrolla (`MASTER_PLAN.md`, `CLAUDE.md` y `GEMINI.md`)
- [ ] Repositorio subido a GitHub

## Estado del código

En `firmware/Micro_Firmware.X` hay código **generado por MCC en clase**, que es la base del firmware. Se revisa en `docs/revision_codigo_mcc.md`.

- **T-02** (C-01, C-02): `device_config.c` con `PPS1WAY = OFF` y `FCMEN = OFF` (comprobado en el `.hex`). El `.mc3` sigue en ON/ON: **no pulsar *Generate*** en MCC.
- **T-03** (C-06 a C-10): `eusart1.c/.h` con buffers circulares sin contador (RX 32, TX 64), `Write`/`Read` sin espera, `tx_DAT` (todo o nada) y `tx_string`; `interrupt_manager.c` llama directo a las ISR. El eco se probó el 2026-10-19 (1.5, 1.6 y 1.7); el `main.c` de eco ya no está.
- **T-04 retirada (2026-11-04):** la trama binaria con suma de verificación se quitó; `crc8.*` y `protocol.*` se borraron del firmware y del proyecto de MPLAB X.
- **T-05 retirada (2026-10-21):** el LED de RA5.
- **T-09** (ADC y envío ASCII; implementada, sin medir): `adc_iq.c/.h` (Timer0 dispara el ADC cada 1 ms; I en RC0, Q en RC1; sin promedio) y `adc_ascii.c/.h` (antes `adc_prueba.*`: una línea `xxxx,xxxx\r\n` cada `ADC_PARES_POR_LINEA` = 2 pares = 500 líneas/s). `main.c` lee cada par con `adc_iq_get()` y lo pasa a `pwm_desde_adc()` y `adc_ascii_par()`; no hay `MODO_PRUEBA_ADC` ni `MODO_ECO`. RC2 saca la salida del Timer0 (`ADC_PIN_PRUEBA` = 2). `nbproject/` registrado con `adc_iq.c`, `adc_ascii.c` y `pwm.c`. Compila con XC8 2.40 junto con T-06 (2026-11-04, 0 errores); sin probar en el chip con 500 líneas/s.
- **T-06** (PWM de reconstrucción; implementada, sin probar): `pwm.c/.h` con PWM6 en RA2 (I) y PWM7 en RA4 (Q), Timer2 a Fosc/4 con `T2PR` = 255 (15,625 kHz, 10 bits); `main.c` llama a `pwm_desde_adc(i, q)` con cada par (`duty = (código + 2) >> 2`) y luego a `adc_ascii_par(i, q)`. `pwm.c` agregado a `configurations.xml` y `Makefile-default.mk`. Compilado con XC8 2.40 (2026-11-04, 0 errores; nombres de registros comprobados en el `.h`); sin probar en el chip. Filtro de reconstrucción sin montar (propuesta: Sallen-Key 22 kΩ / 100 nF / 47 nF ≈ 105 Hz).
- **Filtros:** no hay filtro anti-aliasing en esta entrega (indicación de la persona, 2026-11-05); la prueba 4.7 queda pendiente.
- **PC:** `herramientas/interfaz_micro3.py` (SINAD, ENOB, ruido, histograma, plano I-Q) y `herramientas/receptor_micro3.py` (gráfica y CSV) leen las líneas ASCII; probados con su simulador. Guía del VI de LabVIEW: `docs/guia_labview_paso_a_paso.md`.

## Sin confirmar

1. Baud pedido por el profesor: 115200 (lo escogido) o 125 000 (lo que dejó el MCC de clase).
2. Que el chip sea `PIC16F18426` y no el `LF` (máximo 3,6 V), y que el PIC se alimente de los 3,7 V.
3. Marcado y pines del módulo CP2102 (jumper de nivel en 3,3 V; TXD en reposo ≈3,3 V con el multímetro).
4. Que los pines propuestos coincidan con la placa.
5. Cómo se mide el requisito **SINAD > 60 dB** (ADC con senoidal casi a escala completa, lo adoptado, o señal real de ±185 mV, donde un 12 bits ideal da 54 dB) y a qué frecuencias. De dónde salen los 352 µV del profesor (son 12 bits y 5 V; con 3,7 V son 261 µV). Otras dudas de la clase (jitter < 10 ns y si aplica a Q, «modular y ver los 10 Hz»; la precisión del auto-cero pasa a Macro): `docs/especificaciones_preinforme.md` §10.2.
6. Unidad de `ADACQ` = 160: 10 µs (lo que guarda MCC), 40 µs (Fig. 32-2 del datasheet) o 320 µs (texto del registro). Se mide con RC2 (prueba 4.16); la fórmula está en `docs/especificacion_adc.md` §3.1.
7. Modelo e impedancia de salida (`Zout`) del opa de I y Q de PCB2 (el datasheet del MCP604 ya está en `docs/MCP601-2.PDF` y no da `Zout`; `.gitignore` ignora `docs/*.pdf`) y si `Vref` queda en `VDD` o pasa a FVR o externa: `docs/especificacion_adc.md` §3.2 y §7.1.

## Siguiente paso

1. La persona: abrir `firmware/Micro_Firmware.X` en MPLAB X (se borraron `crc8.*` y `protocol.*`, y `adc_prueba.*` pasó a `adc_ascii.*`: si MPLAB los sigue mostrando, cerrar y volver a abrir el proyecto), *Clean and Build* (anotar los warnings), programar con la SNAP.
2. Pruebas del Módulo 2 (envío ASCII, 2.1 a 2.6) y del Módulo 4 (ADC: 4.3 ruido, 4.5 periodo con RC2, 4.15 SINAD, 4.16 `ADACQ`) de `docs/protocolos_prueba.md`, con `herramientas/interfaz_micro3.py`. Anotar los resultados aquí.
3. Decidir con el profesor: si acepta el envío ASCII sin nombres a 500 líneas/s (48,1 % de la línea: cumple la prueba 6.6, < 50 %) y el SINAD > 60 dB.
4. T-06: ya revisada y compilada con XC8 2.40 (memoria: programa 1002 de 16384 palabras, datos 204 de 2048 bytes). Falta *Clean and Build* (revisar que `pwm.c` aparezca en *Source Files*), montar el filtro de `docs/especificacion_pwm.md` §6 y correr las pruebas 3.1 a 3.8 del Módulo 3. T-07 y T-08 pasan a Macro.
5. Seguir la revisión del código de MCC (`docs/revision_codigo_mcc.md`), parte por parte.

## Registro de sesiones

| Fecha | Quién | Qué se hizo | Resultado |
|---|---|---|---|
| 2026-10-16 | Chat de Claude | Especificación, criterios, protocolos de prueba y circuito UART | Documentos en `docs/`. Sin código |
| 2026-10-19 | Chat de Claude | Roles Gemini (planificador) y Claude Code (desarrollador): `CLAUDE.md` y `GEMINI.md` cargan `MASTER_PLAN.md`. Versiones MPLAB X 6 y XC8 2.40. Guía para configurar MCC. Formato fijo para explicar código. MCC como base del firmware | Archivos escritos. Sin probar que cada herramienta cargue el contexto |
| 2026-10-19 | Chat de Claude | Revisión de `eusart1.c` regenerado (16 MHz, SP1BRG=34 correctos; buffers 8/8; 9 puntos a no copiar, `docs/guia_mcc_referencia.md` §8). Revisión parte por parte: parte 1 explicada, C-01 a C-04 propuestos; mapa de ventanas de MCC a archivos. Instrucciones `docs/instrucciones_claude_code_uart.md`; T-03 aprobada (C-06 a C-10) | Solo lectura, sin compilar. Falta C-05 en MCC y la implementación |
| 2026-10-19 | Chat de Claude | Pruebas UART con Python: 1.5 (20/20), 1.6 (256/256), 1.7 (1000/1000, 141 ms) pasaron; 1.18 y 0.3 pasaron (ID `0x30D2`, revisión `0x2002`). Detalle en `docs/resultados_pruebas_uart.md`. La persona aprobó C-01, C-02 y C-04 (watchdog apagado) y rechazó C-03; T-02 reducida y aprobada | Faltan 0.1, 1.4 y 1.17 (la persona los difiere). T-03 aún `implementada, sin probar` en 1.9, 1.13 y 1.14 |
| 2026-10-19 | Claude Code | T-03 en 5 pasos revisados uno a uno: `eusart1.c/.h`, `interrupt_manager.c`, `main.c` de eco (C-06 a C-10); buffers 64/32 ya presentes; archivos en ISO-8859-1 con CRLF. T-02 reducida: `device_config.c` con `PPS1WAY = OFF` y `FCMEN = OFF` (C-01, C-02); nada más cambió | Las dos `implementada, sin probar`. XC8 2.40: sin errores, 6 avisos (520); el `.hex` muestra los dos bits de T-02 en 0. Sin MPLAB X ni chip. Sin commit |
| 2026-10-19 y 21 | Claude Code | T-05 en 4 pasos (RA5 como salida, Timer4 en `heartbeat.c/.h`, rama de TMR4 en `interrupt_manager.c`, llamada en `main.c`; datos en `notas_pic16f18426.md` §3 y §9.3). El 2026-10-21 se retiró todo: archivos borrados y referencias quitadas de `main.c`, `interrupt_manager.c`, `pin_manager.c`, `configurations.xml` y `Makefile-default.mk` | Antes de retirar: `make` sin errores, 6 avisos (520), `T4CON` 0xF4, `T4CLKCON` 0x01, `T4PR` 124. Después: compila con XC8 2.40 (6 avisos 520). Sin MPLAB X ni chip. Sin commit |
| 2026-10-21 | Chat de Claude | Especificaciones `docs/especificacion_pwm.md` y `docs/especificacion_protocolo.md` (pines, eventos, tareas, pruebas). Tareas T-04 (aprobada), T-06, T-07 y T-08 (propuestas) y cambios CP-1 a CP-5 en `MASTER_PLAN.md`. Decisiones de la persona anotadas en `criterios_diseno.md` §10 | Solo documentos, sin código. Faltan en el chip las pruebas de T-02 y T-03 |
| 2026-10-21 | Claude Code | T-04: protocolo de tramas binarias en 5 pasos (retirado el 2026-11-04) | Compiló con XC8 2.40 sin avisos nuevos; nunca se probó en el chip |
| 2026-10-30 | Chat de Claude | Zip del ADC fusionado en `firmware/Micro_Firmware.X` (T-09). Nuevo `docs/especificacion_adc.md` (requisitos, fuente de disparo, fórmula de `ADACQ`, impedancia del opa, origen de `Vref`) y `docs/especificaciones_preinforme.md`. Requisito **SINAD del ADC > 60 dB** en `MASTER_PLAN.md`, `especificacion_diseno_uart.md` y `criterios_diseno.md`; pruebas 4.10 a 4.18 en `protocolos_prueba.md`; C-11, C-12 en `revision_codigo_mcc.md`; datos del ADCC en `notas_pic16f18426.md` §9.2 | Documentos y firmware. Sin compilar en MPLAB X ni medir. Sin commit |
| 2026-11-03 | Claude (cowork) | Se quitó el promedio de 5 del ADC a petición de la persona (reemplaza D-1): `ADC_N_PROMEDIO` eliminado de `adc_iq.h`; la ISR de `adc_iq.c` entrega cada par (I, Q) sin sumar y `adc_iq_get` ya no divide; `adc_prueba.c/.h` cuenta pares directamente (sigue 1 línea cada 50 pares); comentarios de `main.c`. Archivos en Windows-1252 con CRLF | Revisión de sintaxis con gcc y cabeceras simuladas: sin errores. Sin compilar con XC8 ni probar en el chip. Docs actualizados en la misma sesión: `MASTER_PLAN.md`, `criterios_diseno.md`, `especificacion_adc.md`, `especificacion_protocolo.md`, `especificacion_diseno_uart.md`, `protocolos_prueba.md` y `especificaciones_preinforme.md`. La prueba 6.6 (< 50 % de la línea) ya no se cumple: queda para decidir |
| 2026-11-04 | Claude (cowork) | `ADC_PIN_PRUEBA` en `adc_iq.h` pasa a tener 3 valores: 0 = nada, 1 = pulso de la ISR en RC2 (lo de antes), **2 = salida del Timer0 en RC2 por PPS** (`RC2PPS = 0x17`, onda cuadrada de 500 Hz: cada flanco es un disparo del ADC), que queda puesto. `PRUEBA_PARES_POR_LINEA` de 50 a 2 (500 líneas ASCII/s, para medir SINAD en modo texto). `herramientas/interfaz_micro3.py` calcula fs sola | Revisión de sintaxis con gcc y cabeceras simuladas (valores 0, 1 y 2): sin errores. Sin compilar con XC8 ni probar. El código PPS 0x17 sale de MCC del PIC16F18446, falta confirmarlo en el datasheet |
| 2026-11-04 | Claude (cowork) | **Solo ASCII** (decisión de la persona): se borran `crc8.c/.h` y `protocol.c/.h`; `adc_prueba.c/.h` pasa a `adc_ascii.c/.h` (`adc_ascii_task`, `ADC_PARES_POR_LINEA` = 2, `adcLineasPerdidas`); `main.c` sin modos, solo `adc_ascii_task()`; `configurations.xml` y `Makefile-default.mk` actualizados. Python (`interfaz_micro3.py`, `receptor_micro3.py`) solo lee ASCII; se borran `prueba_protocolo.py`, `crc8_vectores.py` y `prueba_uart.py`. Docs actualizados (`MASTER_PLAN.md`, `criterios_diseno.md` §5 y §10, `especificacion_protocolo.md` reescrito para el formato ASCII, `especificacion_diseno_uart.md`, `especificacion_adc.md`, `protocolos_prueba.md` Módulo 2 nuevo, `especificaciones_preinforme.md`, `contexto_uart_tx_rx.md`, guía de LabVIEW para ASCII y otros); se borran `VI_decodificador_trama_9B.md` y `prompt_claude_code_t04.md` | gcc con cabeceras simuladas: sin errores; `a_decimal` probado en el PC. Interfaz y receptor probados con el simulador (SINAD 66,6 dB a 500 Hz). Sin compilar con XC8 ni medir en el chip |
| 2026-11-04 | Claude (cowork) | La línea pierde los nombres a petición de la persona: `adc_ascii.c` manda `2048,2051\r\n` (I, coma, Q; 11 bytes en vez de 15). Con 500 líneas/s la UART va al 48,1 % (antes 65,6 %): cumple la prueba 6.6. Python (`interfaz_micro3.py`, `receptor_micro3.py`) lee `número,número` al inicio de la línea; guía de LabVIEW con *Scan From String* `%d,%d`; docs, diagramas e informe actualizados | gcc con cabeceras simuladas: sin errores; la línea armada se probó en el PC. Interfaz probada con el simulador (SINAD 66,6 dB). Sin compilar con XC8 ni probar en el chip |
| 2026-11-04 | Claude (cowork) | **Cambio de enfoque del profesor:** el auto-cero pasa a Macro (`docs/macro_autocero_pwm.md`; notas [MACRO] en `criterios_filtro_pwm.md` y `guia_filtro_pwm_dac.md`). T-06 reescrita e implementada: `pwm.c/.h`, PWM6/RA2 = I y PWM7/RA4 = Q con Timer2 (`T2PR` = 255); `adc_ascii_task` pasa a `adc_ascii_par(i, q)` y `main` reparte cada par al PWM y a la UART. Nuevo `docs/especificacion_pwm.md` (filtro de reconstrucción ≈ 100 Hz, SINAD simulado ≈ 54 dB a 20 Hz); Módulo 3 de pruebas rehecho; referencias al auto-cero marcadas como Macro en `MASTER_PLAN.md`, `criterios_diseno.md`, `especificacion_diseno_uart.md`, `especificacion_adc.md`, `notas_pic16f18426.md`, `especificaciones_preinforme.md` y el enunciado | Revisión con gcc y cabeceras simuladas: sin errores. Sin XC8 ni chip. Sin commit |
| 2026-11-04 | Claude (cowork) | Revisión de los `.md` para que Claude Code pueda hacer T-06 solo con ellos: `especificacion_pwm.md` §5 con archivos, firmas, escrituras de `pwm_init` en orden y `main`; `especificacion_protocolo.md` §4 con `adc_ascii_par`; quitadas las menciones a `adc_ascii_task` en 10 documentos; prompt en `docs/prompt_claude_code_t06.md` | Solo documentos |
| 2026-11-04 | Claude Code | T-06 revisada contra `especificacion_pwm.md` §5 y `especificacion_protocolo.md` §4: `pwm.c/.h`, `main.c` y `adc_ascii.*` ya cumplían, sin cambios. Nombres de registros y bits comprobados en `pic16f18426.h` (`notas` §9.1). Borrados del disco `crc8.*`, `protocol.*` y `adc_prueba.*` (seguían allí aunque no estaban en `nbproject/`) | XC8 2.40 (-O2): 0 errores, solo avisos (520) de funciones de MCC sin usar; programa 1002/16384 palabras (6,1 %), datos 204/2048 bytes (10 %). Sin chip. Sin commit |
| 2026-11-05 | Claude (cowork) | Nuevo `docs/documento_tecnico_micro3.md`: especificaciones, decisiones, fórmulas, flujo del código, cada función, cambios C-01 a C-12 frente a MCC y 54 preguntas de evaluación. Hallazgo: `pin_manager.c` deja RC5 en Schmitt (`INLVLC = 0x3F`) aunque el diseño decía TTL; corregido en los `.md`. Figura 18-1 del DS (p.260): al ADC le llega el pulso de cada coincidencia del Timer0 (1 kHz) | Solo documentos |
| 2026-11-05 | Claude (cowork) | Revisión de coherencia de todos los `.md` (`MASTER_PLAN.md`, `docs/` y `docs/diagramas_tiempo/`) contra el código, **sin tocar el código**: tabla «Qué tiene esta entrega» en `MASTER_PLAN.md`; sin filtro anti-aliasing; ADC a 1 kHz por canal, PWM con cada par, UART 500 líneas/s; quitadas menciones viejas (LED de RA5, «sin MCC», `fs` de 200 Hz, 15 bytes por línea, RC2 como pulso de la ISR, T-07 «por replantear», «sin compilar con XC8») | Solo documentos |
| 2026-11-05 | Claude (cowork) | MCC queda como base del firmware en todos los `.md`: quitadas las menciones a que no se puede usar (enunciado §2, `MASTER_PLAN.md`, `documento_tecnico_micro3.md` R14 y pregunta 46, `estado.md`, `especificaciones_preinforme.md`, `criterios_diseno.md`, `especificacion_diseno_uart.md`, `guia_mcc_referencia.md`, `macro_autocero_pwm.md`); T-01 descartada | Solo documentos |
| 2026-11-04 | Claude Code | Corrección: la persona vio `No rule to make target 'crc8.c'` al compilar en MPLAB X. `nbproject/` seguía con `crc8`, `protocol` y `adc_prueba` y **sin** `pwm.c` ni `adc_ascii.c` (mi revisión anterior dijo lo contrario por error). `configurations.xml` y `Makefile-default.mk` corregidos: fuentes `adc_iq.c adc_ascii.c pwm.c`, con sus reglas de compilación | XML válido; sin referencias viejas. `make` no está en esta máquina: la compilación con XC8 fue directa (0 errores). La persona debe repetir *Clean and Build* |
