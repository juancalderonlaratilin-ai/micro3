# Documento técnico de Micro 3: especificaciones, decisiones, código y evaluación

Proyecto Micro 3 (Práctica 3, Microcontroladores) · PIC16F18426 · Versión del 2026-11-05 · Firmware: `firmware/Micro_Firmware.X`

> **Para qué sirve:** reúne en un solo archivo **todas** las especificaciones técnicas y decisiones de diseño, las fórmulas con sus cuentas, el flujo del código, la justificación de cada función, los cambios hechos al código que generó MCC y una lista de preguntas de evaluación con respuesta corta. Todo sale del código tal como está hoy. Los documentos de detalle siguen en `docs/` (§12).
>
> **Etiquetas:** **[CÓDIGO]** = leído en el firmware. **[DS]** = datasheet DS40001985C, con página. **[CÁLCULO]** = cuenta propia a partir de los anteriores. **[PROPUESTA]** = decisión por aprobar. **[POR MEDIR]** = no se ha comprobado en el chip.
>
> **Estado:** el firmware compila con XC8 2.40 (0 errores; programa 1002 de 16 384 palabras = 6,1 %, datos 204 de 2048 bytes = 10 %). El eco de la UART a 115200 se probó el 2026-10-19. El envío ASCII funciona según la persona (2026-11-04). **Nada del ADC ni del PWM se ha medido todavía.**

---

## Contenido

1. Qué hace el sistema
2. Requisitos y cumplimiento
3. Hardware: chip, pines, alimentación, niveles y configuración
4. Especificaciones y decisiones por bloque, con fórmulas
5. Flujo del código
6. Archivos y funciones: qué hace cada una y por qué existe
7. Cambios frente al código generado por MCC
8. Alternativas descartadas
9. Verificación: qué está probado y qué no
10. Limitaciones, riesgos y pendientes
11. Preguntas de evaluación (como las haría el profesor)
12. Referencias

---

## 1. Qué hace el sistema

El PIC es el bloque de adquisición de un instrumento de corrientes de Foucault. PCB1 y PCB2 (Práctica 2) entregan dos tensiones de banda base, **I** (en fase) y **Q** (cuadratura), centradas en ≈ 1,85 V. El PIC:

1. **Muestrea I y Q con el ADC a 1 kHz por canal**, con el disparo hecho por hardware desde el Timer0.
2. **Reproduce cada canal con un PWM** (PWM6 → I, PWM7 → Q, base de tiempo Timer2). Con un filtro pasa bajos en cada salida se vuelve a ver la señal analógica muestreada.
3. **Manda I y Q al PC por la UART** como texto, `2048,2051\r\n`, 500 líneas por segundo. El PC (LabVIEW o Python) dibuja el plano I-Q y mide SINAD, ruido e histogramas.

```
            PCB2 (Práctica 2)                       PIC16F18426                                   Salidas
  V_I,OUT ──────────────────────► RC0 ─┐
                                       ├─► ADC 12 bits ──► par (I,Q) cada 1 ms ──┬─► PWM6 → RA2 ─► filtro ─► I reconstruida
  V_Q,OUT ──────────────────────► RC1 ─┘      ▲                                  ├─► PWM7 → RA4 ─► filtro ─► Q reconstruida
                                              │ disparo cada 1 ms                └─► texto "I,Q\r\n" (1 de cada 2) ─► RC4 (TX) ─► USB-serie ─► PC
                                   Timer0 ────┴──► RC2 (TMR0_out, cuadrada de 500 Hz, para medir)
```

**Las tres frecuencias que no hay que confundir** [CÓDIGO]:

| Dónde | I | Q | Por qué |
|---|---|---|---|
| ADC (en el PIC) | 1000 muestras/s | 1000 muestras/s | cada disparo del Timer0 convierte I y luego Q (1000 pares/s) |
| PWM (RA2, RA4) | 1000 actualizaciones/s | 1000 actualizaciones/s | `pwm_desde_adc()` usa **todos** los pares |
| UART → PC | 500 muestras/s | 500 muestras/s | `ADC_PARES_POR_LINEA = 2`: una línea con I y Q cada 2 pares |

**Lo que NO está en esta entrega:** filtro anti-aliasing, auto-cero (pasa a Macro por decisión del profesor del 2026-11-04), comandos desde el PC, trama binaria con suma de verificación, promedio de muestras, máquina de estados (INIT/CALIBRATE/RUN/ERROR), LED de estado.

---

## 2. Requisitos y cumplimiento

| # | Requisito | Origen | Cómo se cumple | Estado |
|---|---|---|---|---|
| R1 | `fs` = 1 kHz | profesor | Timer0: 250 cuentas × 4 µs = 1000 µs; disparo por hardware (`ADACT = 0x02`) | cumple por cálculo; [POR MEDIR] prueba 4.5 |
| R2 | I y Q convertidos dentro de 1 ms | derivado de R1 | 77 µs (caso A) a 697 µs (caso B) | cumple por cálculo; prueba 4.16 |
| R3 | 12 bits | enunciado | ADCC de 12 bits (DS `AD01`, p.632) | cumple |
| R4 | Disparo por Timer, nunca `__delay_ms` | enunciado §3.2 | `ADACT` = Timer0; sin retardos en todo el firmware | cumple (revisión de código) |
| R5 | SINAD del ADC > 60 dB | persona (2026-10-30) | 12 bits ideales dan 74 dB; quedan 14 dB de margen | [POR MEDIR] prueba 4.15 |
| R6 | Jitter: importa el del instante de muestreo | profesor | I por hardware; Q arrancada por la ISR (ver §4.4.8) | I [POR MEDIR]; Q no garantizado a 10 ns |
| R7 | Histograma de 10 000 muestras en LabVIEW | profesor | líneas ASCII a 500/s: 20 s de captura | [POR MEDIR] |
| R8 | UART no bloqueante con buffer circular | enunciado §3.4 | FIFO de TX 64 B y RX 32 B por interrupciones; nada espera | cumple; eco probado |
| R9 | `bool tx_DAT(const char *DATA, uint16_t size)` y `bool tx_string(const char *data)` | enunciado §3.4 C | en `eusart1.c`, todo o nada | cumple (código) |
| R10 | Formato ASCII terminado en `\n` | enunciado §3.4 D | `2048,2051\r\n` | cumple (variante sin nombres) |
| R11 | Un PWM por canal que reproduce las muestras | profesor (2026-11-04) | PWM6/PWM7, 10 bits, actualizados con cada par | cumple (código); [POR MEDIR] Módulo 3 |
| R12 | Filtro anti-aliasing `fc ≤ fs/2` | enunciado §3.2 | **no hay en esta entrega** | **pendiente** |
| R13 | Resistencia de manejo del S&H y salida del opamp | profesor | fórmula de `TACQ` con `RS` (§4.4.5) | calculado; falta `Zout` del opamp |

---

## 3. Hardware: chip, pines, alimentación, niveles y configuración

### 3.1 Chip y herramientas

| Dato | Valor |
|---|---|
| Microcontrolador | **PIC16F18426** (14 pines, 28 KB de Flash = 16 K palabras, 2048 B de RAM) [DS p.5] |
| Rango de VDD | 2,3 a 5,5 V con Fosc ≤ 16 MHz (el `PIC16LF18426` solo llega a 3,6 V: no sirve con 3,7 V) [DS p.616] |
| Alimentación | **VDD = 3,7 V** (fuente Keysight E36233A, la misma de PCB2) |
| Herramientas | MPLAB X 6, XC8 2.40, programador SNAP |
| Revisión de silicio medida | ID 0x30D2, revisión 0x2002 (2026-10-19) |

### 3.2 Pines [CÓDIGO]

| Pin | Puerto | Función | Configuración | Dónde |
|---|---|---|---|---|
| 1 | VDD | 3,7 V | 100 nF de desacople | – |
| 2 | RA5 | libre | entrada analógica (reset) | – |
| 3 | **RA4** | **PWM7 → Q reconstruida** | `RA4PPS = 0x0E`, salida, digital | `pwm_init()` |
| 4 | RA3 | MCLR (programación) | `LVP = ON` | `device_config.c` |
| 5 | **RC5** | **RX de la UART** | `RX1DTPPS = 0x15`, entrada digital (`ANSELC` bit 5 = 0) | `pin_manager.c` |
| 6 | **RC4** | **TX de la UART** | `RC4PPS = 0x0F`, salida (`TRISC` bit 4 = 0) | `pin_manager.c` |
| 7 | RC3 | libre | – | – |
| 8 | **RC2** | **salida del Timer0** (pin de prueba) | `RC2PPS = 0x17`, salida digital (`ADC_PIN_PRUEBA = 2`) | `adc_iq_init()` |
| 9 | **RC1** | **entrada Q** (ANC1) | analógica (`ANSELC` bit 1 = 1), `CANAL_Q = 0x11` | `pin_manager.c`, `adcc.h` |
| 10 | **RC0** | **entrada I** (ANC0) | analógica, `CANAL_I = 0x10` | `pin_manager.c`, `adcc.h` |
| 11 | **RA2** | **PWM6 → I reconstruida** | `RA2PPS = 0x0D`, salida, digital | `pwm_init()` |
| 12 / 13 | RA1 / RA0 | ICSPCLK / ICSPDAT (SNAP) | no se usan | – |
| 14 | VSS | tierra | – | – |

