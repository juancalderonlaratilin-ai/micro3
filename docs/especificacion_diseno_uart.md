# Especificación de diseño: enlace UART PC ↔ PIC (Fase 1)

Proyecto Micro 3 · Firmware `firmware/Micro_Firmware.X` · Fecha: 2026-10-16 · Actualizado: 2026-11-05 (pines, periféricos y prioridad de la ISR según el código)

> **Estado:** borrador v2, revisado contra el datasheet, la errata y la guía de XC8. Los valores marcados **[PROPUESTA]** son decisiones de diseño que hay que confirmar. Los marcados **[VERIFICADO]** salen directamente de los documentos de `docs/`. El detalle y las páginas de cada dato están en **`notas_pic16f18426.md`**.

---

## 1. Alcance

**Dentro de la Fase 1:** comunicación serie no bloqueante, con interrupciones. Se probó en los dos sentidos con un eco (2026-10-19). Desde el 2026-11-04 el envío es **solo texto ASCII y solo PIC → PC**: líneas `xxxx,xxxx\r\n` (`docs/especificacion_protocolo.md`).

**Fuera de la Fase 1** (se diseña después, pero los pines ya quedan reservados): lectura de I y Q con el ADC (**ya especificada en `docs/especificacion_adc.md`; el código de T-09 llegó en un zip el 2026-10-30 y está copiado a `firmware/`, sin probar**), PWM por canal que reconstruye I y Q con un filtro pasa bajos (cambio de enfoque del profesor, 2026-11-04; `docs/especificacion_pwm.md`) y la interfaz gráfica; el PWM de auto-cero (I0/Q0) y la calibración pasan a Macro. La interfaz gráfica, que va **en el PC** (LabVIEW o Python), no en el PIC.

**Objetivo de la Fase 1:** demostrar que el PIC recibe y envía bytes sin pérdidas y sin bloquear el programa principal.

---

## 2. Parámetros de diseño

| Parámetro | Valor | Estado |
|---|---|---|
| Microcontrolador | PIC16F18426 (8 bits, encapsulado de 14 pines) | confirmado (configuración del proyecto) |
| IDE / compilador | MPLAB X 6 + XC8 2.40 | confirmado |
| Programador | SNAP | confirmado |
| Estilo de código | C sobre registros (SFR), con el código de MCC como base, revisado línea a línea (`docs/revision_codigo_mcc.md`) | la persona (2026-10-19) |
| Oscilador | **HFINTOSC interno a 16 MHz**, sin PLL ni cristal (exactitud nominal ±2 %) | [PROPUESTA] |
| Rango de VDD del chip | PIC16F18426 (sin "L"): 2,3 – 5,5 V con Fosc ≤ 16 MHz. **Un PIC16LF18426 (máx. 3,6 V) no sirve con 3,7 V** | [VERIFICADO] |
| Ciclo de instrucción | Fcy = Fosc/4 = 4 MHz → 250 ns | calculado |
| Formato UART | **8N1** (8 bits de datos, sin paridad, 1 bit de parada), asíncrono | [PROPUESTA] |
| Baud rate | **115200** | [PROPUESTA] |
| Alimentación (VCC / VDD) | **3,7 V** DC de la fuente Keysight E36233A (GND a +3,7 V). Se asume que el PIC comparte esta alimentación. Las referencias Vref/Vref2 ≈ 1,85 V son la tierra virtual de las señales analógicas | confirmado (preinforme §1.2) |
| Nivel lógico del conversor USB-serie | 3,3 V (CP2102, FT232R o similar) | ver nota 2.3 |
| Visualización (gráfica de impedancia I vs Q) | **En el PC**, por UART, con LabVIEW o Python. El PIC no maneja pantalla | decidido (2026-10-16) |
| ADC (Fase 2) | 12 bits (ADCC), `VREF = VDD = 3,7 V`, canales I = RC0 y Q = RC1 en un ADC multiplexado, **`fs` = 1 kHz** disparado por Timer0, I y Q convertidos en **menos de 1 ms**, sin promedio (1000 pares/s = 1 kHz por canal, desde el 2026-11-03). **Sin filtro anti-aliasing** en esta entrega. Detalle: `docs/especificacion_adc.md` | profesor y código de T-09; sin probar |
| PWM (Fase 2) | Timer2 a Fosc/4, `T2PR` = 255: 15,625 kHz, 10 bits. PWM6 en RA2 (I), PWM7 en RA4 (Q); duty = (código + 2) >> 2 con cada par (1000 veces/s). Detalle: `docs/especificacion_pwm.md` | cambio de enfoque del profesor (2026-11-04); código de T-06, sin probar |
| **SINAD del ADC** | **> 60 dB** (ENOB > 9,67 bits), medido con una senoidal de 5 Hz y de 20 Hz casi a escala completa. Un 12 bits ideal da 74 dB, así que quedan 14 dB para ruido, jitter, referencia y distorsión (`docs/especificacion_adc.md` §6) | requisito de la persona (2026-10-30); condiciones de medición [PROPUESTA] |
| Ruido de cuantización ideal del ADC | 261 µV rms con `VREF` = 3,7 V (el profesor cita 352 µV, que corresponde a 12 bits y 5 V) | por confirmar con el profesor |

