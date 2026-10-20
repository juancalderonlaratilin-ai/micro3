# Estado del proyecto (bitácora de avance)

Proyecto Micro 3 · Última actualización: 2026-10-19

> **Para qué sirve:** es lo primero que se lee al abrir una sesión nueva (Claude Code lo carga desde `CLAUDE.md`; en el chat de Claude se le pide "lee `docs/estado.md`"). Dice **dónde quedó el trabajo y qué sigue**. Las decisiones y sus motivos viven en `criterios_diseno.md`, no aquí.
>
> **Reglas para actualizarlo (quien trabaje: Claude Code, el chat o tú):**
> 1. Al terminar cada tarea: marcar lo hecho, reescribir "Siguiente paso" y agregar una fila al registro.
> 2. Mantenerlo corto (menos de 80 líneas). Lo viejo se resume o se borra.
> 3. Solo hechos comprobados. Si algo no se probó, escribir "sin probar".
> 4. No copiar decisiones de diseño: enlazar al documento donde están.

---

## Fase actual

**Fase 1:** enlace UART PC ↔ PIC (binario, sin ADC ni PWM todavía).

## Hecho

- [x] Especificación de diseño de la UART: `docs/especificacion_diseno_uart.md`
- [x] Criterios de diseño y valores escogidos: `docs/criterios_diseno.md`
- [x] Notas verificadas del PIC16F18426 con páginas: `docs/notas_pic16f18426.md`
- [x] Protocolos de prueba por módulo: `docs/protocolos_prueba.md`
- [x] Circuito UART con el módulo CP2102 (divisor 1 kΩ + 2,2 kΩ en el TX): `docs/circuito_uart.svg`
- [x] Proyecto MPLAB X creado en `firmware/Micro_Firmware.X`
- [x] Repositorio git local (rama `master`)
- [x] Guía para configurar MCC como referencia: `docs/guia_mcc_referencia.md`
- [ ] Revisión guiada del código de MCC (partes 1 y 6 de 10 explicadas): `docs/revision_codigo_mcc.md`
- [x] Roles definidos: Gemini planifica y Claude Code desarrolla (`MASTER_PLAN.md`, `CLAUDE.md` y `GEMINI.md`)
- [ ] Repositorio subido a GitHub

## Estado del código

En `firmware/Micro_Firmware.X` hay código **generado por MCC en clase**, que la persona indica que sí puede usarse como base (el enunciado dice que no: confirmar con el profesor). Se revisa en `docs/revision_codigo_mcc.md`.

**T-03 implementada, sin probar** (cambios C-06 a C-10; C-05, buffers 64/32, ya estaba en `eusart1.c`):
- `eusart1.c` / `eusart1.h`: buffers circulares con cabeza y cola (sin contadores), `EUSART1_Write` y `EUSART1_Read` sin espera (devuelven `bool`), `tx_DAT` (todo o nada) y `tx_string`, ISR de TX y de RX sin contador, `is_tx_done` con `TRMT`.
- `interrupt_manager.c`: llama directo a `EUSART1_Transmit_ISR()` y `EUSART1_Receive_ISR()`.
- `main.c`: eco con `PEIE`/`GIE` y `tx_string("Micro3 listo\r\n")` al arrancar.
- Agregados que la instrucción no pedía: `#error` si un tamaño de buffer no es potencia de 2, `EUSART1_get_overflow()` (devuelve `bool`) y el contador `echoBytesLost` en `main.c`.
- Compilación de prueba con XC8 2.40 por línea de comandos (salida fuera del proyecto): sin errores; 6 avisos `(520)` de funciones sin llamar (5 de `eusart1.c` y el de `PIN_MANAGER_IOC`, que ya existía). Sin compilar todavía en MPLAB X.

## Sin confirmar

1. Baud pedido por el profesor: 115200 (lo escogido) o 125 000 (lo que dejó el MCC de clase).
2. Si el entregable final puede usar MCC: la persona dice que sí; el enunciado dice que no. Confirmarlo con el profesor.
3. Que el chip sea `PIC16F18426` y no el `LF` (máximo 3,6 V), y que el PIC se alimente de los 3,7 V.
4. Marcado y pines del módulo CP2102 (jumper de nivel en 3,3 V; TXD en reposo ≈3,3 V con el multímetro).
5. Que los pines propuestos coincidan con la placa.

