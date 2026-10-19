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
- [ ] Revisión guiada del código de MCC (parte 1 de 10 explicada): `docs/revision_codigo_mcc.md`
- [x] Roles definidos: Gemini planifica y Claude Code desarrolla (`MASTER_PLAN.md`, `CLAUDE.md` y `GEMINI.md`)
- [ ] Repositorio subido a GitHub

## Estado del código

Sin código propio todavía. En `firmware/Micro_Firmware.X` hay código **generado por MCC en clase**, que no es parte del firmware (regla: sin MCC). Pendiente moverlo a `referencia_mcc/`.

## Sin confirmar

1. Baud pedido por el profesor: 115200 (lo escogido) o 125 000 (lo que dejó el MCC de clase).
2. Si el entregable final puede usar MCC.
3. Que el chip sea `PIC16F18426` y no el `LF` (máximo 3,6 V), y que el PIC se alimente de los 3,7 V.
4. Marcado y pines del módulo CP2102 (jumper de nivel en 3,3 V; TXD en reposo ≈3,3 V con el multímetro).
5. Que los pines propuestos coincidan con la placa.

## Siguiente paso

1. Mover el código de MCC a `referencia_mcc/` y dejar `main.c` vacío. Compilar en MPLAB X para comprobar que abre.
2. Escribir `config.c` (bits de configuración) y `system_init()` con el reloj a 16 MHz.
3. Escribir `uart.c` y `uart.h` (buffers circulares, una sola ISR, `tx_DAT` y `tx_string`) y el eco de `main.c`.
4. Probar con las pruebas 1.5 y 1.6 de `docs/protocolos_prueba.md`.

## Registro de sesiones

| Fecha | Quién | Qué se hizo | Resultado |
|---|---|---|---|
| 2026-10-16 | Chat de Claude | Especificación, criterios, protocolos de prueba y circuito UART | Documentos en `docs/`. Sin código |
| 2026-10-19 | Chat de Claude | Roles Gemini (planificador) y Claude Code (desarrollador). `CLAUDE.md` y `GEMINI.md` cargan `MASTER_PLAN.md`; se agregaron las secciones de roles y tareas | Archivos escritos. Sin probar que cada herramienta cargue el contexto |
| 2026-10-19 | Chat de Claude | Versiones de herramientas en `MASTER_PLAN.md`: MPLAB X 6 y XC8 2.40. Guía para configurar MCC como código de referencia | Documentos actualizados |
| 2026-10-19 | Chat de Claude | Revisión de `eusart1.c` regenerado (16 MHz, SP1BRG=34 correctos; buffers 8/8; 9 puntos a no copiar) en `docs/guia_mcc_referencia.md` §8 | Solo lectura, sin compilar |
| 2026-10-19 | Chat de Claude | Revisión del código de MCC parte por parte; parte 1 (bits de configuración) explicada; cambios C-01 a C-04 propuestos | Sin aprobar |