**Por qué estos pines:** se dejan libres RA0, RA1 y RA3 para que el SNAP pueda programar y depurar. Las funciones digitales (UART, PWM, `TMR0_out`) van por PPS, así que se ponen en cualquier RAx/RCx; I y Q tienen que ir a pines con canal analógico (ANC0 y ANC1). RA4 queda libre porque no hay cristal (`FEXTOSC = OFF`).

**Estado de los puertos tras `PIN_MANAGER_Initialize()`** [CÓDIGO]: `TRISA = 0x3F`, `TRISC = 0x2F` (solo RC4 es salida), `ANSELA = 0x37`, `ANSELC = 0x1F` (RC5 digital), `INLVLA = INLVLC = 0x3F` (Schmitt), `SLRCONA = 0x37`, `SLRCONC = 0x3F` (pendiente limitada), `WPU = 0`, `ODCON = 0`. Después `pwm_init()` pone RA2 y RA4 como salidas digitales y `adc_iq_init()` hace lo mismo con RC2.

### 3.3 Niveles lógicos con VDD = 3,7 V [DS Tabla 39-4]

| Señal | Cálculo | Conclusión |
|---|---|---|
| TX del PIC → CP2102 (VIO = 3,3 V) | máximo del CP2102: VIO + 0,5 = 3,8 V; el PIC saca ≈ 3,7 V | entra por 0,1 V: en protoboard se puso **divisor 1 kΩ + 2,2 kΩ**: 3,7 · 2,2 / 3,2 = **2,54 V** (≥ VIH 1,9 V) |
| CP2102 → RX del PIC, entrada Schmitt | VIH = 0,8 · VDD = **2,96 V** | con 3,3 V queda un margen de 0,34 V |
| CP2102 → RX del PIC, entrada TTL | VIH = 0,25 · VDD + 0,8 = **1,73 V** | margen amplio |

**Ojo:** el diseño decidió TTL en RC5 (`INLVLC5 = 0`), pero **el código lo deja en Schmitt** (`INLVLC = 0x3F`). En la placa Micro3 hay traductores SN74LVC1T45 (U5 y U6) que entregan ≈ 3,7 V al PIC, y con eso Schmitt alcanza. Con el CP2102 conectado directo, el margen es justo. Falta decidir: o se aplica TTL, o se documenta que se usan los traductores.

### 3.4 Bits de configuración (`device_config.c`) [CÓDIGO]

| Bit | Valor | Por qué |
|---|---|---|
| `FEXTOSC` | OFF | se usa el oscilador interno; RA4 y RA5 quedan libres |
| `RSTOSC` | HFINT1 (HFINTOSC a 1 MHz) | arranque seguro; `OSCILLATOR_Initialize()` sube a 16 MHz |
| `CSWEN` | ON | permite cambiar `NOSC`/`OSCFRQ` desde el código |
| `FCMEN` | **OFF** (C-02) | el monitor de reloj solo vigila osciladores externos |
| `CLKOUTEN` | OFF | RA4 se usa como PWM |
| `MCLRE` / `LVP` | ON / ON | RA3 es MCLR; con LVP = 1 el SNAP programa sin alto voltaje |
| `WDTE` | OFF (C-04) | sin watchdog durante el desarrollo |
| `BOREN` / `BORV` | ON / LO (2,45 V) | reset por caída de tensión |
| `PPS1WAY` | **OFF** (C-01) | el candado de PPS se puede abrir más de una vez (lo necesitan `pwm_init` y `adc_iq_init`, que escriben PPS después de MCC) |
| `STVREN` | ON | reset si se desborda la pila |
| `CP` | OFF | sin protección de código |

---

## 4. Especificaciones y decisiones por bloque, con fórmulas

### 4.1 Reloj

| Dato | Valor |
|---|---|
| Oscilador | HFINTOSC interno a **16 MHz**, sin PLL ni cristal; exactitud nominal ±2 % [DS p.4] |
| Registros | `OSCCON1 = 0x60` (NOSC = HFINTOSC, NDIV = 1), `OSCFRQ = 0x05` (16 MHz) en `OSCILLATOR_Initialize()` [CÓDIGO] |
| `Tosc` / `TCY` | 1/16 MHz = **62,5 ns** / 4·Tosc = **250 ns** (Fcy = 4 MHz) |

**Por qué 16 MHz:** es el reloj más bajo que da un error de baud pequeño a 115200 (−0,79 %, §4.6). Con 8 MHz el error sería +2,1 % y con 4 MHz −3,6 %. 32 MHz daría +0,64 %, pero consume más y no hace falta: la CPU tiene poca carga (dos señales de ≈ 20 Hz). El ADC (`TAD`), el PWM y el Timer0 salen del mismo reloj.

**Detalle:** `OSCILLATOR_Initialize()` no espera `OSCCON3[ORDY]`. El cambio de reloj tarda unos ciclos y no afecta, porque la UART se configura después.

### 4.2 Timer0: base de tiempo del muestreo

| Registro | Valor | Significado |
|---|---|---|
| `T0CON1` | 0x44 | `T0CS` = 010 (Fosc/4), `T0ASYNC` = 0, `T0CKPS` = 0100 (1:16) |
| `T0CON0` | 0x80 | `T0EN` = 1, modo de 8 bits, postscaler 1:1 |
| `TMR0H` | 249 | registro de periodo en modo de 8 bits |

**Fórmula del periodo (modo de 8 bits)** [DS §18, p.260–261]:

```
T_tick = 4 · Tosc · prescaler = 4 · 62,5 ns · 16 = 4 µs        (reloj de 250 kHz)
Ts     = (TMR0H + 1) · T_tick = 250 · 4 µs = 1000 µs   →   fs = 1 kHz
```

- En modo de 8 bits el hardware compara `TMR0L` con `TMR0H` y, al coincidir, pone `TMR0L` en 0 **solo, sin software**. No hay recarga en una ISR, así que el periodo no tiene jitter de software.
- **Qué señal va al ADC** (Figura 18-1, DS p.260): la salida del postscaler va directo a «Peripherals» (un pulso en cada coincidencia: cada 1 ms). Hacia el pin va por un flip-flop que cambia de nivel en cada coincidencia: por eso **RC2 muestra una cuadrada de 500 Hz** (2 ms de periodo) en la que **cada flanco** es un disparo del ADC. Falta confirmarlo midiendo (prueba 4.5): si los pulsos de la ISR de I salieran cada 2 ms, habría que poner `TMR0H = 124`.
- **Por qué Timer0 y no otro:** el Timer2 es del PWM (su periodo de 64 µs no sirve para 1 ms); Timer1/3/5 son de 16 bits y sin registro de periodo habría que recargarlos por software; Timer4 o Timer6 también servirían y quedan libres.

### 4.3 PWM con Timer2: reconstrucción de I y Q

| Registro | Valor | Significado |
|---|---|---|
| `T2CLKCON` | 0x01 | reloj del Timer2 = Fosc/4 (el valor de reset, 0, es un pin externo y el timer no contaría) |
| `T2HLT` | 0x00 | modo libre |
| `T2PR` | 255 | periodo |
| `T2CON` | 0x80 | `ON` = 1, prescaler 1:1, postscaler 1:1 |
| `RA2PPS` / `RA4PPS` | 0x0D / 0x0E | PWM6OUT → RA2, PWM7OUT → RA4 [DS Tabla 16-3] |
| `PWM6CON` / `PWM7CON` | 0x80 | `EN` = 1, `POL` = 0 (en alto durante el duty) |
| `CCPTMRS1` | sin escribir | tras el reset ya asigna PWM6 y PWM7 al Timer2 [DS p.346] |

**Fórmulas** [DS Ec. 24-1 y 24-3]:

```
T_PWM      = (T2PR + 1) · 4 · Tosc · prescaler = 256 · 4 · 62,5 ns · 1 = 64 µs   →   f_PWM = 15,625 kHz
Resolución = log2(4 · (T2PR + 1)) = log2(1024) = 10 bits
Duty       = PWMxDC / (4 · (T2PR + 1)) = PWMxDC / 1024
V_media    = VDD · PWMxDC / 1024          paso = 3,7 V / 1024 = 3,61 mV
```

**Del código del ADC (12 bits) al duty (10 bits)** [CÓDIGO, `a_duty()`]:

```
duty = (código + 2) >> 2         recortado a 1023
```

