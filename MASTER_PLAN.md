# Archivo Maestro de Arquitectura y Tareas (Fuente de la Verdad)
# Micro 3: contexto del proyecto

Instrumento de inspección por corrientes de Foucault. El PIC adquirirá las señales I y Q, generará el auto-cero con PWM y se comunicará con LabVIEW por UART.

## Estructura de la carpeta

- `hardware/`: archivos de Altium (esquemático, PCB, BOM). Son binarios, no se editan desde aquí.
- `firmware/Micro_Firmware.X/`: proyecto de MPLAB X (PIC16F18426, MPLAB X 6, XC8 2.40, programador SNAP).
- `docs/`: especificaciones y documentación. **Empieza por `docs/especificacion_diseno_uart.md`** (qué se va a construir) y consulta **`docs/notas_pic16f18426.md`** (resumen verificado del chip, con páginas) antes de abrir los PDF grandes (datasheet de ~700 páginas, guía de XC8 de ~580).

Criterios de diseño y valores escogidos (reloj, baud, formato, niveles, buffers, protocolo): `docs/criterios_diseno.md`. Circuito UART con el módulo CP2102: `docs/circuito_uart.svg`. El TX del PIC va al adaptador **por un divisor 1 kΩ + 2,2 kΩ**, no directo.

Pruebas de cada módulo (UART primero, luego protocolo, PWM, ADC, auto-cero e integración): `docs/protocolos_prueba.md`. Cada módulo debe cumplir sus criterios antes de pasar al siguiente.

MCC: la persona indica que SÍ puede usarse (2026-10-19). El código generado en clase es la base de trabajo y se revisa parte por parte: `docs/revision_codigo_mcc.md`. Configuración: `docs/guia_mcc_referencia.md`. Ejemplo de nombres: el registro de entrada PPS de RX1 se llama `RX1DTPPS` en el `.h`, aunque el datasheet lo llame `RX1PPS`.

## Herramientas (versiones que usa el equipo)

| Herramienta | Versión |
|---|---|
| MPLAB X IDE | **6** (rama 6.x) |
| Compilador MPLAB XC8 | **2.40** |
| Programador | SNAP |