### 2.1 Por qué 16 MHz

La carga del procesador es muy baja: en la Fase 2 solo se muestrearán dos señales de unos 20 Hz. 16 MHz sobra, consume menos que 32 MHz (la placa va con batería) y da un error de baud rate pequeño. Si aparecieran errores de framing, se puede subir a 32 MHz sin cambiar el diseño.

### 2.2 Cálculo del baud rate

Modo de alta velocidad con generador de 16 bits (`BRGH = 1`, `BRG16 = 1`):

```
SPBRG = Fosc / (4 × Baud) − 1 = 16 000 000 / (4 × 115 200) − 1 = 33.72  →  34
Baud real = 16 000 000 / (4 × (34 + 1)) = 114 286 baud
Error = (114 286 − 115 200) / 115 200 = −0.79 %
```

Registros: `SP1BRGH = 0`, `SP1BRGL = 34`. La fórmula (`Fosc / [4 × (n + 1)]` con `BRGH = 1` y `BRG16 = 1`) y el valor de 32 MHz (`n = 68`, +0,64 %) coinciden con la Tabla 28-2 del datasheet [VERIFICADO].

**Margen de error:** el −0,79 % se suma a la exactitud nominal del oscilador interno (±2 %), así que el peor caso combinado ronda **−2,8 %**, al límite de lo tolerable. Si el enlace pierde bytes por framing, el datasheet recomienda ajustar **`OSCTUNE`** como método preferido (DS p.410). La alternativa es subir a 32 MHz (`SPBRG = 68`, error +0,64 %). La prueba P2 (ráfaga de 1000 bytes) debe validar este margen.

### 2.3 Notas de hardware

1. **Presupuesto de tiempo a 115200 baud:** 1 byte = 10 bits = 86,8 µs ≈ 347 ciclos de instrucción. Las ISR de UART deben ser cortas (objetivo: RX + TX combinadas < 100 ciclos por byte).
2. **Niveles lógicos con VDD = 3,7 V:**
   - **PIC → conversor:** el TX del PIC sale a ~3,7 V. En el datasheet del CP2102-GMR (Silicon Labs) los pines de la UART están referidos a VIO: máximo absoluto **VIO + 0,5 V** y la hoja no los describe como tolerantes a 5 V. Con VIO = 3,3 V el máximo es 3,8 V: 3,7 V entra, pero con solo 0,1 V de margen. **Decisión: divisor resistivo en el TX del PIC (1 kΩ en serie + 2,2 kΩ a GND)**, que deja ≈2,5 V en el RXD del adaptador (VIH mínimo con VIO = 3,3 V: 1,9 V). El módulo real puede llevar otra variante del CP2102: verificar el marcado del chip.
   - **Conversor → PIC:** un conversor a 3,3 V entrega ~3,3 V. Con VDD = 3,7 V, la entrada Schmitt (por defecto) exige VIH ≥ 0,8 × VDD = **2,96 V**, un margen justo. Con entrada TTL basta VIH ≥ 0,25 × VDD + 0,8 = **1,73 V**. **Decisión: configurar RC5 como TTL (`INLVLC5 = 0`)** [VERIFICADO: DS Tabla 39-4 y registro `INLVLC`, p.217].
   - **No** uses un conversor con salida a 5 V: la entrada del PIC no debe superar VDD + 0,3 V.