- Dividir entre 4 pasa de 4096 a 1024 niveles; sumar 2 antes redondea al más cercano, así que el error es ≤ ½ paso (1,8 mV). 4095 daría 1024, que no existe: se recorta a 1023.
- **Ganancia 1 sin calibrar:** el ADC usa `VDD` como referencia y el PWM sale entre 0 y `VDD`. Código 2048 → duty 512 → 3,7 · 512/1024 = **1,850 V**, la misma tensión de entrada.
- El duty de 10 bits se parte en dos registros: `PWMxDCH = duty >> 2` (8 bits altos) y `PWMxDCL = (duty & 3) << 6` (2 bits bajos en las posiciones 7:6).
- **Doble buffer:** el hardware copia `DCH`/`DCL` al empezar el siguiente periodo (cuando `T2TMR = T2PR`). Si esa copia cae entre las dos escrituras, **un solo periodo de 64 µs** sale con un valor mezclado (error ≤ 3 pasos). El filtro lo borra. Por eso no se apagan las interrupciones ni se espera nada.
- **Retardo muestra → PWM:** ≈ 0,1 ms (fin de la ráfaga I+Q y `main`) + hasta 64 µs (siguiente periodo). Después, el retardo de grupo del filtro.
- **SINAD ideal de un DAC de 10 bits:** 6,02 · 10 + 1,76 = **62 dB**.
- **Sin interrupciones:** el PWM corre solo en hardware; no agrega ninguna fuente a la ISR.

**Filtro de reconstrucción** [PROPUESTA, sin montar; `docs/especificacion_pwm.md` §6]: Sallen-Key de ganancia 1, R1 = R2 = 22 kΩ, C1 = 100 nF (realimentación), C2 = 47 nF (a tierra).

```
f0 = 1 / (2π · √(R1·R2·C1·C2)) = 1 / (2π · 22 kΩ · √(100 nF · 47 nF)) ≈ 105 Hz
Q  = √(R1·R2·C1·C2) / (C2 · (R1 + R2)) ≈ 0,73
```

Corta en ≈ 100 Hz porque tiene que **dejar pasar la señal** (hasta ≈ 20 Hz, −0,01 dB), **quitar la portadora del PWM** (15,6 kHz, −87 dB) y **suavizar los escalones de 1 ms** (imágenes alrededor de 1 kHz, −39 dB). Simulado: SINAD ≈ 54 dB a 20 Hz y ≈ 64 dB a 5 Hz; lo limita un 2.º armónico propio del PWM de flanco final (el centro del pulso se mueve con el duty).

### 4.4 ADC

#### 4.4.1 Configuración [CÓDIGO, `adcc.c`]

| Registro | Valor | Significado |
|---|---|---|
| `ADCON0` | 0x84 | `ON` = 1, `FM` = 1 (justificado a la derecha), reloj Fosc/`ADCLK`, sin conversión continua |
| `ADCLK` | 0x0F | Fosc/32 → **`TAD` = 2 µs** (rango permitido 0,5 a 9 µs, DS `AD20`) |
| `ADREF` | 0x00 | `VREF+ = VDD`, `VREF− = VSS` |
| `ADACQ` | 160 | tiempo de adquisición (unidad en duda, §4.4.5) |
| `ADACT` | 0x02 | disparo automático: `TMR0_overflow` [DS Tabla 32-6, p.538] |
| `ADCON1`, `ADCON2`, `ADCON3` | 0x00 | modo básico: sin promedio, sin filtro, sin doble muestra (la errata 1.1.2 no aplica) |
| `ADPCH` | 0x00 en MCC → `CANAL_I` (0x10) en `adc_iq_init()` | ver C-12 |
| `PIE1.ADIE` | 1 | interrupción al terminar cada conversión |

#### 4.4.2 Resolución y cuantización

```
LSB = VREF / 2^N = 3,7 V / 4096 = 0,903 mV
código = Vin · 4096 / VDD          (1,85 V → 2048;  1,000 V → 1107)
σq  = LSB / √12 = 0,261 mV rms     (ruido de cuantización ideal)
```

El profesor citó 352 µV: corresponde a 12 bits con 5 V (1,221 mV / √12). Con 3,7 V son 261 µV.

**Referido a PCB2:** la etapa final tiene ganancia 10, así que 1 LSB equivale a 90 µV en la banda base. El rango lineal de PCB2 es ±185 mV = 370 mV = **410 códigos** (≈ 8,7 bits útiles).

#### 4.4.3 Por qué `VREF = VDD`

| Opción | Veredicto |
|---|---|
| **`VDD` = 3,7 V (elegida)** | cubre de 0 a 3,7 V (los rieles de PCB2) sin recortar; no usa pines; es **ratiométrica**: el reposo `Vref2` ≈ 1,85 V sale de un divisor del mismo riel, así que si `VDD` se mueve el código de reposo sigue en 2048. Desventaja: el ruido de `VDD` entra en la medida |
| FVR de 4,096 V | no puede superar `VDD` [DS p.491] |
| FVR de 2,048 V | recorta todo lo que pase de 2,048 V; el rango lineal llega a 2,035 V (13 mV de margen); no es ratiométrica |
| Pin `VREF+` externo | está en RA1 = ICSPCLK: choca con el programador |

#### 4.4.4 Tiempo de conversión

```
T_conv = 14 · TAD + 2 · TCY = 14 · 2 µs + 2 · 0,25 µs = 28,5 µs        [DS AD22, p.633]
```

#### 4.4.5 Tiempo de adquisición y resistencia de manejo del S&H

El capacitor de retención `CHOLD` tiene que cargarse hasta quedar a ½ LSB de la tensión del pin [DS §32.3, Ec. 32-1, p.506–507]:

```
TACQ  = TAMP + TC + TCOFF
TAMP  = 2 µs
TC    = CHOLD · (RIC + RSS + RS) · ln(2^(n+1) − 1)     con n = 12:  ln(8191) = 9,011
TCOFF = 0,05 µs/°C · (T − 25 °C)
CHOLD = 28 pF,  RIC = 1 kΩ,  RSS ≈ 7 kΩ a 5 V (a 3,7 V se usa 10 kΩ, conservador)
RS    = impedancia que ve el pin: salida del opamp de PCB2 + resistencias en serie (recomendado ≤ 10 kΩ, AD08)
```

| `RS` | `TACQ` a 25 °C | a 50 °C |
|---|---|---|
| 200 Ω | 4,83 µs | 6,08 µs |
| 1 kΩ | 5,03 µs | 6,28 µs |
| 10 kΩ | 7,30 µs | 8,55 µs |

Comprobación: con `RSS` = 7 kΩ, `RS` = 10 kΩ y 50 °C la fórmula da 7,79 µs, igual que el ejemplo del datasheet.

**La salida del opamp no es una resistencia fija:** en lazo cerrado `Zout ≈ Ro / (1 + A(f)·β)`, muy baja en continua y mayor a las frecuencias del golpe de carga de `CHOLD` (cientos de kHz). El datasheet del MCP604 no da `Zout`: hay que simularla (modelo SPICE) o medirla. Con cargas de más de 40 pF el fabricante pide una resistencia en serie (`RISO`); el pin ya presenta ≈ 33 pF.

**Duda abierta sobre `ADACQ` = 160** [POR MEDIR, prueba 4.16]:

| Si cuenta… | Adquisición | Por canal | I + Q |
|---|---|---|---|
| ciclos de Fosc (lo que guarda MCC: 10 µs) | 10 µs | 38,5 µs | **77 µs** |
| `TCY` (Fig. 32-2 del DS) | 40 µs | 68,5 µs | 137 µs |
| `TAD` (texto del registro, DS p.529: «Number of ADC clock periods») | 320 µs | 348,5 µs | **697 µs** |

Los tres casos cumplen R2 (< 1 ms). Si resulta ser 320 µs, sobra: se propone bajar `ADACQ` (CP-7) con `ADACQ = ⌈1,2 · TACQ / T_unidad⌉`.

#### 4.4.6 Secuencia de I y Q en cada tick (multiplexado)

1. El Timer0 coincide → el hardware pone `GO` → **adquisición y conversión de I** (RC0).
2. Fin de I → `ADIF` → `adc_iq_isr()`: guarda I, `ADPCH = CANAL_Q`, `ADGO = 1` → **adquisición y conversión de Q** (RC1).
3. Fin de Q → `ADIF` → `adc_iq_isr()`: guarda Q, `ADPCH = CANAL_I` (listo para el próximo tick), deja el par en la salida y pone `listo = true`.

**Por qué un solo ADC multiplexado:** el chip tiene un solo convertidor, y para señales de ≈ 20 Hz no hace falta muestreo simultáneo.

**Sesgo entre I y Q:** Q se muestrea `Δt` después de I.

```
error relativo = 2π · f · Δt          desfase = 360° · f · Δt
caso A (Δt ≈ 38,5 µs), f = 20 Hz:  0,48 %  y  0,28°
caso B (Δt ≈ 348,5 µs), f = 20 Hz: 4,4 %   y  2,5°
```