- Dentro del rango que pide el enunciado (MPLAB X 6.00–6.30, XC8 2.40–2.45).
- Los nombres de registros y bits se comprueban en el `pic16f18426.h` de **esta** instalación (`...\xc8\v2.40\pic\include\proc\`).
- El proyecto `Micro_Firmware.X` ya está guardado con XC8 2.40 (`nbproject/configurations.xml`). El código de MCC dice "probado con XC8 2.36 o superior": 2.40 cumple.
- Si cambia alguna versión, actualizar esta tabla y `docs/especificacion_diseno_uart.md` §2.

## Requerimientos de la Práctica 3 (se cargan en cada sesión)

@docs/Especificacion_Tecnica_Practica3_Microcontroladores.md

Ese archivo es el enunciado de la práctica (qué pide el curso). Donde contradiga las **Reglas del proyecto** de abajo o `docs/especificacion_diseno_uart.md`, mandan estas últimas. Diferencias ya decididas:

- El enunciado sugiere tramas ASCII y 9600/19200 baud. Nosotros usamos **protocolo binario** (recomendación del profesor) a **115200**.
- Su fórmula `SPBRG = Fosc/(16·baud) − 1` es de otras familias; en este chip el baud se calcula con `BRG16=1`, `BRGH=1` (ver `docs/notas_pic16f18426.md`).
- Nombres como `SPBRG`, `TXREG`, `RCSTA`, `CCP1CON` son genéricos; aquí son `SP1BRG`, `TX1REG`, `RC1STA`, etc. Comprobar en `pic16f18426.h`.

## Reglas del proyecto

- Se puede usar MCC como base (lo indica la persona; **el enunciado dice lo contrario, así que la persona lo confirma con el profesor**). Todo código de MCC se revisa y se entiende línea a línea antes de usarse, y lo que se cambie queda en `docs/revision_codigo_mcc.md`. El código que se agregue a mano es C escribiendo directamente en los registros (SFR).
- Sin esperas bloqueantes en la lógica: nada de `while(!TXIF)` ni `__delay_ms` en el programa principal.
- UART por interrupciones con FIFO circular. Protocolo binario, no ASCII.
- Alimentación VCC = 3,7 V (fuente Keysight E36233A, según el preinforme). Conversor USB-serie a 3,3 V, nunca a 5 V.
- Chip fijo: PIC16F18426 (14 pines, 8 usados, 1 de reserva: RC2). La gráfica de impedancia I vs Q se dibuja **en el PC** (LabVIEW o Python); el PIC no maneja pantalla. Presupuesto de pines en `docs/especificacion_diseno_uart.md` §3.1.
- Oscilador HFINTOSC a 16 MHz, UART 8N1 a 115200 baud (SP1BRG = 34).
- Firmas pedidas en clase: `tx_DAT(const char *DATA, uint16_t size)` y `tx_string(const char *data)`.
- **Nunca escribir nombres de registros o bits de memoria.** Comprobarlos en `pic16f18426.h` (carpeta `pic\include\proc\` de la instalación de XC8) o en el datasheet. Cuando algo se verifique, anotarlo en `docs/notas_pic16f18426.md`.
- Una sola ISR (`void __interrupt() isr(void)`, vector único). Comprobar bandera **y** habilitación de cada fuente. `TX1IF` vale 1 siempre que `TX1REG` esté vacío, aunque `TX1IE` sea 0.
- Los pines de `ANSELx` están en modo analógico al reset: bajar el bit de cada pin digital.
- PWM del auto-cero: **PWM6 / PWM7** (no PWM3/PWM4, que no existen en este chip).
- Bits de configuración: `LVP` habilitado (RA3 es MCLR). No usar RA0/RA1/RA3 (programación).

## Registro de avance (se carga en cada sesión)

@docs/estado.md

**Regla:** al terminar cada tarea, actualiza `docs/estado.md`: marca lo hecho, reescribe "Siguiente paso" y agrega una fila al registro (fecha, quién, qué se hizo, resultado). Solo hechos comprobados; si algo no se probó, escribe "sin probar". Las decisiones de diseño van en `docs/criterios_diseno.md`, no ahí. Cuando termines, dile a la persona que haga el commit.

## Estado

- Fase 1 (actual): enlace UART PC ↔ PIC. Sin ADC ni PWM todavía.
- Fase 2: ADC de I/Q, PWM de auto-cero, máquina de estados.

Cuando cambie una decisión de diseño (pines, baud, formato de trama), actualizar `docs/especificacion_diseno_uart.md`.

## Cómo se explica el código (formato fijo)

Para quien aprende el código línea a línea. Claude, Gemini o Claude Code explican **siempre así**, corto y sin repetir el datasheet:

1. **Una parte a la vez.** Se abre con "Parte N de M" y el nombre. No se pasa a la siguiente hasta que la persona diga "entendido".
2. **Abre:** archivo y función (ruta dentro de `firmware/Micro_Firmware.X/`) y, si viene de MCC, en qué módulo de MCC se configura.
3. **Se encarga de:** una frase.
4. **Lista:** tabla `nombre | qué es o significa | valor aquí`, un renglón por línea o registro, en el orden del código. Los bits del mismo registro van en una fila.
5. **Cambiar:** el ID de `docs/revision_codigo_mcc.md`, o "nada".
6. **Compruébalo:** una o dos preguntas.

Reglas: máximo unos 25 renglones por parte. Nombres, valores y páginas salen del datasheet o del `.h`, nunca de memoria; lo no verificado se marca. Sin teoría que esa parte no use.

## Roles y flujo de trabajo

| Quién | Rol | Escribe en |
|---|---|---|
| Gemini | Planificador | los apartados "Tareas" y "Cambios propuestos" de este archivo, y `docs/` |
| Claude Code | Desarrollador principal de código | `firmware/`, `docs/estado.md` y `docs/notas_pic16f18426.md` |
| La persona (equipo) | Decide y aprueba, compila y programa con MPLAB X, mide, hace los commits | todo |
| Chat de Claude (claude.ai) [PROPUESTA] | Verificación técnica contra el datasheet y redacción de documentos (bitácora, informes) | `docs/` |

**Ciclo de cada tarea**

1. Gemini la define en "Tareas" (estado `propuesta`).
2. La persona la revisa y la pasa a `aprobada`.
3. Claude Code la implementa, la marca `implementada, sin probar` y actualiza `docs/estado.md`.
4. La persona compila, programa y ejecuta las pruebas indicadas de `docs/protocolos_prueba.md`, y anota el resultado: `probada` o `bloqueada`.
5. Gemini planifica la siguiente tarea con ese resultado.

**Reglas**

- Este archivo y `docs/` son la única fuente de la verdad. No se copian decisiones en otros sitios.
- Si algo se contradice, manda este orden: el enunciado de la práctica y lo que pida el profesor, luego `docs/criterios_diseno.md` y `docs/especificacion_diseno_uart.md`, luego las tareas de este archivo, y al final lo demás.
- Los cambios a decisiones de diseño se proponen en "Cambios propuestos". Cuando la persona los aprueba, se anotan en `docs/criterios_diseno.md` (§10, con fecha).
- Nadie escribe nombres de registros o bits de memoria: se verifican en `pic16f18426.h` o en `docs/notas_pic16f18426.md`.

## Tareas

Propuesta inicial, tomada de "Siguiente paso" de `docs/estado.md`. Gemini la refina.

| ID | Tarea | Criterio de aceptación | Estado |
|---|---|---|---|
| T-01 | Mover el código de MCC a `referencia_mcc/` y dejar `main.c` vacío | El proyecto abre y compila en MPLAB X | en espera: la persona decide si MCC se queda en el firmware |
| T-02 | `config.c` (bits de configuración) y `system_init()` con reloj de 16 MHz | Pruebas 0.2 a 0.5 de `docs/protocolos_prueba.md` | propuesta |
| T-03 | `uart.c` y `uart.h`: buffers circulares, una sola ISR, `tx_DAT` y `tx_string`; eco en `main.c` | Pruebas 1.1 a 1.18 | propuesta |
| T-04 | Protocolo de tramas con CRC-8 | Pruebas 2.1 a 2.8 | propuesta |

## Preguntas para el planificador

(Las deja Claude Code cuando una tarea choca con la especificación. Gemini responde aquí.)

## Cambios propuestos

(Los dejan Gemini o Claude Code. La persona decide.)