3. **Tierra común** entre el conversor USB-serie y la placa.
4. **Por qué 115200 y no 9600** (de cara a la Fase 2): se mandan 500 líneas/s de hasta 11 bytes = 5500 B/s (48,1 % de la línea a 114 286 baud). A 9600 baud solo cabrían 87 líneas/s. 115200 es el baud estándar más bajo en que cabe.

---

## 3. Asignación de pines

El pinout del PIC16F18426 de 14 pines y las funciones de cada pin están [VERIFICADOS] (DS p.6 y p.8). **La asignación de funciones a cada pin es [PROPUESTA]**: hay que confirmar que coincide con tu placa o protoboard.

Evitar los pines de programación (RA0 = ICSPDAT, RA1 = ICSPCLK, RA3 = MCLR) para no perder el depurador SNAP. Las funciones digitales (UART, PWM) se asignan con PPS, así que se pueden mover a cualquier RAx/RCx sin cambiar el diseño.

| Pin físico | Puerto | Función | Fase | Notas |
|---|---|---|---|---|
| 6 | **RC4** | **TX (EUSART1)** | **1** | salida vía PPS: `RC4PPS = 0x0F`. Bajar `ANSELC4` (reset = analógico) |
| 5 | **RC5** | **RX (EUSART1)** | **1** | entrada: `RX1DTPPS = 0x15` (en el datasheet se llama `RX1PPS`) (ya es el valor por defecto). Bajar `ANSELC5`. Nivel TTL (`INLVLC5 = 0`) propuesto; **el código actual lo deja en Schmitt** (`INLVLC = 0x3F` en `pin_manager.c`; VIH = 2,96 V) |
| 2 | RA5 | libre | – | el LED de estado (T-05) se retiró el 2026-10-21 |
| 10 | RC0 | Entrada analógica canal I (ANC0) | 2 | **en uso** (T-09, sin probar) |
| 9 | RC1 | Entrada analógica canal Q (ANC1) | 2 | **en uso** (T-09, sin probar) |
| 11 | RA2 | PWM de I (reconstrucción de la señal muestreada) | 2 | PWM6 vía PPS (`RA2PPS = 0x0D`), código en `pwm.c` (T-06) |
| 3 | RA4 | PWM de Q (reconstrucción de la señal muestreada) | 2 | PWM7 vía PPS (`RA4PPS = 0x0E`), código en `pwm.c` (T-06) |
| 7 | RC3 | libre (el pulsador de auto-cero pasa a Macro, 2026-11-04) | — | – |
| 8 | RC2 | pin de prueba: **salida del Timer0** (`TMR0_out`, `RC2PPS = 0x17`), cuadrada de 500 Hz, cada flanco es un disparo del ADC | 2 | `ADC_PIN_PRUEBA = 2` (`adc_iq.h`, lo que tiene el código). Con `1` sube mientras corre `adc_iq_isr()`. En la placa nueva RC2 es `EN_AZ_PIC`: JP_AZEN en 1–2 |
| 13 / 12 / 4 | RA0 / RA1 / RA3 | ICSPDAT / ICSPCLK / MCLR | – | programación, no usar. RA3 solo es entrada |
| 1 / 14 | VDD / VSS | alimentación | – | con capacitor de desacople de 100 nF cerca |