**Por qué no alternar canales a 2 kHz** (I en un tick y Q en el siguiente): el sesgo sería de 500 µs (6 % a 20 Hz).

#### 4.4.7 Nyquist y aliasing (sin filtro anti-aliasing)

```
fs (ADC y PWM) = 1000 Hz  →  Nyquist = 500 Hz
fs (PC)        = 500 Hz   →  Nyquist = 250 Hz
f_alias = | f − k · fs |   (el k entero que deja el resultado entre 0 y fs/2)
sobremuestreo respecto de 20 Hz = 1000 / (2 · 20) = 25
```

- **En esta entrega no hay filtro anti-aliasing.** Una componente de más de 500 Hz en el pin se pliega ya en el ADC. Una entre 250 y 500 Hz se ve bien en el PWM, pero se pliega en los datos del PC, porque descartar 1 de cada 2 pares no lleva filtro digital.
- Ejemplo: una entrada de 500 Hz aparece en los datos del PC como una continua (|500 − 1·500| = 0).
- El filtro de 19,4 Hz de PCB2 es parte de la demodulación (Práctica 2); en las pruebas del ADC se usa un generador directo al pin, sin filtro.

#### 4.4.8 Jitter

```
SNR_jitter = −20 · log10(2π · f_in · σt)
```

| `σt` | a 20 Hz | a 5 Hz |
|---|---|---|
| 10 ns | 118 dB | 130 dB |
| 1 µs | 78 dB | 90 dB |
| 10 µs | 58 dB | 70 dB |

- Lo que importa es el jitter del **instante de muestreo** (fin de la adquisición), no el del reloj de conversión: el S&H ya capturó la tensión.
- **I** la dispara el hardware: solo el oscilador aporta jitter (el datasheet no lo especifica).
- **Q** la arranca el software en la ISR: hereda la variación de la latencia de interrupción (3 a 4 `TCY` = 0,75 a 1 µs) y, si coincide con una ISR de la UART, unos µs más (hay un solo vector y no hay anidamiento). Si la latencia varía uniformemente entre 0 y `T`, σ = `T` / √12.
- Para la señal de ≤ 20 Hz, hasta σ ≈ 2,5 µs el jitter solo cuesta 70 dB: el requisito de 10 ns es mucho más estricto que lo que pide el SINAD. Con Q por software **no se puede garantizar 10 ns**.

#### 4.4.9 SINAD > 60 dB

```
SINAD = 20 · log10(V_señal,rms / V_(ruido+distorsión),rms)
ENOB  = (SINAD − 1,76) / 6,02
SINAD ideal 12 bits = 6,02 · 12 + 1,76 = 74,0 dB
ENOB mínimo para 60 dB = (60 − 1,76) / 6,02 = 9,67 bits
Senoidal a escala completa: 1,85 V pico = 1,308 V rms → ruido + distorsión ≤ 1,308 V / 10^(60/20) = 1,31 mV rms = 1,45 LSB rms
```

- El SINAD **depende de la amplitud**: con la señal real de PCB2 (±185 mV), un 12 bits ideal da 54 dB. Se adoptó que el requisito es del **ADC**, medido con una senoidal casi a escala completa (1,7 V de amplitud centrada en 1,85 V, a 5 Hz y 20 Hz). Falta confirmarlo con el profesor.
- Lo consumen: ruido de `VDD` (referencia), jitter de Q, no linealidad (`AD02` ±1 LSB típ.), acople del PWM (15,6 kHz en RA2/RA4) y ruido de la placa. El offset y la ganancia no entran (son errores de continua).
- Medición: 4096 pares por las líneas ASCII, FFT con ventana de Kaiser (β = 14), fundamental ± 6 bins (`herramientas/interfaz_micro3.py`).

#### 4.4.10 Sin promedio

Desde el 2026-11-03 cada par es **una sola** conversión de I y una de Q (antes se promediaban 5). Así cada línea trae el ADC crudo, que es lo que piden el histograma y el SINAD. Se pierde la mejora de ruido del promedio (hasta 7 dB si el ruido es blanco).

### 4.5 Interrupciones

- Un solo vector (`0x0004`), sin prioridades ni anidamiento [DS §7]. Una sola función: `INTERRUPT_InterruptManager()` (`void __interrupt()`).
- Orden de revisión [CÓDIGO]: `if` **TX** (`TX1IE && TX1IF`) → `else if` **RX** (`RC1IE && RC1IF`) → `else if` **ADC** (`ADIE && ADIF`). Atiende una fuente por entrada; si hay otra pendiente, la ISR vuelve a entrar enseguida.
- Siempre se comprueba **habilitación y bandera**: `TX1IF` vale 1 siempre que `TX1REG` esté vacío, aunque `TX1IE` sea 0.
- `PEIE` y `GIE` se encienden **al final** de `main`, después de configurar todo, para que ninguna interrupción llegue a un módulo a medio configurar.
- **Plazo de la RX:** la FIFO del receptor es de 2 bytes; hay overrun si llega completo un tercero sin leer `RC1REG`: ≈ 2 · 87,5 µs = **175 µs**. Ninguna ISR dura tanto.

### 4.6 UART (EUSART1)

| Registro | Valor | Significado |
|---|---|---|
| `BAUD1CON` | 0x08 | `BRG16` = 1 |
| `TX1STA` | 0x24 | `TXEN` = 1, `BRGH` = 1, asíncrono, 8 bits |
| `RC1STA` | 0x90 | `SPEN` = 1, `CREN` = 1, 8 bits |
| `SP1BRGH:SP1BRGL` | 0x00:0x22 (34) | divisor del baud |
| `PIE3.RC1IE` / `TX1IE` | 1 / 0 al inicio | TX solo se enciende cuando hay datos |

**Fórmulas** [DS Tabla 28-2]:

```
Baud = Fosc / (4 · (SP1BRG + 1))     con BRGH = 1 y BRG16 = 1
SP1BRG = 16 MHz / (4 · 115200) − 1 = 33,72 → 34
Baud real = 16 MHz / (4 · 35) = 114 286         error = (114 286 − 115 200) / 115 200 = −0,79 %
T_bit = 8,75 µs;   T_byte (8N1, 10 bits) = 87,5 µs;   capacidad = 11 429 B/s
```

- **La fórmula de clase** (`SPBRG = Fosc/(16·baud) − 1`, `BRG16` = 0) da 7,68 → 8 → 111 111 baud = **−3,55 %**: demasiado. Con `BRG16` = 1 el divisor es 4·(n + 1) y el error baja a −0,79 %.
- **Tolerancia:** el receptor muestrea a la mitad de cada bit; el bit de parada está a 9,5 bits del flanco de inicio. El desfase acumulado debe ser menor que medio bit: 0,5 / 9,5 = **±5,3 %** teórico (±2–3 % en la práctica). Peor caso: −0,79 % + ±2 % del oscilador ≈ −2,8 %.
- **Por qué 115200 y no 9600/19200** (lo que sugiere el enunciado): se mandan 5500 B/s. A 9600 (960 B/s) ocuparía el 573 %, a 19200 el 286 % y a 57600 el 95 %. 115200 es el estándar más bajo en que cabe con margen (48 %).
- **8N1:** 8 bits porque todas las terminales y LabVIEW los esperan; sin paridad porque solo detecta errores de 1 bit y cuesta 10 % de la línea (una línea dañada casi siempre pierde la forma `número,número`); 1 bit de parada basta.

**Buffers circulares** [CÓDIGO, `eusart1.c`]:

| Buffer | Tamaño | Útil | Por qué |
|---|---|---|---|
| TX | 64 B | 63 B = 5 líneas de 11 B | el PIC manda 11 B cada 2 ms; aguanta que `main` se atrase varias líneas |
| RX | 32 B | 31 B | hoy no se lee (no hay comandos); queda por si se agregan |

- **Potencia de 2:** el índice se envuelve con `(i + 1) & (N − 1)`, una sola operación. Con `%` haría falta una división por software dentro de la ISR. Hay un `#error` si el tamaño no es potencia de 2.
- **Lleno / vacío sin contador:** vacío si `head == tail`; lleno si `(head + 1) & mask == tail`. Se pierde una casilla, pero cada índice lo escribe **un solo lado** (TX: `main` escribe `head`, la ISR escribe `tail`; RX: al revés). Al ser de 8 bits, cada lectura o escritura es una sola instrucción: no hace falta apagar interrupciones.
- **RAM:** 64 + 32 + 32 (estado de RX) ≈ 128 B de 2048.

### 4.7 Formato de envío: líneas ASCII

```
2048,2051\r\n        →   32 30 34 38 2C 32 30 35 31 0D 0A   (11 bytes)
```

| Parte | Bytes |
|---|---|
| I en decimal, 0 a 4095, sin ceros a la izquierda | 1 a 4 |
| `,` | 1 |
| Q en decimal | 1 a 4 |
| `\r\n` | 2 |

