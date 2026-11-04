# Archivo Maestro de Arquitectura y Tareas (Fuente de la Verdad)
# Micro 3: contexto del proyecto

Instrumento de inspección por corrientes de Foucault. El PIC adquirirá las señales I y Q, generará el auto-cero con PWM y se comunicará con LabVIEW por UART.

## Estructura de la carpeta

- `hardware/`: archivos de Altium (esquemático, PCB, BOM). Son binarios, no se editan desde aquí.
- `firmware/Micro_Firmware.X/`: proyecto de MPLAB X (PIC16F18426, MPLAB X 6, XC8 2.40, programador SNAP).
- `docs/`: especificaciones y documentación. **Empieza por `docs/especificacion_diseno_uart.md`** (qué se va a construir) y consulta **`docs/notas_pic16f18426.md`** (resumen verificado del chip, con páginas) antes de abrir los PDF grandes (datasheet de ~700 páginas, guía de XC8 de ~580).

Criterios de diseño y valores escogidos (reloj, baud, formato, niveles, buffers, formato de envío ASCII): `docs/criterios_diseno.md`. Circuito UART con el módulo CP2102: `docs/circuito_uart.svg`. El TX del PIC va al adaptador **por un divisor 1 kΩ + 2,2 kΩ**, no directo.

Pruebas de cada módulo (UART primero, luego formato de envío, PWM, ADC, auto-cero e integración): `docs/protocolos_prueba.md`. Cada módulo debe cumplir sus criterios antes de pasar al siguiente.

ADC (Fase 2): requisitos del profesor, SINAD > 60 dB, tiempos, jitter, referencia, histograma, envío de datos y pruebas: `docs/especificacion_adc.md`. *(Agregado por el chat de Claude el 2026-10-30; Gemini lo revisa.)*

Preinforme: **leer `docs/especificaciones_preinforme.md` antes de escribir cualquier preinforme** (requisitos de la clase, valores del UART y el ADC, bloques, diagramas de tiempo, formato de envío, cumplimiento, pendientes y estructura). *(Agregado por el chat de Claude el 2026-10-30; Gemini lo revisa.)*

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

- El enunciado sugiere líneas ASCII y 9600/19200 baud. Nosotros usamos **líneas ASCII** `I:2048,Q:2051\r\n` (decisión del 2026-11-04) a **115200**, 500 líneas por segundo. Formato: `docs/especificacion_protocolo.md`.
- Su fórmula `SPBRG = Fosc/(16·baud) − 1` es de otras familias; en este chip el baud se calcula con `BRG16=1`, `BRGH=1` (ver `docs/notas_pic16f18426.md`).
- Nombres como `SPBRG`, `TXREG`, `RCSTA`, `CCP1CON` son genéricos; aquí son `SP1BRG`, `TX1REG`, `RC1STA`, etc. Comprobar en `pic16f18426.h`.

## Reglas del proyecto

- Se puede usar MCC como base (lo indica la persona; **el enunciado dice lo contrario, así que la persona lo confirma con el profesor**). Todo código de MCC se revisa y se entiende línea a línea antes de usarse, y lo que se cambie queda en `docs/revision_codigo_mcc.md`. El código que se agregue a mano es C escribiendo directamente en los registros (SFR).
- Sin esperas bloqueantes en la lógica: nada de `while(!TXIF)` ni `__delay_ms` en el programa principal.
- UART por interrupciones con FIFO circular. Envío en texto ASCII, una línea `I:xxxx,Q:xxxx\r\n` cada 2 pares del ADC (500 por segundo). El PIC no recibe comandos.
- Alimentación VCC = 3,7 V (fuente Keysight E36233A, según el preinforme). Conversor USB-serie a 3,3 V, nunca a 5 V.
- Chip fijo: PIC16F18426 (14 pines, 8 usados, 1 de reserva: RC2). La gráfica de impedancia I vs Q se dibuja **en el PC** (LabVIEW o Python); el PIC no maneja pantalla. Presupuesto de pines en `docs/especificacion_diseno_uart.md` §3.1.
- Oscilador HFINTOSC a 16 MHz, UART 8N1 a 115200 baud (SP1BRG = 34).
- **SINAD del ADC > 60 dB** (ENOB > 9,67 bits). Requisito de la persona, 2026-10-30. Un 12 bits ideal da 74 dB a escala completa, así que quedan 14 dB para ruido, jitter, referencia y distorsión. Cómo se mide: `docs/especificacion_adc.md` §6, prueba 4.15 de `docs/protocolos_prueba.md`. Por confirmar con el profesor.
- ADC de 12 bits con `VREF = VDD` = 3,7 V, muestreo de **1 kHz** disparado por Timer0 (hardware) y conversión de I (RC0) y Q (RC1) en **menos de 1 ms**, sin esperas bloqueantes. Detalle y motivos: `docs/especificacion_adc.md` y `docs/criterios_diseno.md` §11.
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