**Regla general:** al reset, todos los pines de `ANSELA` y `ANSELC` están en **modo analógico** (valor 1). Cada pin que se use como digital necesita su bit de `ANSEL` en 0 (DS p.205).

### 3.1 Presupuesto de pines [PROPUESTA — decisión: se queda el PIC16F18426 de 14 pines]

| Función | Pines | Fase |
|---|---|---|
| UART TX y RX | 2 | 1 |
| LED de estado (retirado el 2026-10-21) | 0 | — |
| Canales I y Q (ADC) | 2 | 2 |
| PWM de I y Q (PWM6 y PWM7 + filtro pasa bajos de reconstrucción) | 2 | 2 |
| Pin de prueba RC2 (salida del Timer0) | 1 | 2 |
| Pulsador de auto-cero (pasa a Macro) | 0 | — |
| **Usados** | **7** | |
| Libres para E/S (12 − RA0, RA1, RA3) | 9 | |
| **Reserva** | **2** (RA5 y RC3) | |

Razones y consecuencias:

- **Los PWM reproducen las señales muestreadas** (cambio de enfoque del profesor, 2026-11-04): cada par del ADC fija el duty de PWM6 (I) y PWM7 (Q), y un filtro pasa bajos de ≈ 100 Hz devuelve la señal analógica (`docs/especificacion_pwm.md`). La sustitución de los trimmers I0/Q0 por PWM (auto-cero) pasa a Macro.
- **La gráfica de impedancia no usa pines del PIC.** Los valores de I y Q viajan por la UART y el PC dibuja I (eje X) contra Q (eje Y). Cada par viaja como texto, `xxxx,xxxx\r\n` (hasta 11 bytes, §8). A 500 líneas/s son 5500 B/s, el 48,1 % de la capacidad de la línea.
- **Una pantalla local casi no cabe** en este chip: un OLED I2C ocuparía los 2 pines libres (RA5 y RC3). Una pantalla SPI necesitaría 4–5 pines. Si el profe la pide, el reemplazo sin cambios de código es el **PIC16F18446** (20 pines, 18 E/S, 17 canales de ADC; mismo datasheet [VERIFICADO: DS p.14]).
- **Sin cristal externo:** usaría RA4 (OSC2) y RA5 (OSC1); RA4 es el PWM de Q.
- **Debe respetarse RA0/RA1/RA3** para que el SNAP pueda programar y depurar.

---

## 4. Periféricos (Fase 1)

