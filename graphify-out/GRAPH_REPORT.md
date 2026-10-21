# Graph Report - micro3  (2026-10-21)

## Corpus Check
- Corpus is ~38,622 words - fits in a single context window. You may not need a graph.

## Summary
- 210 nodes · 284 edges · 19 communities (14 shown, 5 thin omitted)
- Extraction: 86% EXTRACTED · 14% INFERRED · 0% AMBIGUOUS · INFERRED: 40 edges (avg confidence: 0.86)
- Token cost: 0 input · 0 output

## Community Hubs (Navigation)
- Notas del chip y MCC
- Especificación UART y criterios
- Código heartbeat e ISR
- Código EUSART1
- Revisión eusart1 y T-03
- Circuito CP2102 (PNG)
- Pruebas Python UART
- Preinforme analógico I/Q
- Guías y protocolos de prueba
- Trama binaria y CRC-8
- Reglas del proyecto
- Pendientes y uso de MCC
- Requisitos del enunciado
- Config MCP filesystem
- Niveles lógicos UART
- Circuito UART (SVG)
- Formato de explicación
- Herramientas

## God Nodes (most connected - your core abstractions)
1. `Notas del PIC16F18426 (resumen verificado)` - 12 edges
2. `Protocolos de prueba y medición por módulo` - 9 edges
3. `PIC16F18426 (VDD pin 1, VSS pin 14)` - 8 edges
4. `main()` - 7 edges
5. `Estado del proyecto (bitacora)` - 7 edges
6. `Buffer circular sin contador (cabeza/cola, máscara potencia de 2)` - 7 edges
7. `Tarea T-03 (C-05 a C-10)` - 7 edges
8. `main()` - 6 edges
9. `SYSTEM_Initialize()` - 6 edges
10. `MASTER_PLAN (fuente de la verdad)` - 6 edges

## Surprising Connections (you probably didn't know these)
- `Uso de MCC (permitido segun la persona, prohibido segun el enunciado)` --conceptually_related_to--> `Enunciado Practica 3 (corrientes de Foucault)`  [AMBIGUOUS]
  MASTER_PLAN.md → docs/Especificacion_Tecnica_Practica3_Microcontroladores.md
- `main()` --calls--> `Heartbeat_Initialize()`  [INFERRED]
  firmware/Micro_Firmware.X/main.c → firmware/Micro_Firmware.X/heartbeat.c
- `MASTER_PLAN (fuente de la verdad)` --references--> `Enunciado Practica 3 (corrientes de Foucault)`  [EXTRACTED]
  MASTER_PLAN.md → docs/Especificacion_Tecnica_Practica3_Microcontroladores.md
- `T-02 Bits de configuracion PPS1WAY y FCMEN` --implements--> `Bits de configuracion propuestos (LVP, WDTE, PPS1WAY)`  [INFERRED]
  MASTER_PLAN.md → docs/especificacion_diseno_uart.md
- `Estado del proyecto (bitacora)` --references--> `T-02 Bits de configuracion PPS1WAY y FCMEN`  [EXTRACTED]
  docs/estado.md → MASTER_PLAN.md

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **Flujo de trabajo Gemini - Claude Code - persona** — gemini_rol_planificador, claude_rol_claude_code, master_plan_ciclo_tareas [EXTRACTED 1.00]
- **Diseno UART no bloqueante: ISR unica, FIFO y firmas tx** — docs_especificacion_diseno_uart_isr, docs_especificacion_diseno_uart_fifo, docs_especificacion_tecnica_practica3_microcontroladores_firmas_tx, master_plan_t03 [INFERRED 0.85]
- **Decisiones del enlace: reloj 16 MHz, baud 115200, 8N1, protocolo binario** — docs_criterios_diseno_reloj_16mhz, docs_criterios_diseno_baud_115200, docs_criterios_diseno_formato_8n1, docs_criterios_diseno_protocolo_binario [EXTRACTED 1.00]
- **Cambios de T-03 a la UART de MCC (C-05 a C-10)** — docs_revision_codigo_mcc_c05_buffers, docs_revision_codigo_mcc_c06_punteros_handlers, docs_revision_codigo_mcc_c07_read, docs_revision_codigo_mcc_c08_write, docs_revision_codigo_mcc_c09_sin_contadores, docs_revision_codigo_mcc_c10_tx_done [EXTRACTED 1.00]
- **Funciones que comparten los buffers circulares TX/RX** — docs_instrucciones_claude_code_uart_eusart1_write, docs_instrucciones_claude_code_uart_eusart1_read, docs_instrucciones_claude_code_uart_eusart1_transmit_isr, docs_instrucciones_claude_code_uart_eusart1_rxdatahandler, docs_instrucciones_claude_code_uart_tx_dat [EXTRACTED 1.00]
- **Módulos de prueba secuenciales (0 a 6)** — docs_protocolos_prueba_modulo0, docs_protocolos_prueba_modulo1_uart, docs_protocolos_prueba_modulo2_protocolo, docs_protocolos_prueba_modulo3_pwm, docs_protocolos_prueba_modulo4_adc, docs_protocolos_prueba_modulo5_autocero, docs_protocolos_prueba_modulo6_integracion [EXTRACTED 1.00]
- **Cadena analogica I/Q** — docs_preinforme_practica_2_final_sofia_aguilar_sebastian_calderon_samuel_martinez_pcb1, docs_preinforme_practica_2_final_sofia_aguilar_sebastian_calderon_samuel_martinez_iq_demod, docs_preinforme_practica_2_final_sofia_aguilar_sebastian_calderon_samuel_martinez_lpf, docs_preinforme_practica_2_final_sofia_aguilar_sebastian_calderon_samuel_martinez_zero_ref [EXTRACTED 0.95]
- **Divisor de nivel TX del PIC al adaptador** — docs_circuito_uart_rc4_tx1, docs_circuito_uart_r1_1k, docs_circuito_uart_r2_2k2, docs_circuito_uart_cp2102_module [EXTRACTED 1.00]
- **TX path: RC4/TX1 through R1/R2 divider to adapter RXD** — docs_circuito_uart_rc4_tx1, docs_circuito_uart_r1_1k, docs_circuito_uart_r2_2k2, docs_circuito_uart_cp2102 [EXTRACTED 1.00]