```
T_línea   = 11 · 87,5 µs = 962,5 µs
1 línea por par (1000/s):    11 000 B/s → 96 % de la línea (sin margen)
1 línea cada 2 pares (500/s): 5500 B/s → 48,1 %   ← lo escogido (ADC_PARES_POR_LINEA = 2)
```

- **Por qué texto:** se lee en cualquier terminal, en el Serial Plotter del Arduino IDE (números separados por coma = dos curvas) y en LabVIEW con *VISA Read* (fin de línea `\n`) + *Scan From String* (`%d,%d`). Es lo que sugiere el enunciado. La trama binaria con suma de verificación se retiró el 2026-11-04 porque su receptor en LabVIEW era muy largo.
- **Por qué sin nombres** (`I:…,Q:…`): 11 bytes en vez de 15; con nombres la línea iría al 65,6 %.
- **Lo que se pierde:** no hay suma de verificación ni número de secuencia (una línea perdida no se ve: se revisan `adcLineasPerdidas` y las líneas por segundo); el PC depende del orden (primero I); no hay comandos del PC al PIC.
- **Conversión a decimal sin dividir** (`a_decimal()`): para cada posición se resta 1000, 100, 10 o 1 mientras quepa; las veces que cupo son la cifra, y `'0' + cifra` es su código ASCII. Para 4095: 4 + 0 + 9 + 5 = 18 restas. El PIC16 no tiene divisor por hardware y `printf` ocuparía mucha memoria.

---

## 5. Flujo del código

### 5.1 Arranque (`main.c`)

| Orden | Llamada | Qué deja listo |
|---|---|---|
| 1 | `SYSTEM_Initialize()` (MCC) | `PMD_Initialize()` (todos los periféricos con reloj), `PIN_MANAGER_Initialize()` (pines y PPS de la UART), `OSCILLATOR_Initialize()` (16 MHz), `ADCC_Initialize()` (ADC con disparo por Timer0, `ADIE` = 1), `TMR0_Initialize()` (Timer0 contando), `EUSART1_Initialize()` (115200, `RC1IE` = 1) |
| 2 | `adc_iq_init()` | apaga el Timer0, `ADPCH = CANAL_I`, borra `ADIF`, estado inicial, RC2 → `TMR0_out`, `TMR0L = 0` y enciende el Timer0 |
| 3 | `pwm_init()` | Timer2, PWM6 en RA2 y PWM7 en RA4 con duty 512 (1,85 V) |
| 4 | `INTERRUPT_PeripheralInterruptEnable()`, `INTERRUPT_GlobalInterruptEnable()` | `PEIE` = 1 y `GIE` = 1, **al final** |
| 5 | `tx_string("Micro3 listo\r\n")` | mensaje de arranque (14 bytes); el PC lo ignora porque no tiene la forma `número,número` |
| 6 | `while (1)` | bucle principal |

**Por qué `adc_iq_init()` va antes de `GIE`:** MCC deja el ADC en RA0 y el Timer0 ya contando. Si las interrupciones estuvieran encendidas, la primera conversión podría ser de RA0 (ICSPDAT) y la máquina de I/Q arrancaría desfasada.

### 5.2 Bucle principal

```c
while (1) {
    if (adc_iq_get(&i, &q)) {       // true solo si la ISR dejó un par nuevo (cada 1 ms)
        pwm_desde_adc(i, q);        // primero el PWM: la salida cambia lo antes posible
        adc_ascii_par(i, q);        // cada 2 pares arma la línea y la encola
    }
    // nada espera; lo que llegue por RX se queda en el buffer sin leer
}
```

### 5.3 Interrupciones

| Fuente | Función | Qué hace |
|---|---|---|
| TX libre (`TX1IE && TX1IF`) | `EUSART1_Transmit_ISR()` | pasa **un** byte del buffer a `TX1REG`; si no queda nada, `TX1IE = 0` |
| Byte recibido (`RC1IE && RC1IF`) | `EUSART1_Receive_ISR()` | anota `FERR`/`OERR`, reinicia el receptor si hubo overrun, guarda el byte (o lo descarta si el buffer está lleno) |
| Fin de conversión (`ADIE && ADIF`) | `adc_iq_isr()` | máquina de dos estados: guarda I y arranca Q, o guarda Q y deja el par listo |

### 5.4 Un periodo de 2 ms (caso A de adquisición)

```
tiempo (µs)   0      38,5    77                 1000   1038,5  1077              2000
Timer0        ▲ disparo                          ▲ disparo                          ▲
ADC           [adq I|conv I][adq Q|conv Q]       [adq I|conv I][adq Q|conv Q]
ISR ADC              ▌             ▌                    ▌             ▌
main            pwm + cuenta par (1.º)              pwm + cuenta par (2.º) + arma línea (≈ 280 µs [ESTIMADO])
UART                                                  [──────── 11 bytes, 962,5 µs ────────]
RC2           ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾|_____________________________________|‾‾‾  (cambia en cada disparo)
```

Latencia de una muestra hasta el último byte en el cable ≈ 1,3 ms (sin el USB-serie).

### 5.5 Variables compartidas entre la ISR y `main`

| Variable | La escribe | La lee | Cómo se protege |
|---|---|---|---|
| `salidaI`, `salidaQ` (16 bits) | ISR del ADC | `adc_iq_get()` | `main` apaga **solo `ADIE`** mientras copia (un `uint16_t` se lee en dos pasos de 8); `GIE` sigue encendido y la UART se atiende |
| `listo` (bool) | ISR (true) y `adc_iq_get` (false) | los dos | un byte: lectura atómica |
| `eusart1TxHead` / `TxTail` | `main` / ISR de TX | el otro lado | cada índice tiene un solo escritor; 8 bits |
| `adcParesPerdidos` | ISR | depurador | `volatile`; debe quedar en 0 |
| `adcLineasPerdidas` | `main` | depurador | `volatile`; debe quedar en 0 |

---

## 6. Archivos y funciones: qué hace cada una y por qué existe

### 6.1 Archivos

| Archivo | Origen | Para qué |
|---|---|---|
| `main.c` | MCC + a mano (T-09, T-06) | arranque y bucle principal |
| `adc_iq.c/.h` | a mano (T-09) | máquina de muestreo de I y Q |
| `adc_ascii.c/.h` | a mano (T-09) | texto `I,Q\r\n` hacia el PC |
| `pwm.c/.h` | a mano (T-06) | Timer2 + PWM6/PWM7 de reconstrucción |
| `mcc_generated_files/eusart1.c/.h` | MCC, editado (T-03: C-05 a C-10) | UART con buffers circulares, `tx_DAT`, `tx_string` |
| `mcc_generated_files/interrupt_manager.c/.h` | MCC, editado (C-06, C-11) | la única ISR |
| `mcc_generated_files/device_config.c` | MCC, editado (C-01, C-02) | bits de configuración |
| `mcc_generated_files/mcc.c/.h` | MCC, sin cambios | `SYSTEM_Initialize`, reloj, PMD |
| `mcc_generated_files/pin_manager.c/.h` | MCC, sin cambios | pines y PPS de la UART |
| `mcc_generated_files/adcc.c/.h` | MCC, sin cambios | configuración del ADC; define `CANAL_I` y `CANAL_Q` |
| `mcc_generated_files/tmr0.c/.h` | MCC, sin cambios | Timer0 a 1 ms |

### 6.2 Funciones escritas a mano