| Periférico | Uso | Configuración clave |
|---|---|---|
| Oscilador HFINTOSC | reloj del sistema | `RSTOSC` = HFINTOSC 1 MHz; `OSCILLATOR_Initialize()` (`mcc.c`): `OSCFRQ = 0x05` (16 MHz), `OSCCON1 = 0x60` (NOSC = 110, NDIV = 1:1); no espera `OSCCON3[ORDY]` |
| PPS | enrutar TX y RX a RC4 / RC5 | `RC4PPS = 0x0F` (TX1), `RX1DTPPS = 0x15` (en el datasheet se llama `RX1PPS`) (RC5). `PPSLOCKED = 0` tras el reset: no hace falta desbloquear |
| Entradas/salidas | pines digitales | `ANSELC4 = ANSELC5 = 0`; `TRISC4 = 0`, `TRISC5 = 1`; `INLVLC5 = 0` (TTL) propuesto; **el código actual lo deja en Schmitt** (`INLVLC = 0x3F` en `pin_manager.c`; VIH = 2,96 V) |
| EUSART1 | enlace con el PC | async, 8N1, `BRGH = 1`, `BRG16 = 1`, `SP1BRGH = 0`, `SP1BRGL = 34`, `SYNC = 0`, `SPEN`, `TXEN`, `CREN` |
| Interrupciones | RX y TX por evento | `RC1IE` (recepción) siempre activa. `TX1IE` **solo mientras hay datos por enviar**. `PEIE` y `GIE` activos. Vector único en `0x0004` |
| ADCC | I y Q a 12 bits | `ADREF = 0x00` (VDD y VSS), `ADCLK = 0x0F` (Fosc/32, `TAD` = 2 µs), `ADACQ = 160`, `ADACT = 0x02` (desbordamiento de Timer0), resultado justificado a la derecha, `ADIE = 1`. Ver `docs/especificacion_adc.md` §2 y §3 |
| Timer0 | disparo del ADC cada 1 ms | 8 bits, Fosc/4, prescaler 1:16, `TMR0H = 249` (250 cuentas × 4 µs = 1000 µs); `TMR0_out` a RC2 por PPS (`RC2PPS = 0x17`) |
| Timer2 + PWM6/PWM7 | reconstrucción de I y Q | `T2CLKCON = 0x01` (Fosc/4), `T2PR = 255`, `T2CON = 0x80` (1:1); `RA2PPS = 0x0D` (PWM6), `RA4PPS = 0x0E` (PWM7); `PWMxCON = 0x80`. Sin interrupciones (`docs/especificacion_pwm.md` §5.3) |
| Bits de configuración | ver §11 | `LVP` habilitado, WDT desactivado en desarrollo, BOR activo |

Periféricos de la Fase 2: el ADCC (2 canales, referencia VDD = 3,7 V; las salidas I y Q quedan centradas en ~1,85 V) y el Timer0 (disparo a 1 kHz) tienen código desde el 2026-10-30 (T-09, copiado a `firmware/`, sin probar). Timer 2 + **PWM6/PWM7** ya se usan para reconstruir I y Q (T-06, `pwm.c`); el IOC del pulsador pasa a Macro.

---

## 5. Eventos

Arquitectura guiada por eventos: las interrupciones solo capturan el evento y mueven bytes. El procesamiento ocurre en el programa principal.

| ID | Evento | Fuente | Respuesta |
|---|---|---|---|
| E1 | Llegó un byte | `RC1IF && RC1IE` | La ISR lee `RC1STA` (para `FERR`/`OERR`) **antes** que `RC1REG`, y guarda el byte en la FIFO de RX. Leer `RC1REG` limpia `RC1IF` |
| E2 | `TX1REG` libre | `TX1IF && TX1IE` (**hay que comprobar las dos**: `TX1IF` vale 1 siempre que `TX1REG` esté vacío) | La ISR saca el siguiente byte de la FIFO de TX y lo escribe en `TX1REG`. Al escribir el último byte, desactiva `TX1IE` |
| E3 | Error de hardware en RX (framing u overrun) | `FERR` / `OERR` | Marca la bandera de error. El framing **no genera interrupción**: se detecta al leer el byte. Si hay overrun (`OERR`), se recupera con `CREN = 0` y luego `CREN = 1` |
| E4 | FIFO de RX llena | la ISR no puede guardar | Descarta el byte y marca la bandera de overflow |
| E5 | FIFO de TX llena | `uart_tx_*` no puede encolar | Devuelve `false` al llamador (no bloquea) |
| E6 | Hay un par nuevo del ADC | `adc_iq_get()` en el programa principal | `pwm_desde_adc()` fija el duty de PWM6/PWM7; cada 2 pares, `adc_ascii_par()` arma la línea `I,Q\r\n` y la encola con `tx_string()` |
| E8 | Terminó una conversión del ADC | `ADIF && ADIE` | `adc_iq_isr()` guarda I (o Q), arranca la conversión siguiente y deja el par listo (T-09) |

---

## 6. Tareas