- Fase 1: enlace UART PC ↔ PIC (eco probado el 2026-10-19).
- Fase 2 (actual): ADC de I/Q con envío en texto ASCII (T-09), luego PWM de auto-cero y máquina de estados.
- *(Nota del chat de Claude, 2026-10-30, para revisión de Gemini)* El código del ADC (T-09) llegó en un zip antes de cerrar las pruebas de la Fase 1. Se fusionó en `firmware/Micro_Firmware.X` el 2026-10-30; está sin compilar en MPLAB X y sin probar. Ver T-09 y `docs/especificacion_adc.md`.

Cuando cambie una decisión de diseño (pines, baud, formato de envío), actualizar `docs/especificacion_diseno_uart.md`.

## Cómo se explica el código (formato fijo)

Para quien aprende el código línea a línea. Claude, Gemini o Claude Code explican **siempre así**, corto y sin repetir el datasheet:

1. **Una parte a la vez.** Se abre con "Parte N de M" y el nombre. No se pasa a la siguiente hasta que la persona diga "entendido".
2. **Abre:** archivo y función (ruta dentro de `firmware/Micro_Firmware.X/`) y, si viene de MCC, en qué módulo de MCC se configura.
3. **Se encarga de:** una frase.
4. **Qué hace, por función:** las líneas se agrupan por lo que cumplen ("apagar avisos mientras se configura", "encender el receptor"), no por valores numéricos. Los valores solo si la persona los pide.
5. **Nombres:** decodificar cada nombre nuevo la primera vez que aparece (por ejemplo `RC1IE` = *Receive 1 Interrupt Enable*: interruptor del aviso de recepción de la UART 1; `IF` = *Interrupt Flag*: bandera que sube el hardware).
6. **Cambiar:** el ID de `docs/revision_codigo_mcc.md`, o "nada".
7. **Compruébalo:** una o dos preguntas.