| Función | Qué hace | Quién la llama | Por qué existe así |
|---|---|---|---|
| `adc_iq_init()` | apaga el Timer0, pone `ADPCH = CANAL_I`, borra `ADIF`, inicializa el estado y los contadores, saca `TMR0_out` por RC2, reinicia `TMR0L` y enciende el Timer0 | `main`, una vez, antes de `GIE` | corrige C-12 (MCC deja el canal en RA0 = ICSPDAT); hace que el primer disparo llegue 1 ms completo después y que sea de I |
| `adc_iq_isr()` | si el estado es I: guarda I, `ADPCH = CANAL_Q`, `ADGO = 1`; si es Q: `ADPCH = CANAL_I`, cuenta un par perdido si el anterior no se leyó, copia el par a la salida, `listo = true`. Baja `ADIF` primero | `interrupt_manager.c` (C-11) | `ADACT` solo admite **una** fuente de disparo, así que Q tiene que arrancarla el software. Corta y sin esperas: la ADC hace la adquisición sola porque `ADACQ` > 0 |
| `adc_iq_get(&i, &q)` | si `listo`, copia el par con `ADIE` apagado y devuelve `true`; si no, `false` al instante | `main` | entrega el par sin bloquear y sin leer medio valor viejo y medio nuevo; apaga solo `ADIE` para no frenar la UART |
| `adc_ascii_par(i, q)` | cuenta pares; cada `ADC_PARES_POR_LINEA` arma `I,Q\r\n` y lo encola con `tx_string`; si no cabe, cuenta `adcLineasPerdidas` | `main` | separa el formato del muestreo: el mismo par sirve al PWM y a la UART; nunca reintenta ni espera |
| `a_decimal(v, dst)` | escribe 0–9999 en decimal por restas sucesivas, sin ceros a la izquierda | `adc_ascii_par` | sin `printf` ni división (el PIC16 no divide por hardware) |
| `pwm_init()` | apaga PWM y Timer2, pone RA2/RA4 como salidas digitales en 0, asigna PPS, configura el Timer2 (Fosc/4, `T2PR` = 255), duty 512, enciende el Timer2 y los PWM | `main`, una vez, antes de `GIE` | orden seguro: el pin sale en 0 antes de volverlo salida y el duty está cargado antes de encender |
| `pwm_set(canal, duty)` | revisa el canal y el duty (≤ 1023); escribe `DCH = duty >> 2` y `DCL = (duty & 3) << 6`; guarda la copia | `pwm_init`, `pwm_desde_adc` | valida antes de tocar el hardware; no espera porque los registros tienen doble buffer |
| `pwm_get(canal)` | devuelve la copia en RAM del duty (0xFFFF si el canal no existe) | depuración | leer el duty sin rearmarlo desde dos registros |
| `a_duty(código)` | `(código + 2) >> 2`, recortado a 1023 | `pwm_desde_adc` | 12 → 10 bits redondeando (error ≤ ½ paso) |
| `pwm_desde_adc(i, q)` | aplica `a_duty` a cada canal y llama a `pwm_set` | `main`, con cada par | une el ADC con el PWM: 1000 actualizaciones por segundo |

### 6.3 Funciones de la UART (`eusart1.c`, editadas)

| Función | Qué hace | Por qué |
|---|---|---|
| `EUSART1_Initialize()` | apaga RX/TX IE, configura baud y modo, vacía los índices, enciende `RC1IE` | deja todo en un estado conocido antes de `GIE` |
| `EUSART1_Write(b)` | encola un byte; `false` si el buffer está lleno; enciende `TX1IE` | C-08: la de MCC tenía `while` que esperaba |
| `tx_DAT(DATA, size)` | calcula el espacio libre, y si cabe **todo** copia el bloque y enciende `TX1IE`; si no, `false` sin tocar nada | firma pedida en clase; «todo o nada» evita mandar media línea |
| `tx_string(data)` | `tx_DAT(data, strlen(data))` | firma pedida en clase |
| `EUSART1_Read(&b)` | saca un byte; `false` si no hay | C-07: la de MCC esperaba un byte para siempre |
| `EUSART1_is_tx_ready()` / `is_rx_ready()` | hay espacio / hay datos, con índices | C-09: sin contadores compartidos |
| `EUSART1_is_tx_done()` | buffer vacío **y** `TRMT` = 1 | C-10: `TRMT` solo no sabe si quedan bytes en el buffer |
| `EUSART1_get_last_status()`, `get_overflow()` | errores del último byte leído y aviso de RX descartados | diagnóstico |
| `EUSART1_Transmit_ISR()` | un byte por interrupción; apaga `TX1IE` al vaciar | si `TX1IE` quedara encendido con el buffer vacío, la ISR se repetiría sin fin |
| `EUSART1_Receive_ISR()` + `RxDataHandler()` | lee `FERR`/`OERR` **antes** de `RC1REG`, reinicia con `CREN` 0→1 si hubo overrun, guarda el byte o lo descarta | sin reinicio, tras un overrun el EUSART deja de recibir |

---

## 7. Cambios frente al código generado por MCC

**Regla:** el código de MCC es la base. Todo lo que se cambia queda registrado con su ID en `docs/revision_codigo_mcc.md`. **Después de editar a mano no se vuelve a pulsar *Generate*:** reescribiría los archivos (el `.mc3` todavía dice `PPS1WAY` y `FCMEN` en ON).

| ID | Archivo | Qué hacía MCC | Qué se cambió | Por qué |
|---|---|---|---|---|
| C-01 | `device_config.c` | `PPS1WAY = ON` | `OFF` | con ON, el PPS solo se puede desbloquear una vez y un cambio posterior se ignora sin aviso; `pwm_init` y `adc_iq_init` escriben PPS después de MCC |
| C-02 | `device_config.c` | `FCMEN = ON` | `OFF` | el monitor de reloj solo vigila osciladores externos; con el HFINTOSC no hace nada |
| C-03 | `device_config.c` | – | rechazado | la persona no vio necesario copiar y comentar los bits en otro archivo |
| C-04 | `device_config.c` | `WDTE = OFF` | sin cambio (decisión) | watchdog apagado durante el desarrollo |
| C-05 | `eusart1.c` (en MCC) | buffers de 8 B | TX 64 B, RX 32 B | en 8 B no cabe ni una línea de 11 B |
| C-06 | `eusart1.c/.h`, `interrupt_manager.c` | punteros `EUSART1_Tx/RxDefaultInterruptHandler` | llamada directa a `EUSART1_Transmit_ISR()` / `Receive_ISR()` | los punteros solo sirven para cambiar la función en ejecución, cosa que no se hace; ahorra un salto indirecto en la ISR |
| C-07 | `EUSART1_Read()` | `while (0 == eusart1RxCount);` | devuelve `false` si no hay byte | el enunciado prohíbe esperas bloqueantes |
| C-08 | `EUSART1_Write()` | `while (0 == eusart1TxBufferRemaining);` | devuelve `false` si está lleno; se agregan `tx_DAT` y `tx_string` | mismo motivo; firmas pedidas en clase |
| C-09 | `eusart1.c` | contadores `RxCount` y `TxBufferRemaining` compartidos | lleno/vacío con los índices `head` y `tail` | con contador, `main` y la ISR escriben la misma variable de varios pasos: MCC tenía que apagar `RC1IE`/`TX1IE` un instante |
| C-10 | `EUSART1_is_tx_done()` | solo `TRMT` | buffer vacío **y** `TRMT` | `TRMT` solo mira el registro de desplazamiento |
| C-11 | `interrupt_manager.c` | `ADCC_ISR()` → puntero `ADCC_ADI_InterruptHandler` → función vacía | llamada directa a `adc_iq_isr()`, que baja `ADIF` | mismo criterio que C-06; sin bajar `ADIF` la interrupción se repetiría sin fin |
| C-12 | (efecto de `adcc.c`) | `ADPCH = 0x00` = RA0 = ICSPDAT | `adc_iq_init()` pone `ADPCH = CANAL_I` | el primer disparo debe convertir I (RC0), no el pin del programador |

**Lo que se dejó como lo generó MCC** (revisado, sin cambios): `adcc.c` (salvo el uso de `CANAL_I`/`CANAL_Q`), `tmr0.c`, `pin_manager.c`, `mcc.c`. Las funciones de MCC que no se usan (`ADCC_GetSingleConversion`, `TMR0_Reload`, etc.) siguen en el proyecto y solo generan avisos (520) del compilador.

**Diferencias que quedaron a propósito o por resolver:**
- `INLVLC = 0x3F` (RC5 en Schmitt), aunque el diseño decía TTL (§3.3).
- `ADACQ = 160` con unidad en duda (§4.4.5).
- `ADCC_Initialize()` configura y habilita el ADC antes de que `adc_iq_init()` cambie el canal: por eso `adc_iq_init()` apaga el Timer0 y borra `ADIF` antes de nada.

---

## 8. Alternativas descartadas

| Decisión | Alternativa | Por qué no |
|---|---|---|
| Reloj de 16 MHz | 8 MHz / 32 MHz / cristal | 8 MHz: +2,1 % de error de baud; 32 MHz: más consumo sin necesidad; cristal: ocupa RA4 y RA5 |
| 115200 baud | 9600 / 19200 / 57600 / 125 000 | no caben 5500 B/s; 125 000 (lo que daba el MCC de clase) no es estándar |
| Texto ASCII | trama binaria con suma de verificación (T-04) | receptor muy largo en LabVIEW; se retiró el 2026-11-04 |
| 500 líneas/s | 1000 líneas/s | 96 % de la línea, sin margen |
| Línea sin nombres | `I:2048,Q:2051` | 15 B: 65,6 % de la línea |
| Disparo por Timer0 | `__delay_ms` / disparo por software / Timer1 | prohibido o con jitter; Timer1 necesita recarga por software |
| Un ADC en ráfaga | alternar canales a 2 kHz | 500 µs de sesgo entre I y Q (6 % a 20 Hz) |
| `VREF = VDD` | FVR 2,048 V / 4,096 V / `VREF+` externo | recorta / no puede superar `VDD` / RA1 es del programador |
| Sin promedio | promedio de 5 pares | la persona quiere el ADC crudo para histograma y SINAD |
| PWM6/PWM7 | PWM3/PWM4 / CCP en modo PWM | PWM3/4 no existen en este chip; los CCP sirven, pero PWM6/7 son más simples |
| Buffers 64/32 | 8/8 (MCC) | no cabe una línea |
| ISR con llamadas directas | punteros de MCC | indirección inútil dentro de la ISR |