| ID | Tarea | Dónde corre | Descripción |
|---|---|---|---|
| T1 | `SYSTEM_Initialize()`, `adc_iq_init()`, `pwm_init()` | arranque | Oscilador, pines/PPS, EUSART, ADC y Timer0, Timer2 y PWM; al final `PEIE` y `GIE` |
| T2 | bloque RX de la ISR | interrupción (E1, E3, E4) | Mover el byte de hardware a la FIFO de RX. Corto. Hoy nadie lee esos bytes (no hay comandos) |
| T3 | bloque TX de la ISR | interrupción (E2) | Mover un byte de la FIFO de TX al hardware |
| T4 | `uart_tx_DAT(const uint8_t *data, uint16_t size)` | llamada desde el programa principal | Encola un bloque de bytes. `true` si cupo todo, `false` si no (E5) |
| T5 | `uart_tx_string(const char *s)` | llamada desde el programa principal | Igual que T4, para cadenas terminadas en `'\0'` (firma pedida en clase) |
| T6 | `uart_rx_read(uint8_t *byte)` (`EUSART1_Read`) | llamada desde el programa principal | Saca un byte de la FIFO de RX. `false` si está vacía. Hoy no se usa |
| T7 | `adc_ascii_par(i, q)` | bucle principal | Recibe el par que leyó `main` con `adc_iq_get`; cada 2 pares arma `I,Q\r\n` y lo encola (E6) |
| T8 | `pwm_desde_adc(i, q)` | bucle principal | Con cada par, `duty = (código + 2) >> 2` en PWM6 (I) y PWM7 (Q) (E6, `docs/especificacion_pwm.md`) |
| T9 | `adc_iq_isr()` | interrupción (E8) | Guarda I, arranca Q, guarda Q y deja el par listo (T-09) |
| T10 | `adc_iq_get(&i, &q)` | llamada desde el programa principal | Entrega el último par (una muestra de I y una de Q, sin promedio) si hay uno nuevo; no espera (T-09) |

**Bucle principal:** `while(1) { if (adc_iq_get(&i, &q)) { pwm_desde_adc(i, q); adc_ascii_par(i, q); } }`, sin esperas bloqueantes (prohibido `while(!TXIF)` y `__delay_ms` en la lógica).

**Una sola ISR:** el chip tiene un único vector de interrupción (`0x0004`) y no tiene prioridades. T2 y T3 son dos bloques dentro de la misma función `void __interrupt() isr(void)`, que comprueba **bandera y habilitación** de cada fuente y limpia (o consume) la bandera antes de salir. No se rehabilita `GIE` dentro de la ISR [VERIFICADO: UG §5.8]. Con T-09 se suma un tercer bloque, el del ADC (`ADIF` y `ADIE`), que `interrupt_manager.c` atiende después de TX y RX.

**Prioridad:** E1, E2 y (desde T-09) E8 son las únicas interrupciones. `interrupt_manager.c` las revisa con `if / else if` en este orden: **TX, luego RX, luego ADC**, y atiende una por entrada (las demás quedan con su bandera arriba y entran enseguida). Como hoy el RX no se lee, el orden no arriesga datos; si se agregan comandos, conviene revisar que la RX no espere más de ≈ 174 µs.

**Plazo de servicio de RX:** el receptor tiene una FIFO de solo 2 bytes y se produce overrun si llega completo un tercer byte sin haber leído `RC1REG`. Con 86,8 µs por byte, la ISR debe atenderse en menos de ~2 tiempos de byte (≈ 174 µs). En la Fase 2, ninguna otra ISR (ADC, timer) puede retener la CPU ese tiempo. La duración de `adc_iq_isr()` se mide con RC2 (prueba 4.13), y el jitter del instante de muestreo de Q depende de lo que tarden las ISR de la UART (`docs/especificacion_adc.md` §4).

**Reglas de código de las ISR** [VERIFICADO: UG §5.8.5–5.8.7]:
- Variables compartidas con la ISR: `volatile`. Los accesos a más de 1 byte no son atómicos: proteger con `di()` / `ei()` en el programa principal.
- No llamar a la misma función desde la ISR y desde `main` (XC8 la duplica, porque no es reentrante). Los índices de las FIFO se manipulan con código propio de cada lado.