Reglas: lenguaje simple, una idea por renglón; máximo unos 25 renglones por parte. Nombres, valores y páginas salen del datasheet o del `.h`, nunca de memoria; lo no verificado se marca. Sin teoría que esa parte no use.

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
| T-02 | Reducida (2026-10-19): en `device_config.c` cambiar a mano `PPS1WAY = OFF` y `FCMEN = OFF` (C-01 y C-02 de `docs/revision_codigo_mcc.md`). Sin `config.c` nuevo (C-03 rechazada); el watchdog ya está apagado (C-04). El reloj de 16 MHz ya lo pone `mcc.c` | Compila; la UART sigue pasando 1.5, 1.6 y 1.7 de `docs/protocolos_prueba.md`; 0.4 y 0.5 cuando haya código de prueba | implementada, sin probar (2026-10-19) |
| T-03 | Adaptar `eusart1.c`/`.h`, `interrupt_manager.c` y `main.c` (cambios C-06 a C-10 de `docs/revision_codigo_mcc.md`): sin esperas, buffers circulares sin contador, `tx_DAT` y `tx_string`, muy comentado. Instrucciones: `docs/instrucciones_claude_code_uart.md` | Compila sin warnings; pruebas 1.5, 1.6, 1.7, 1.9, 1.13 y 1.14 de `docs/protocolos_prueba.md` | aprobada (2026-10-19); requiere C-05 hecho en MCC |
| T-04 | Protocolo de tramas binarias con suma de verificación (`crc8.c/.h`, `protocol.c/.h`) | — | **retirada (2026-11-04)**: la persona decidió mandar solo texto ASCII; los archivos se borraron del firmware |
| T-05 | LED de estado en RA5 (pin 2): heartbeat a 1 Hz **sin esperas** (base de tiempo con un temporizador e interrupción, nunca `__delay_ms`), manteniendo el eco de T-03. RA5 como salida digital (bajar su bit de `ANSELA`). Temporizador, registros y cuentas: elegirlos y verificarlos en el datasheet y en `pic16f18426.h`, y explicarlos en comentarios. Sin otros cambios a la UART | Pruebas 0.5 y 1.12 de `docs/protocolos_prueba.md`; el eco sigue pasando 1.5, 1.6 y 1.7 mientras parpadea | retirada (2026-10-21): la persona pidió quitar el código; las pruebas 0.5 y 1.12 quedan sin hacer |
| T-06 | PWM de auto-cero, **sin serial**: `pwm.c/.h` con `pwm_init`, `pwm_set` y `pwm_get`. PWM6 en RA2 (I0) y PWM7 en RA4 (Q0), Timer2 con `T2PR` = 255 a Fosc/4 (15,625 kHz, 10 bits), duty inicial 512 en los dos. El duty se fija en el código de `main` para la prueba. Detalle: `docs/especificacion_pwm.md` §3 a §7. Los nombres de registros se comprueban en `pic16f18426.h` antes de usarlos. El cálculo del filtro RC queda para después (la persona, 2026-10-21) | Pruebas 3.1 y 3.2 (osciloscopio en el pin); 3.3, 3.4 y 3.7 cuando haya filtro; el envío ASCII sigue funcionando | propuesta  |
| T-07 | Control del PWM desde el PC. Hoy el PIC no recibe comandos: hay que definir un comando de texto (por ejemplo `P:I,512\n`) y leerlo en `main` sin esperas | por definir (`docs/especificacion_pwm.md` §8 tiene la versión anterior) | **por replantear** (2026-11-04): la versión con tramas binarias se retiró junto con T-04 |
| T-08 | Ajuste manual del offset de I y Q desde el PC, con PCB2 conectada y el filtro RC montado. Sin código nuevo: es una tarea de pruebas y de hardware. Valores de R y C y nodo de conexión: por definir | Pruebas 3.5, 3.6, 3.8, 3.9, 3.15 y 3.16; anotar el residuo en cuentas del ADC | propuesta (requiere T-07 y el filtro) |
| T-09 | ADC de I y Q a 1 kHz y envío en texto: `adc_iq.c/.h` (Timer0 dispara el ADC cada 1 ms; I en RC0, Q en RC1; sin promedio desde el 2026-11-03: una muestra por par) y `adc_ascii.c/.h` (una línea `I:xxxx,Q:xxxx\r\n` cada `ADC_PARES_POR_LINEA` = 2 pares = 500 líneas/s, 65,6 % de la línea). `main.c` solo llama a `adc_ascii_task()`. RC2 con la salida del Timer0 (`ADC_PIN_PRUEBA` = 2). Detalle: `docs/especificacion_adc.md` y `docs/especificacion_protocolo.md`. *Fila agregada por el chat de Claude el 2026-10-30; actualizada el 2026-11-04* | Pruebas 2.1 a 2.6, 4.1 a 4.8 y 4.10 a 4.17 de `docs/protocolos_prueba.md`. **SINAD del ADC > 60 dB (4.15)**; I y Q convertidos en menos de 1 ms (4.16) | implementada, sin probar con 500 líneas/s (la persona confirmó el 2026-11-04 que el modo ASCII funciona) |

## Preguntas para el planificador

(Las deja Claude Code cuando una tarea choca con la especificación. Gemini responde aquí.)

Dejadas el 2026-10-30 (chat de Claude, al documentar T-09):

1. El código de T-09 cita `docs/instrucciones_claude_code_adc.md`, la decisión D-3 y las pruebas 4.9, 4.11 y 4.12. Ese archivo no está en la carpeta: ¿dónde está, o hay que redefinirlas? En `docs/especificacion_adc.md` §12 se reconstruyeron D-1, D-2, D-4, C-11 y C-12 a partir de los comentarios del código.
2. `ADACQ = 160`: MCC lo guarda como 10 µs (`AquisitionTime` en el `.mc3`, que divide entre 16 MHz); el datasheet (p.529) lo define en periodos del reloj del ADC, que serían 320 µs, y su Fig. 32-2 (p.501) lo rotula en `TCY`, que serían 40 µs. ¿Cuál es el valor buscado? Se mide con la prueba 4.16 (ver CP-7).
3. SINAD > 60 dB: ¿se mide en el ADC con una senoidal casi a escala completa (lo que se adoptó) o con la señal real de PCB2 (±185 mV, donde un 12 bits ideal da 54 dB)? ¿A qué frecuencias (5 Hz y 20 Hz)?
4. El profesor dio 352 µV de ruido de cuantización ideal. Eso corresponde a 12 bits y 5 V; con `VREF` = 3,7 V son 261 µV. ¿Con qué `Vref` lo calculó?
5. ¿Se confirma `Vref = VDD` (la genera la PIC) o se prefiere el FVR o una referencia externa? Ver `docs/especificacion_adc.md` §7.1. Falta además la impedancia de salida (`Zout`) del opa de las salidas I y Q de PCB2 (el datasheet del MCP604 ya está en `docs/MCP601-2.PDF` y no la da; §3.2).