## Siguiente paso

1. La persona: *Clean and Build* en MPLAB X (anotar los warnings), programar con el SNAP y correr las pruebas 1.5, 1.6, 1.7, 1.9, 1.13 y 1.14 de `docs/protocolos_prueba.md` (guía: `docs/guia_prueba_uart_snap.md`). El resultado decide si T-03 pasa a `probada` o `bloqueada`.
2. Decidir (planificador/persona): si se acepta que queden avisos `(520)` de funciones sin llamar (el criterio de T-03 dice "sin warnings"), y cómo se hacen las pruebas 1.9 y 1.14, que el eco no cubre (no llama a `tx_DAT` y `main` siempre lee el RX).
3. Seguir la revisión del código de MCC (`docs/revision_codigo_mcc.md`), parte por parte. T-01 (mover MCC a `referencia_mcc/`) queda en espera.
4. T-02 (aprobada, reducida): Claude Code cambia en `device_config.c` `PPS1WAY = OFF` y `FCMEN = OFF`. Sin `config.c` nuevo; watchdog ya OFF.

## Registro de sesiones

| Fecha | Quién | Qué se hizo | Resultado |
|---|---|---|---|
| 2026-10-16 | Chat de Claude | Especificación, criterios, protocolos de prueba y circuito UART | Documentos en `docs/`. Sin código |
| 2026-10-19 | Chat de Claude | Roles Gemini (planificador) y Claude Code (desarrollador). `CLAUDE.md` y `GEMINI.md` cargan `MASTER_PLAN.md`; se agregaron las secciones de roles y tareas | Archivos escritos. Sin probar que cada herramienta cargue el contexto |
| 2026-10-19 | Chat de Claude | Versiones de herramientas en `MASTER_PLAN.md`: MPLAB X 6 y XC8 2.40. Guía para configurar MCC como código de referencia | Documentos actualizados |
| 2026-10-19 | Chat de Claude | Revisión de `eusart1.c` regenerado (16 MHz, SP1BRG=34 correctos; buffers 8/8; 9 puntos a no copiar) en `docs/guia_mcc_referencia.md` §8 | Solo lectura, sin compilar |
| 2026-10-19 | Chat de Claude | Revisión del código de MCC parte por parte; parte 1 (bits de configuración) explicada; cambios C-01 a C-04 propuestos | Sin aprobar |
| 2026-10-19 | Chat de Claude | Formato fijo para explicar código en `MASTER_PLAN.md`; mapa de ventanas de MCC a archivos; MCC permitido según la persona; T-01 en espera | Pendiente confirmar con el profesor |
| 2026-10-19 | Chat de Claude | Instrucciones para Claude Code en `docs/instrucciones_claude_code_uart.md`; T-03 aprobada (C-06 a C-10) | Falta C-05 en MCC y la implementación |
| 2026-10-19 | Chat de Claude | Pruebas UART con Python: 1.5 (20/20), 1.6 (256/256), 1.7 (1000/1000, 141 ms) pasaron; 1.18 y 0.3 pasaron (ID `0x30D2`, revisión `0x2002`). Detalle en `docs/resultados_pruebas_uart.md`. La persona aprobó C-01, C-02 y C-04 (watchdog apagado) y rechazó C-03; T-02 reducida y aprobada | Faltan 0.1, 1.4 y 1.17 (la persona los difiere). T-03 aún `implementada, sin probar` en 1.9, 1.13 y 1.14 |
| 2026-10-19 | Claude Code | T-03 en 5 pasos revisados uno a uno: `eusart1.c/.h`, `interrupt_manager.c`, `main.c` de eco (C-06 a C-10). Buffers 64/32 ya presentes. Archivos en ISO-8859-1 con CRLF | `implementada, sin probar`. Compilación de prueba XC8 2.40: sin errores, 6 avisos (520). Sin MPLAB X ni chip. Sin commit |
