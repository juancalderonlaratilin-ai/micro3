# Diagramas de tiempo y plan del histograma del periodo

Proyecto Micro 3 · 2026-11-03 · Firmware: ADC a 1 kHz sin promedio, tramas binarias `DATA_IQ` de 9 bytes a 115200.

**Etiquetas:** **[DS]** = datasheet del PIC16F18426; **[CÁLCULO]** = cuenta exacta con los registros del código; **[ESTIMADO]** = cuenta aproximada de instrucciones de XC8 (no hay listado del código actual), hay que medirla. En el preinforme, lo [ESTIMADO] va como «previsto» hasta medirlo.

## 1. Tiempos de cada etapa (requisito 2 del profesor)

| Etapa | Tiempo | Etiqueta | De dónde sale |
|---|---|---|---|
| Periodo de muestreo | **1000 µs** | [CÁLCULO] | Timer0: Fosc/4 = 4 MHz, prescaler 1:16, `TMR0H` = 249 → 250 × 4 µs |
| Disparo de la conversión de I | 0 µs de software | [DS] | `ADACT` = TMR0 (hardware, sin jitter de software) |
| **Adquisición** (S&H conectado) | 10 µs (caso A); 40 µs o 320 µs según la unidad de `ADACQ` = 160 | [DS] / por medir | prueba 4.16 |
| **Conversión del ADC** (por canal) | **28,5 µs** = 14 TAD + 2 TCY, TAD = 2 µs | [DS] AD22 | `ADCLK` = Fosc/32 |
| Latencia de interrupción | 0,75 a 1 µs (3–4 TCY) | [DS] p.99 | |
| **Lectura del ADC** (ISR: leer `ADRESH:ADRESL`, guardar, cambiar canal, arrancar Q) | ≈ 10 µs la de I y ≈ 11 µs la de Q | [ESTIMADO] | prueba 4.13 (ancho del pulso del marcador) |
| Ráfaga completa I + Q | ≈ 97 µs (caso A) | [ESTIMADO] | margen de ≈ 90 % sobre 1 ms |
| Sesgo entre la muestra de I y la de Q | ≈ 46 µs (caso A) | [ESTIMADO] | |
| `adc_iq_get()` en main | ≈ 5 µs | [ESTIMADO] | |
| **Conversión de los datos** (binario: `I>>8`, `I&0xFF`, …) | ≈ 2,5 µs | [ESTIMADO] | no hay ASCII en el protocolo |
| Conversión a decimal/ASCII (solo modo Serial Plotter) | ≈ 95 µs por valor de 4 cifras | [ESTIMADO] | `a_decimal()`: restas de 1000, 100, 10 y 1 |
| **Armado de la trama**: cabecera y payload | ≈ 7,5 µs | [ESTIMADO] | `frame_send()` |
| CRC-8 bit a bit sobre 6 bytes | ≈ 150 µs (100 a 200) | [ESTIMADO] | `crc8()`: 48 vueltas, depende de los datos |
| Encolar 9 bytes (`tx_DAT`) | ≈ 50 µs | [ESTIMADO] | |
| **De «par listo» a «trama encolada»** | **≈ 215 µs** | [ESTIMADO] | |
| Bit en la línea | 8,75 µs (8,68 µs a 115200 exactos) | [CÁLCULO] | `SP1BRG` = 34 → 114 286 baud |
| Byte 8N1 (10 bits) | 87,5 µs | [CÁLCULO] | |
| Trama de 9 bytes | **787,5 µs** = 78,8 % del periodo | [CÁLCULO] | |
| ISR de TX (una por byte) | ≈ 6 µs | [ESTIMADO] | |
| Línea ASCII `I:2048,Q:2051\r\n` | 15 bytes = 1312,5 µs | [CÁLCULO] | no cabe a 1 kHz: por eso el Serial Plotter va a 20 líneas/s |

## 2. Figuras