---

## 7. Estructuras de datos

| Estructura | Tamaño | Notas |
|---|---|---|
| FIFO de RX | 32 bytes | potencia de 2 para usar máscara en lugar de módulo |
| FIFO de TX | 64 bytes | |
| Índices (`head`, `tail`) | `uint8_t`, `volatile` | la ISR escribe uno y el programa principal el otro |

El chip tiene 2048 B de RAM [VERIFICADO], así que hay margen para agrandar las FIFO si la Fase 2 lo pide. Los motivos de cada tamaño y qué pasa al llenarse están en `docs/criterios_diseno.md` §6.1.

Las operaciones de la FIFO que compartan estado entre la ISR y el programa principal deben ser atómicas (índice de 8 bits, o sección crítica corta).

---

## 8. Formato de envío (texto ASCII)

### 8.1 Fase 1a: eco crudo (ya probado)

El PIC devolvía exactamente cada byte que recibía. Sirvió para validar cableado, baud rate y las dos direcciones (pruebas 1.5, 1.6 y 1.7, pasaron el 2026-10-19). Ese código ya no está en el firmware.

### 8.2 Envío de datos: una línea de texto por par

```
2048,2051\r\n
```

- I y Q en decimal, de 0 a 4095, sin ceros a la izquierda; termina en `\r\n`. Hasta 11 bytes.
- Una línea cada 2 pares del ADC = **500 líneas/s** (`ADC_PARES_POR_LINEA` en `adc_ascii.h`). Una de 11 bytes tarda 0,96 ms; la línea va al 48,1 %.
- Solo PIC → PC. El PIC empieza a mandar al encender y no recibe comandos.
- El PC lee hasta `\n` y separa con `%d,%d`; una línea con otra forma se descarta.

Detalle, cuentas y motivos: `docs/especificacion_protocolo.md` y `docs/criterios_diseno.md` §5.

---

## 9. Pruebas de aceptación

| # | Prueba | Criterio de éxito |
|---|---|---|
| P1 | Eco de 1 byte desde una terminal serie en hexadecimal | vuelve el mismo byte (pasó el 2026-10-19, con el código de eco de entonces) |
| P2 | Ráfaga de 1000 bytes | vuelven los 1000 bytes sin pérdidas ni cambios (pasó el 2026-10-19) |
| P3 | Líneas `I,Q` en una terminal | 500 líneas/s con la forma correcta (pruebas 2.1 y 2.2) |
| P4 | 10 minutos de captura | 0 líneas malas después de la primera y `adcLineasPerdidas` = 0 (prueba 2.4) |
| P7 | Desconectar y reconectar el cable | el enlace se recupera sin reiniciar el PIC (prueba 2.5) |
| P8 | **SINAD del ADC** con una senoidal de 5 Hz y otra de 20 Hz (1,7 V de amplitud, centrada en 1,85 V) en RC0 y RC1 | **SINAD > 60 dB** (prueba 4.15 de `docs/protocolos_prueba.md`) |
| P9 | Muestreo de I y Q | `fs` = 1 kHz ± 1 % y las dos conversiones en menos de 1 ms (pruebas 4.5 y 4.16) |
| P10 | Histograma con la entrada fija | σ y códigos distintos anotados (prueba 4.3) |

---

## 10. Pendientes por confirmar