## Communities (19 total, 5 thin omitted)

### Community 0 - "Notas del chip y MCC"
Cohesion: 0.09
Nodes (30): Guía: configurar MCC como código de referencia, Diferencias código MCC vs nuestro diseño, Notas del PIC16F18426 (resumen verificado), ADCC 12 bits (Fase 2, pendiente de leer), Bits de configuración en XC8 (LVP=1, WDTE off, PPS1WAY), Errata aplicable (ER) y revisión de silicio, EUSART1 baud 115200: SP1BRG=34, error -0,79 %, Umbrales Schmitt vs TTL a VDD 3,7 V (INLVLx) (+22 more)

### Community 1 - "Especificación UART y criterios"
Cohesion: 0.11
Nodes (24): Claude Code: desarrollador principal, Baud 115200 (SP1BRG=34, error -0,79 %), Buffers circulares RX 32 / TX 64, Reloj HFINTOSC 16 MHz, Bits de configuracion propuestos (LVP, WDTE, PPS1WAY), Especificacion de diseno del enlace UART (Fase 1), Eventos E1-E7 (arquitectura guiada por eventos), Estructuras FIFO (head/tail, mascara) (+16 more)

### Community 2 - "Código heartbeat e ISR"
Cohesion: 0.12
Nodes (8): Heartbeat_Initialize(), Heartbeat_ISR(), EUSART1_Transmit_ISR(), __interrupt(), OSCILLATOR_Initialize(), PMD_Initialize(), SYSTEM_Initialize(), PIN_MANAGER_Initialize()

### Community 3 - "Código EUSART1"
Cohesion: 0.13
Nodes (13): main(), EUSART1_DefaultErrorHandler(), EUSART1_get_last_status(), EUSART1_Initialize(), EUSART1_Read(), EUSART1_Receive_ISR(), EUSART1_RxDataHandler(), EUSART1_SetErrorHandler() (+5 more)

### Community 4 - "Revisión eusart1 y T-03"
Cohesion: 0.15
Nodes (19): Revisión de eusart1.c regenerado (9 puntos a no copiar), Buffer circular sin contador (cabeza/cola, máscara potencia de 2), EUSART1_Read (no bloqueante), EUSART1_RxDataHandler (descarta si lleno), EUSART1_Transmit_ISR, EUSART1_Write (no bloqueante), interrupt_manager.c llama directo a las ISR (C-06), main.c de eco para pruebas (+11 more)

### Community 5 - "Circuito CP2102 (PNG)"
Cohesion: 0.18
Nodes (17): Capacitor de desacople 100 nF (pines 1 y 14), GND comun (fuente, PIC y adaptador), Modulo USB-TTL CP2102, Modulo USB-TTL CP2102 (TXD, RXD, GND; 3V3/5V/RTS/CTS sin conectar), Condensador de desacople 100 nF (junto a pines 1 y 14), Divisor resistivo 1k + 2,2k (2,54 V al adaptador), GND comun (fuente, PIC y adaptador), Pull-up 10 kOhm de MCLR (RA3, al SNAP) (+9 more)

### Community 6 - "Pruebas Python UART"
Cohesion: 0.19
Nodes (6): abrir(), main(), prueba_1_5(), prueba_1_6(), prueba_1_7(), vaciar_entrada()