| Archivo | Qué muestra |
|---|---|
| `D1_periodo_1ms` | El periodo completo: Timer0, ADC, interrupciones, main, línea UART y marcador |
| `D2_rafaga_IQ` | Zoom de 0 a 110 µs: adquisición, conversión, latencia, ISR, cambio de canal, sesgo I–Q |
| `D3_armado_trama` | Conversión de los datos, CRC y encolado de la trama |
| `D4_byte_y_trama` | El byte 0xA5 bit a bit en 8N1 y la trama completa `A5 5A A1 05 08 00 08 03 02` |
| `D5_camino_ASCII` | La alternativa en ASCII frente al binario |

Cada figura está en PNG (para Word) y en SVG (se escala sin perder calidad).

**Un detalle que muestra D1:** la trama de 787,5 µs empieza unos 315 µs después del disparo, así que termina pasado el siguiente GO. La última ISR de TX cae hacia los 1016 µs, cerca de la ráfaga siguiente. Si el CRC tarda un poco más o un poco menos (depende de los datos), esa ISR puede coincidir con la ISR de I y retrasar el arranque de Q unos 6 µs. Es la principal fuente de jitter de Q, y el histograma del §4 la debería mostrar.

## 3. Cómo cambiar lo [ESTIMADO] por medido

Con un pin marcador y el osciloscopio:

1. **ISR del ADC (pruebas 4.13 y 4.16):** el marcador que ya existe (`ADC_PIN_PRUEBA`). En la placa nueva RC2 es `EN_AZ_PIC`: medir con JP_AZEN en 1–2, en el pin 3 de JP_AZEN.
2. **Armado de la trama:** subir un pin al entrar a `protocol_send_iq()` y bajarlo al salir. Para separar el CRC, otro pulso alrededor de `crc8()`.
3. **Conversión ASCII:** igual, alrededor de `a_decimal()` en `adc_prueba.c`.
4. Alternativa sin osciloscopio: el *Stopwatch* del simulador de MPLAB X cuenta los ciclos exactos de una función.

Tiempo = ancho del pulso. Con eso se reemplazan los valores de la tabla y se redibujan las figuras.

## 4. Histograma del periodo con 10 000 muestras en LabVIEW

### Por qué no sirve marcar la hora en LabVIEW

Si LabVIEW anota la hora de llegada de cada trama, mide el USB y Windows, no el PIC: el adaptador USB-serie entrega los bytes en paquetes con latencias de milisegundos. El histograma saldría con un σ de milisegundos que no tiene nada que ver con el ADC.

### Propuesta: que el PIC mida su propio periodo y lo mande en la trama

**Idea:** un Timer1 libre, contando a Fosc = 16 MHz (62,5 ns por cuenta). En cada ISR de I se lee el Timer1 y se resta la lectura anterior. El periodo ideal es **16 000 cuentas**. Ese valor viaja en el campo Q de la trama (16 bits, cabe), y LabVIEW junta 10 000.

**Qué mide:** el periodo entre las entradas a la ISR de I. Ese es el instante en que el software arranca Q, así que es el periodo real de la muestra de Q. Incluye la latencia de interrupción (3–4 TCY, o sea ±4 cuentas) y los bloqueos por la ISR de TX (≈ 6 µs ≈ 96 cuentas).

**Qué no mide:**
- El jitter del oscilador: Timer0 y Timer1 usan el mismo reloj, así que ese jitter se cancela.
- Nada por debajo de 62,5 ns, así que no puede probar el «σ < 10 ns».

Para el instante de I, que lo fija el hardware, hace falta el método de respaldo de abajo.

**Cambio de firmware (propuesta, sin aplicar):**