## Cambios propuestos

(Los dejan Gemini o Claude Code. La persona decide.)

Propuestos el 2026-10-21 (chat de Claude, a petición de la persona). Cada uno queda `propuesta` hasta que la persona lo apruebe:

| ID | Cambio | Dónde se aplicaría | Estado |
|---|---|---|---|
| CP-1 | Comandos del PWM desde el PC (antes tramas `SET_PWM`/`GET_PWM`): ahora en texto, formato por definir con T-07 | `docs/especificacion_protocolo.md`, al replantear T-07 | por replantear |
| CP-2 | Prueba 3.5: el criterio pasa de «≤ 0,9 mV» a «≤ 0,09 mV pp en I0/Q0» (el rizado se multiplica ×10 en la etapa de resta) | `docs/protocolos_prueba.md` Módulo 3 | propuesta |
| CP-3 | Prueba 5.3: el criterio pasa de «≤ 3 cuentas» a «≤ 20 cuentas» con el PWM de 10 bits (paso de 36 mV en la salida = 40 cuentas, error mínimo de medio paso). La resta del residuo se hace después, en el PC o en el PIC | `docs/protocolos_prueba.md` Módulo 5 | propuesta |
| CP-4 | Agregar a `docs/protocolos_prueba.md` las pruebas 3.10 a 3.16 (`docs/especificacion_pwm.md` §10) | `docs/protocolos_prueba.md` | propuesta |
| CP-5 | Decisiones PR-1 a PR-5 del protocolo binario | T-04 | retirada (2026-11-04) con T-04 |
| CP-6 | Prueba 4.3: el criterio de ruido pasa de «σ ≤ 2 LSB» a «σ ≤ 1,4 LSB». Con σ = 2 LSB el SINAD a escala completa sería ≈ 57 dB, por debajo del requisito de 60 dB (presupuesto de ruido y distorsión: 1,31 mV = 1,45 LSB). *Propuesto el 2026-10-30* | `docs/protocolos_prueba.md` Módulo 4 | propuesta |
| CP-7 | `ADACQ` en `adcc.c`: si la prueba 4.16 muestra que cuenta periodos del reloj del ADC (320 µs), bajarlo a 5 (≈ 10 µs con `TAD` = 2 µs). El sesgo entre I y Q a 20 Hz pasaría de 4,4 % a 0,5 % y I + Q terminarían en ≈ 77 µs en vez de ≈ 697 µs. Comprobar antes que 10 µs bastan con la impedancia de fuente real de PCB2 (el datasheet recomienda ≤ 10 kΩ). Fórmula y cuentas: `docs/especificacion_adc.md` §3.1 y §3.2. *Propuesto el 2026-10-30* | T-09 | propuesta (depende del resultado de 4.16) |

Ya decididos por la persona el 2026-10-21 (anotados en `docs/criterios_diseno.md` §10): PWM de auto-cero con 10 bits directos; cálculo del filtro RC aplazado.

Decidido por la persona el 2026-10-30 (anotado en `docs/criterios_diseno.md` §10): **SINAD del ADC > 60 dB**.

Decidido por la persona el 2026-11-03 (anotado en `docs/criterios_diseno.md` §10): **el ADC no promedia**. Se quitó `ADC_N_PROMEDIO` = 5 (D-1); cada par es una sola muestra de I y de Q.

Decidido por la persona el 2026-11-04 (anotado en `docs/criterios_diseno.md` §10): **solo texto ASCII**. Se quitó la trama binaria con su suma de verificación (T-04 retirada, `crc8.*` y `protocol.*` borrados). El PIC manda `I:xxxx,Q:xxxx\r\n` 500 veces por segundo y no recibe comandos. Motivo: el receptor en LabVIEW de la trama binaria era muy largo; el texto se lee con *VISA Read* + *Scan From String*.