---

## 9. Verificación: qué está probado y qué no

| Qué | Estado | Prueba (`docs/protocolos_prueba.md`) |
|---|---|---|
| Compilación | XC8 2.40: 0 errores, solo avisos 520 | – |
| ID del chip | 0x30D2, revisión 0x2002 | 0.3 (pasó) |
| Eco UART a 115200 | 20/20, 256/256, 1000/1000 (141 ms) | 1.5, 1.6, 1.7, 1.18 (pasaron) |
| Envío ASCII | funciona según la persona | 2.1 a 2.6 [POR MEDIR con 500 líneas/s] |
| Periodo de muestreo | – | 4.5 [POR MEDIR] (RC2) |
| Unidad de `ADACQ` | – | 4.16 [POR MEDIR] |
| Duración de la ISR del ADC | – | 4.13 [POR MEDIR] (`ADC_PIN_PRUEBA = 1`) |
| Ruido (histograma) | – | 4.3 [POR MEDIR] |
| SINAD > 60 dB | – | 4.15 [POR MEDIR] |
| Jitter de Q | – | 4.17 [POR MEDIR] |
| PWM y filtro de reconstrucción | filtro sin montar | 3.1 a 3.8 [POR MEDIR] |
| Aliasing | sin filtro anti-aliasing | 4.7 pendiente |
| Contadores `adcParesPerdidos` y `adcLineasPerdidas` | deben quedar en 0 | 2.4, 4.14 |

---

## 10. Limitaciones, riesgos y pendientes

1. **Sin filtro anti-aliasing** (R12): no se cumple todavía el criterio `fc ≤ fs/2` del enunciado.
2. **Jitter de Q** arrancado por software: no se puede garantizar σ < 10 ns.
3. **Unidad de `ADACQ`**: con 320 µs el sesgo I/Q sube a 4,4 % a 20 Hz.
4. **`Zout` del opamp de PCB2** desconocida (el MCP604 no la publica): entra en `RS` y en `TACQ`.
5. **SINAD > 60 dB**: falta confirmar con el profesor si es del ADC a escala completa (lo adoptado) o con la señal real de PCB2 (donde el ideal es 54 dB).
6. **RX en Schmitt** en vez de TTL (§3.3).
7. **Capa física**: protoboard con divisor contra placa con SN74LVC1T45; confirmar cuál se entrega.
8. **Exactitud del oscilador** (±2 %) mayor que el ±1 % pedido a `fs` (prueba 4.5); se corrige con `OSCTUNE` si hace falta.
9. **Sin detección de líneas perdidas** en el PC (no hay número de secuencia).
10. Lo [ESTIMADO] de los tiempos de CPU (≈ 280 µs para armar una línea) se mide con un pin y el osciloscopio.

---

## 11. Preguntas de evaluación (como las haría el profesor)

Cada pregunta trae una respuesta corta y dónde está el detalle. Sirven para preparar la sustentación.

### Reloj y temporización

1. **¿Por qué 16 MHz y no 32 MHz?**
   Da −0,79 % de error a 115200 con el consumo más bajo que lo logra; la CPU tiene poca carga. 32 MHz daría +0,64 %, pero sin necesidad (§4.1).
2. **Muestre la cuenta del periodo del Timer0.**
   Fosc/4 = 4 MHz; /16 = 250 kHz → 4 µs por cuenta; `TMR0H` = 249 → 250 cuentas → 1000 µs (§4.2).
3. **¿Por qué en RC2 se ve una cuadrada de 500 Hz si el muestreo es de 1 kHz?**
   El pin recibe `TMR0_out` a través de un flip-flop que cambia en cada coincidencia: un flanco cada 1 ms. Al ADC le llega el pulso de cada coincidencia, sin el flip-flop (Figura 18-1, DS p.260). Cada flanco de RC2 es un disparo.
4. **¿El disparo del ADC tiene jitter de software? ¿Por qué no recargan el Timer0 en una ISR?**
   No para I: el Timer0 en modo de 8 bits se reinicia solo y el ADC arranca por hardware. Recargarlo en una ISR sumaría la latencia variable de la interrupción a cada periodo.
5. **¿Por qué no usaron el Timer2 para el ADC?**
   Es la base del PWM; con `T2PR` = 255 su periodo es de 64 µs.

### ADC

6. **¿Cuánto vale un LSB y el ruido de cuantización?**
   3,7/4096 = 0,903 mV; σq = LSB/√12 = 261 µV. Los 352 µV del profesor corresponden a 5 V.
7. **¿Por qué `VREF = VDD` y no el FVR?**
   Cubre todo el rango, no usa pines y es ratiométrica con `Vref2`. El FVR de 2,048 V recortaría y el de 4,096 V supera `VDD` (§4.4.3).
8. **Escriba la ecuación del tiempo de adquisición y diga qué es `RS`.**
   `TACQ = 2 µs + CHOLD·(RIC + RSS + RS)·ln(8191) + 0,05·(T − 25)`. `RS` es la impedancia que ve el pin: salida del opamp más lo que haya en serie (§4.4.5).
9. **¿Por qué importa la impedancia de salida del opamp si es «baja»?**
   Sube con la frecuencia (`Ro/(1 + Aβ)`) y el golpe de carga de `CHOLD` es rápido; además el opamp tiene que recuperarse en el tiempo de adquisición. Hay que simularla o medirla.
10. **¿Cuánto tarda una conversión? ¿Y I + Q?**
    14·`TAD` + 2·`TCY` = 28,5 µs. I + Q: 77 µs (adquisición de 10 µs) o 697 µs (de 320 µs); los dos caben en 1 ms.
11. **¿Qué significa `ADACQ` = 160 y por qué no lo saben con certeza?**
    El texto del registro dice periodos del reloj del ADC (320 µs); la Fig. 32-2 dice `TCY` (40 µs); MCC guarda 10 µs. Se mide con la separación entre los pulsos de la ISR en RC2 (prueba 4.16).
12. **¿Cómo muestrean dos canales con un solo ADC? ¿Qué error introduce?**
    En ráfaga: I por hardware y Q por la ISR. Q queda `Δt` después: 0,48 % (0,28°) a 20 Hz en el caso A (§4.4.6).
13. **¿Por qué Q no se dispara también por hardware?**
    `ADACT` solo elige una fuente. Se podría con un segundo evento (CLC, otro timer), pero no se estudió.
14. **¿Dónde está el filtro anti-aliasing? ¿Qué pasa con una señal de 600 Hz?**
    No hay en esta entrega. 600 Hz se pliega en el ADC a |600 − 1000| = 400 Hz; en el PC, a |400 − 500| = 100 Hz (§4.4.7).
15. **¿Por qué una entrada de 500 Hz se ve como continua en el PC?**
    El PC recibe 500 muestras/s: cada muestra cae en la misma fase de la señal (alias a 0 Hz).
16. **¿Cuál es el SINAD ideal de 12 bits y cuánto ruido permite el requisito de 60 dB?**
    74 dB; ruido + distorsión ≤ 1,31 mV rms = 1,45 LSB rms con una senoidal a escala completa (§4.4.9).
17. **Con la señal real de PCB2, ¿se alcanzan los 60 dB?**
    No: ±185 mV es el 10 % del rango y el ideal baja a 54 dB. Por eso el requisito se mide sobre el ADC con una senoidal casi a escala completa.
18. **¿Qué jitter tiene cada canal? ¿Cumplen 10 ns?**
    I: el del oscilador (sin dato del fabricante). Q: la latencia de interrupción (0,75 a 1 µs, más si coincide con la UART). Q no puede garantizar 10 ns; para el SINAD a 20 Hz bastarían ≈ 2 µs.
19. **¿Cómo medirían el histograma del periodo de 10 000 muestras?**
    No con la hora de llegada en LabVIEW (mide el USB). Con osciloscopio o contador sobre RC2 (10 000 periodos), exportados a LabVIEW; o haciendo que el PIC mida su periodo con el Timer1 (propuesta).
20. **¿Por qué quitaron el promedio?**
    Para que cada línea traiga el ADC crudo (histograma y SINAD). Cuesta hasta 7 dB de ruido.

### PWM

21. **Muestre el periodo y la resolución del PWM.**
    (255 + 1)·4·62,5 ns = 64 µs → 15,625 kHz; log2(4·256) = 10 bits; paso de 3,61 mV (§4.3).
22. **¿Por qué `duty = (código + 2) >> 2`?**
    Pasa de 12 a 10 bits dividiendo entre 4 y redondeando (el +2 es medio paso); se recorta a 1023.