```c
/* adc_iq.h */
#define MODO_PERIODO 0   /* 1 = en el campo Q de DATA_IQ va el periodo medido con Timer1 (cuentas de 62,5 ns) */

/* adc_iq_init(), antes de encender el Timer0 */
#if MODO_PERIODO
    T1CLK = 0x02;  /* reloj de Timer1 = Fosc (16 MHz). VERIFICAR el código de CS en el datasheet y en pic16f18426.h */
    T1CON = 0x03;  /* prescaler 1:1, RD16 = 1 (lectura de 16 bits segura), ON = 1 */
#endif

/* adc_iq_isr(), rama ESTADO_I, primera línea */
#if MODO_PERIODO
    uint16_t t;
    t  = TMR1L;                     /* leer primero L: con RD16 = 1 congela H */
    t |= (uint16_t)TMR1H << 8;
    periodo = (uint16_t)(t - tAnterior);   /* la resta de 16 bits ya resuelve el desborde (cada 4,1 ms) */
    tAnterior = t;
#endif

/* adc_iq_isr(), rama ESTADO_Q, al pasar el par a la salida */
#if MODO_PERIODO
    salidaQ = periodo;              /* ≈ 16000 en vez del código de Q */
#else
    salidaQ = resultado;
#endif
```

(`periodo` y `tAnterior` son `static volatile uint16_t` de `adc_iq.c`.) La primera muestra no vale, porque `tAnterior` arranca en 0: se descarta.

**El VI de LabVIEW, paso a paso:**

1. **VISA Configure Serial Port:** 115200, 8 bits, sin paridad, 1 de parada, sin control de flujo y **Enable Termination Char = False** (la trama es binaria y puede traer un 0x0A).
2. **Encender el envío:** VISA Write de los bytes `A5 5A 20 00 01 00 00 00 2D` (`ADC_STREAM` = 1, CRC ya calculado). El PIC responde `ADC_ACK` (tipo `A0`).
3. **Bucle de lectura:** VISA Read con *Bytes at Port*, concatenar a un buffer, buscar `A5 5A`, tomar 9 bytes y comprobar el CRC-8 (polinomio 0x07, sobre los bytes 2 a 7, en un *Formula Node* o subVI). Si `TYPE` = `A1`, guardar `SEQ` y `Q = P2·256 + P3` (U16, **sin** recortar a 12 bits). Repetir hasta tener 10 001 valores (10 s a 1000 tramas/s).
4. **Validez:** si `SEQ` salta (se perdió una trama), ese periodo no vale: descartarlo o repetir la captura.
5. **Apagar el envío:** `A5 5A 20 01 00 00 00 00 59`.
6. **Análisis:** quitar el primer valor, multiplicar por 62,5 para pasar a ns, y usar **Histogram.vi** (paleta *Mathematics → Probability & Statistics*) con un bin por cuenta (bins = máx − mín + 1), **Std Deviation and Variance.vi** para σ y **Mean.vi** para la media (debe dar 1 000 000 ns). Mostrarlo en un *XY Graph* y guardar con *Write Delimited Spreadsheet*.

**Qué esperar [ESTIMADO]:** casi todo en 16 000 ± 4 cuentas (± 250 ns, la latencia de 3–4 TCY), y dos colas en torno a ±96 cuentas (≈ ±6 µs) cuando la ISR de TX coincide con la del ADC: cada retraso alarga un periodo y acorta el siguiente en lo mismo. Por eso la media sigue en 16 000, y el σ cuenta cuántas veces pasa.

### Método de respaldo para el «σ < 10 ns» (instante de I, hardware)

- Sacar la salida del Timer0 a un pin por PPS (el datasheet lo permite, p.262; el código de `RxyPPS` está por verificar). En esta placa no hay pin libre: usar temporalmente RA5 (LED) con JP_LED abierto.
- Medir el periodo con un **osciloscopio o contador** con estadística (10 000 mediciones, resolución ≤ 1 ns). Exportar las mediciones a CSV y cargarlas en LabVIEW con *Read Delimited Spreadsheet*: el mismo Histogram.vi y Std Deviation.vi.
- Así se cumple «10 000 muestras en LabVIEW» con una medición que sí resuelve nanosegundos.

### Si el profesor se refería al histograma de códigos (ruido)

Es el mismo VI sin cambiar el firmware: entrada fija (J7 a GND, o a Vref2), 10 000 tramas, histograma de I (o de Q) con un bin por código, y σ en LSB y en µV (1 LSB = 0,903 mV). Detalle en `docs/especificaciones_preinforme.md` §8.1.