1. **Que tu chip sea `PIC16F18426` y no `PIC16LF18426`** (mira el marcado). El LF solo admite hasta 3,6 V y la placa trabaja a 3,7 V.
2. Que el PIC se alimente de los mismos 3,7 V que PCB2, y el modelo exacto del conversor USB-serie con su nivel lógico.
3. Que los pines propuestos (§3) coincidan con tu placa o protoboard.
4. **Revisión de silicio del chip** (MPLAB X → *Window > Dashboard*). Si es A4, ninguna errata del EUSART aplica. Si es A1/A2, la regla de escribir `TX1REG` solo con `TX1IF = 1` ya cubre el workaround (ER 1.9.2).
5. Formato de los comandos de texto: no hay en Micro 3 (T-07, control del PWM desde el PC, pasa a Macro).
6. WDT: se deja desactivado en desarrollo. ¿Se activa en la versión final?
7. Valores exactos de los bits de configuración: generarlos con MPLAB X (*Window > Target Memory Views > Configuration Bits*) o consultar `pic_chipinfo.html` de la carpeta `docs` de XC8.
8. Qué terminal se usará para las pruebas (Realterm, Hercules, Tera Term) y si el PC ya tiene el driver del conversor.
9. Valor `VBOR` (brown-out) con VDD = 3,7 V, que falta leer en las especificaciones eléctricas.
10. Referencia del ADC = VDD: está en el código desde el 2026-10-30 (`ADREF = 0x00`). Falta confirmarla con el profesor y medir el ruido de la alimentación, porque la lectura depende de ella (`docs/especificacion_adc.md` §7). Los demás pendientes del ADC están en §13 de ese archivo.
11. Capa física de la UART: este documento describe el divisor 1 kΩ + 2,2 kΩ (montaje en protoboard); la placa Micro3 usa traductores SN74LVC1T45 (U5 y U6, `docs/contexto_uart_tx_rx.md` §1). Confirmar cuál se usa en la entrega.

---

## 11. Bits de configuración [PROPUESTA — generar el pragma con MPLAB X]

| Bit | Valor | Motivo |
|---|---|---|
| `FEXTOSC` | sin oscilador externo (`100`) | se usa el HFINTOSC |
| `RSTOSC` | HFINTOSC 1 MHz (`110`) | punto de partida limpio para subir a 16 MHz en código |
| `CSWEN` | `1` | permite cambiar `NOSC`/`OSCFRQ` desde el código |
| `LVP` | **`1` (habilitado)** | con `LVP = 1`, RA3 es MCLR y `MCLRE` se ignora. Con `LVP = 0` haría falta alto voltaje en MCLR para programar |
| `WDTE` | `00` (desactivado) | solo durante el desarrollo |
| `BOREN` | `11` (activo) | protege contra caídas de tensión |
| `STVREN` | `1` | reset por desbordamiento de pila |
| `PPS1WAY` | `0` | permite reconfigurar PPS más de una vez (el valor de fábrica es 1) |
| `DEBUG` | lo gestiona MPLAB X/SNAP | no tocar a mano |

XC8 exige programar **todos** los bits de configuración, y los valores numéricos del `#pragma config` se escriben en decimal o hexadecimal, **nunca con `0b`** [VERIFICADO: UG §5.2.5].

---

## 12. Documentos de referencia (en `docs/`)

| Archivo | Contenido |
|---|---|
| `notas_pic16f18426.md` | **Resumen verificado** del chip: pines, oscilador, PPS, EUSART, errata, XC8, con páginas |
| `PIC16-L-F18426-46-Micorontrollers-with-XLP-40001985.pdf` | Datasheet (fuente de verdad) |
| `PIC16-L-F18426-46-Family-Silicon-Errata-...-80000799.pdf` | Errata de silicio |
| `50002737C XC8 C Compiler UG for PIC.pdf` | Guía del compilador XC8 |
| `TB3261-Technical-Brief-DS90003261A.pdf` | Cómo escribir C para PIC16/PIC18 |
| `00688b.pdf` | AN688: layout para ADC de 12 bits (relevante para PCB, no para el firmware) |
| `Preinforme Practica 2 Final-...pdf` | Alimentación, filtros de 20 Hz, referencias de 1,85 V |
| `especificacion_adc.md` | **ADC:** requisitos del profesor, SINAD > 60 dB, tiempos, jitter, referencia, histograma y envío de datos |