23. **¿Por qué la salida filtrada tiene ganancia 1 sin calibrar?**
    El ADC mide contra `VDD` y el PWM sale entre 0 y `VDD`: código 2048 → duty 512 → 1,85 V.
24. **¿Qué pasa si el hardware actualiza el duty entre la escritura de `DCH` y `DCL`?**
    Un solo periodo de 64 µs sale con un valor mezclado (error ≤ 3 pasos); el filtro lo borra. Por eso no se apagan interrupciones.
25. **¿Por qué el filtro de reconstrucción corta en ≈ 100 Hz y no en 20 Hz?**
    Tiene que dejar pasar la señal de 20 Hz sin atenuar ni desfasar mucho y aun así atenuar los escalones de 1 kHz y el PWM de 15,6 kHz.
26. **¿Por qué el SINAD de la salida reconstruida (≈ 54 dB) es menor que el de un DAC ideal de 10 bits (62 dB)?**
    El PWM de flanco final mueve el centro del pulso con el duty: aparece un 2.º armónico.
27. **¿Por qué PWM6/PWM7 y no PWM3/PWM4 como en los apuntes?**
    Este chip no tiene PWM3 ni PWM4.

### UART y formato

28. **Calcule `SP1BRG` y el error. ¿Por qué no usaron la fórmula de clase?**
    16 MHz/(4·115200) − 1 = 33,72 → 34; 114 286 baud, −0,79 %. La fórmula con `BRG16` = 0 da −3,55 %.
29. **¿Cuánto error de baud aguanta la UART?**
    ±5,3 % teórico (medio bit en 9,5 bits); se reserva la mitad. El peor caso con el oscilador es ≈ −2,8 %.
30. **¿Por qué 115200 si el enunciado sugiere 9600?**
    5500 B/s no caben a 9600 (960 B/s).
31. **¿Cuánto tarda una línea y qué porcentaje de la UART ocupan?**
    11 bytes × 87,5 µs = 962,5 µs; 500 por segundo = 48,1 %.
32. **¿Por qué mandan solo 1 de cada 2 pares?**
    Una línea por par ocuparía el 96 % de la línea: sin margen para el mensaje de arranque o un atraso.
33. **¿Cómo detecta el PC una línea dañada o perdida?**
    Dañada: no tiene la forma `número,número` y se descarta. Perdida: no se detecta (no hay número de secuencia); se vigila `adcLineasPerdidas` y que lleguen 500 líneas/s.
34. **¿Cómo convierten a decimal sin `printf`?**
    Restas sucesivas de 1000, 100, 10 y 1; las veces que cabe son la cifra; `'0' + cifra` es el ASCII (§4.7).
35. **¿Por qué los buffers son de 64 y 32 y potencia de 2?**
    TX: 5 líneas de margen. RX: no se usa hoy. Potencia de 2: el índice se envuelve con una máscara, sin dividir dentro de la ISR.
36. **¿Por qué no usan un contador de bytes en el buffer?**
    Lo escribirían `main` y la ISR: haría falta apagar interrupciones. Con `head`/`tail` cada índice tiene un solo escritor.
37. **¿Qué devuelve `tx_DAT` si el bloque no cabe completo?**
    `false` y no encola nada (todo o nada): nunca sale media línea.
38. **¿Por qué `TX1IE` no queda siempre encendido?**
    `TX1IF` vale 1 siempre que `TX1REG` esté vacío: la ISR se repetiría sin parar.
39. **¿Qué pasa si hay overrun en la RX?**
    `OERR` = 1 y el receptor se detiene; la ISR lo reinicia con `CREN` 0 → 1.

### Interrupciones y estructura del código

40. **¿Cuántas ISR tiene el programa y en qué orden atiende las fuentes?**
    Una (vector único). Orden: TX, RX, ADC, una por entrada.
41. **¿Por qué `adc_iq_get` apaga solo `ADIE` y no `GIE`?**
    Para copiar dos variables de 16 bits sin que la ISR del ADC las cambie a mitad, sin frenar la UART. Si el ADC termina en ese momento, `ADIF` queda arriba y la ISR corre al volver a encender `ADIE`.
42. **¿Por qué `PEIE` y `GIE` se encienden al final de `main`?**
    Para que ninguna interrupción llegue con un módulo a medio configurar.
43. **¿Dónde hay una espera bloqueante en su código?**
    En ninguna parte: no hay `while` que espere ni `__delay_ms`. Los bucles de `a_decimal` y `tx_DAT` son cortos y acotados.
44. **¿Qué pasa si `main` tarda más de 1 ms en leer un par?**
    Ese par se pierde y se cuenta en `adcParesPerdidos` (debe quedar en 0).
45. **¿Qué variables son `volatile` y por qué?**
    Las que cambia la ISR y lee `main` (índices de los buffers, `salidaI/Q`, `listo`, contadores): sin `volatile`, el compilador podría guardar una copia vieja en un registro.

### MCC y decisiones de proceso

46. **¿Qué parte del código es de MCC y qué parte escribieron ustedes?**
    MCC: inicialización (reloj, pines, ADC, Timer0, UART) y la estructura de la ISR. A mano: `adc_iq`, `adc_ascii`, `pwm`, `main`, y los cambios C-01 a C-12 (§7).
47. **¿Qué cambiaron del código de MCC y por qué?**
    Quitar esperas (C-07, C-08), quitar contadores compartidos (C-09), llamadas directas en la ISR (C-06, C-11), buffers más grandes (C-05), `PPS1WAY` y `FCMEN` en OFF (C-01, C-02), canal inicial del ADC (C-12).
48. **¿Por qué no pueden volver a pulsar *Generate* en MCC?**
    Reescribiría los archivos editados a mano y el `.mc3` todavía tiene `PPS1WAY` y `FCMEN` en ON.
49. **¿Qué pasa con el auto-cero que pide el enunciado?**
    Pasa a Macro por decisión del profesor (2026-11-04); en Micro 3 el PWM reconstruye las señales muestreadas.

### Hardware y medición

50. **¿Cómo conectan el TX del PIC (3,7 V) a un adaptador de 3,3 V?**
    Protoboard: divisor 1 kΩ + 2,2 kΩ → 2,54 V. Placa: traductor SN74LVC1T45.
51. **¿Qué nivel de entrada tiene el RX y por qué importa?**
    El código lo deja en Schmitt (VIH = 2,96 V). Con TTL serían 1,73 V, más margen frente a los 3,3 V del CP2102 (§3.3).
52. **¿Cómo medirían el SINAD y con qué señal?**
    Generador directo al pin (≤ 10 kΩ), senoidal de 1,7 V centrada en 1,85 V a 5 y 20 Hz, 4096 pares a 500/s, FFT con ventana de Kaiser; criterio ≥ 60 dB en I y en Q.
53. **¿Cómo comprueban que el muestreo es de 1 kHz y no de 500 Hz?**
    Que el PC reciba ≈ 500 líneas/s (con 1 línea cada 2 pares); o, con `ADC_PIN_PRUEBA` = 1, ver en RC2 un par de pulsos (I y Q) cada 1 ms.
54. **¿Qué dejaron accesible para la «fuente física en un pin»?**
    La alimentación entra por J1 y está en TP3 (VAA) y en el pin 2 de J4 (según el esquemático de la placa Micro3); confirmar en la placa.

---

## 12. Referencias

**Documentos del proyecto (`docs/`):** `especificacion_adc.md` (ADC completo), `especificacion_pwm.md` (PWM y filtro de reconstrucción), `especificacion_protocolo.md` (líneas ASCII), `especificacion_diseno_uart.md` (pines, periféricos, eventos), `criterios_diseno.md` (por qué de cada valor), `notas_pic16f18426.md` (datos del chip con páginas), `revision_codigo_mcc.md` (cambios C-xx), `protocolos_prueba.md` (pruebas), `especificaciones_preinforme.md` (preinforme), `estado.md` (avance), `MASTER_PLAN.md` (tabla «Qué tiene esta entrega»).

**Fuentes externas:**
- Microchip, *PIC16(L)F18426/46 Data Sheet*, DS40001985C: Timer0 cap. 18 (Fig. 18-1, p.260); PWM cap. 24; EUSART cap. 28 (Tabla 28-2); ADC2 cap. 32 (§32.2.6, Ec. 32-1 p.506–507, `ADACQ` p.529, Tabla 32-6 p.538); parámetros AD01–AD23 (p.632–633).
- Microchip, *PIC16(L)F18426/46 Silicon Errata*, DS80000799.
- Microchip, *MCP601/1R/2/3/4 Data Sheet*, DS21314G.
- Microchip, *MPLAB XC8 C Compiler User's Guide for PIC*, DS50002737.
- Texas Instruments, *Fundamentals of Precision ADC Noise Analysis*, SLYY192A (2025).
- Preinforme de la Práctica 2 (PCB1 y PCB2: filtros de 20 Hz, `Vref2`, ecuaciones 5 y 8).