### Community 7 - "Preinforme analógico I/Q"
Cohesion: 0.15
Nodes (17): Implementacion en Altium (esquematico y ruteo PCB), Auto-cero digital por PWM (proyecto micro3) que reemplaza trimmers I0/Q0, Puente diferencial con bobina activa y de referencia (RBA 100, RBR trimmer 500), Secuencia de puesta en marcha: VCC, Vref2, OSC, clocks, Vexc; RBR; luego I0/Q0, Bobinas anulares medidas: L~196 uH, Rs 8,09-8,59 ohm, desbalance 3,35%, Demodulacion sincrona I/Q con senales cuadradas (LM2903B + TS5A23157), Procesamiento LabVIEW a 10 kHz (DAQ, Hilbert, filtro 20 Hz, G=10), Filtro activo pasa-bajos 2do orden 20 Hz (R 82k, C 100nF, Q~0,709) (+9 more)

### Community 8 - "Guías y protocolos de prueba"
Cohesion: 0.28
Nodes (8): Guía: probar la UART con el SNAP y el PIC, Divisor 1 kΩ + 2,2 kΩ en TX hacia el CP2102, Instrucciones para Claude Code: adaptar EUSART de MCC (T-03), FIFO de 2 bytes del receptor y overrun (CREN 0→1), Prompt base para Claude Code (T-03), Módulo 1: pruebas UART (1.1 a 1.18), Revisión guiada del código generado por MCC, Ruta de revisión en 10 partes

### Community 9 - "Trama binaria y CRC-8"
Cohesion: 0.29
Nodes (7): Formato de byte 8N1, Protocolo binario de tramas de 9 bytes con CRC-8, Fase 1a: eco crudo, Trama binaria de 9 bytes (A5 5A, TYPE, SEQ, PAYLOAD, CRC8), Interfaz LabVIEW: plano de impedancia I vs Q y diagnostico, Trama ASCII sugerida por el enunciado, T-04 Protocolo de tramas con CRC-8

### Community 10 - "Reglas del proyecto"
Cohesion: 0.40
Nodes (4): Cambios y correcciones (PWM6/PWM7, LVP, RX1DTPPS), PWM6/PWM7 para auto-cero, Una sola ISR (vector unico), Reglas del proyecto

### Community 11 - "Pendientes y uso de MCC"
Cohesion: 0.50
Nodes (4): Pendientes por confirmar (baud del profesor, MCC, chip LF), Puntos sin confirmar (baud, MCC, chip LF, CP2102, pines), Uso de MCC (permitido segun la persona, prohibido segun el enunciado), T-01 Mover MCC a referencia_mcc

### Community 12 - "Requisitos del enunciado"
Cohesion: 0.50
Nodes (4): Muestreo ADC y criterio de Nyquist, Modulo de Auto-Cero (PWM como DAC), Maquina de estados INIT/CALIBRATE/RUN/ERROR, Timers y generacion de PWM

### Community 13 - "Config MCP filesystem"
Cohesion: 0.50
Nodes (3): npx, filesystem, @modelcontextprotocol/server-filesystem

## Ambiguous Edges - Review These
- `Uso de MCC (permitido segun la persona, prohibido segun el enunciado)` → `Enunciado Practica 3 (corrientes de Foucault)`  [AMBIGUOUS]
  MASTER_PLAN.md · relation: conceptually_related_to

## Knowledge Gaps
- **39 isolated node(s):** `npx`, `@modelcontextprotocol/server-filesystem`, `Formato fijo para explicar el codigo`, `T-01 Mover MCC a referencia_mcc`, `T-04 Protocolo de tramas con CRC-8` (+34 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 60 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **5 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `Guía: probar la UART con el SNAP y el PIC` connect `Guías y protocolos de prueba` to `Notas del chip y MCC`, `Pruebas Python UART`?**
  _High betweenness centrality (0.032) - this node is a cross-community bridge._
- **What connects `npx`, `@modelcontextprotocol/server-filesystem`, `Formato fijo para explicar el codigo` to the rest of the system?**
  _39 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `Notas del chip y MCC` be split into smaller, more focused modules?**
  _Cohesion score 0.09195402298850575 - nodes in this community are weakly interconnected._
- **Why does `Pruebas 1.5, 1.6, 1.7 pasaron (20/20, 256/256, 1000/1000)` connect `Notas del chip y MCC` to `Revisión eusart1 y T-03`, `Pruebas Python UART`?**
  _High betweenness centrality (0.032) - this node is a cross-community bridge._
- **Should `Especificación UART y criterios` be split into smaller, more focused modules?**
  _Cohesion score 0.11231884057971014 - nodes in this community are weakly interconnected._
- **Why does `Notas del PIC16F18426 (resumen verificado)` connect `Notas del chip y MCC` to `Guías y protocolos de prueba`, `Revisión eusart1 y T-03`?**
  _High betweenness centrality (0.028) - this node is a cross-community bridge._

### Low-confidence Hints
_AMBIGUOUS edges — the extractor was unsure. Verify before acting on these._

- **What is the exact relationship between `Uso de MCC (permitido segun la persona, prohibido segun el enunciado)` and `Enunciado Practica 3 (corrientes de Foucault)`?**
  _Edge tagged AMBIGUOUS (relation: conceptually_related_to) - confidence is low._