# Diagramas de tiempo y plan del histograma del periodo

Proyecto Micro 3 · 2026-11-04 · Firmware: ADC a 1 kHz sin promedio; envío en texto ASCII `xxxx,xxxx\r\n`, una línea cada 2 pares (500 líneas/s) a 115200.

**Etiquetas:** **[DS]** = datasheet del PIC16F18426; **[CÁLCULO]** = cuenta exacta con los registros del código; **[ESTIMADO]** = cuenta aproximada de instrucciones de XC8 (no hay listado del código actual), hay que medirla. En el preinforme, lo [ESTIMADO] va como «previsto» hasta medirlo.

## 1. Tiempos de cada etapa (requisito 2 del profesor)

| Etapa | Tiempo | Etiqueta | De dónde sale |
|---|---|---|---|
| Periodo de muestreo | **1000 µs** | [CÁLCULO] | Timer0: Fosc/4 = 4 MHz, prescaler 1:16, `TMR0H` = 249 → 250 × 4 µs |
| Disparo de la conversión de I | 0 µs de software | [DS] | `ADACT` = TMR0 (hardware, sin jitter de software) |
| **Adquisición** (S&H conectado) | 10 µs (caso A); 40 µs o 320 µs según la unidad de `ADACQ` = 160 | [DS] / por medir | prueba 4.16 |
| **Conversión del ADC** (por canal) | **28,5 µs** = 14 TAD + 2 TCY, TAD = 2 µs | [DS] AD22 | `ADCLK` = Fosc/32 |
| Latencia de interrupción | 0,75 a 1 µs (3–4 TCY) | [DS] p.99 | |
| **Lectura del ADC** (ISR: leer `ADRESH:ADRESL`, guardar, cambiar canal, arrancar Q) | ≈ 10 µs la de I y ≈ 11 µs la de Q | [ESTIMADO] | prueba 4.13 (`ADC_PIN_PRUEBA` = 1) |
| Ráfaga completa I + Q | ≈ 97 µs (caso A) | [ESTIMADO] | margen de ≈ 90 % sobre 1 ms |
| Sesgo entre la muestra de I y la de Q | ≈ 46 µs (caso A) | [ESTIMADO] | |
| `adc_iq_get()` y contar el par | ≈ 5 µs | [ESTIMADO] | |
| **Conversión a decimal/ASCII** | ≈ 95 µs por valor de 4 cifras | [ESTIMADO] | `a_decimal()`: restas de 1000, 100, 10 y 1 |
| Texto fijo (`,` y `\r\n`) | ≈ 5 µs | [ESTIMADO] | |
| Encolar 11 bytes (`tx_string`) | ≈ 80 µs | [ESTIMADO] | largo de la cadena + copia |
| **De «par listo» a «línea encolada»** | **≈ 280 µs** | [ESTIMADO] | solo cada 2 pares; en el otro tick, ≈ 5 µs |
| Bit en la línea | 8,75 µs (8,68 µs a 115200 exactos) | [CÁLCULO] | `SP1BRG` = 34 → 114 286 baud |
| Byte 8N1 (10 bits) | 87,5 µs | [CÁLCULO] | |
| Línea `2048,2051\r\n` (11 bytes) | **962,5 µs** = 48,1 % de 2 ms | [CÁLCULO] | una por par (cada 1 ms): 96 %, sin margen |
| ISR de TX (una por byte) | ≈ 6 µs | [ESTIMADO] | |

## 2. Figuras

| Archivo | Qué muestra |
|---|---|
| `D1_periodo_2ms.png` | Dos periodos: Timer0, ADC, interrupciones, main, la línea en la UART y RC2 (salida del Timer0) |
| `D2_rafaga_IQ.png` | Zoom de 0 a 110 µs: adquisición, conversión, latencia, ISR, cambio de canal, sesgo I–Q |
| `D3_armado_linea.png` | `a_decimal` de I y de Q, texto fijo y `tx_string` |
| `D4_byte_y_linea.png` | El byte `'2'` (0x32) bit a bit en 8N1 y la línea completa con cada byte en hexadecimal |
| `D5_ocupacion_linea.png` | Una línea por par deja la UART ocupada el 96 %; una cada 2 pares, el 48 % |

Se generan con `matlab/generar_diagramas_tiempo.m` (cómo: `como_se_hicieron_los_diagramas.md`). Las mismas imágenes están en `docs/informe/figuras/`.

**Un detalle que muestra D1:** la línea de 962,5 µs empieza unos 380 µs después del disparo y termina hacia los 1345 µs, así que sus ISR de TX caen encima de la ráfaga siguiente (la de 1000 µs). Si una coincide con la ISR de I, retrasa el arranque de Q unos 6 µs. Es la principal fuente de jitter de Q, y el histograma del §4 la debería mostrar.

## 3. Cómo cambiar lo [ESTIMADO] por medido

Con un pin marcador y el osciloscopio:

1. **ISR del ADC (pruebas 4.13 y 4.16):** `ADC_PIN_PRUEBA` = 1 en `adc_iq.h` (RC2 sube durante cada ISR). En la placa nueva RC2 es `EN_AZ_PIC`: medir con JP_AZEN en 1–2, en el pin 3 de JP_AZEN.
2. **Periodo de muestreo (prueba 4.5):** `ADC_PIN_PRUEBA` = 2: RC2 saca la salida del Timer0, una cuadrada de 2 ms de periodo (cada flanco es un disparo).
3. **Armado de la línea:** subir un pin al entrar a `adc_ascii_par()` cuando toca mandar y bajarlo al salir; para separar la conversión, otro pulso alrededor de `a_decimal()` en `adc_ascii.c`.
4. **La línea en la UART:** osciloscopio en RC4 (TX): una ráfaga de 11 bytes de ≈ 0,96 ms cada 2 ms.
5. Alternativa sin osciloscopio: el *Stopwatch* del simulador de MPLAB X cuenta los ciclos exactos de una función.

Tiempo = ancho del pulso. Con eso se reemplazan los valores de la tabla y se redibujan las figuras.

## 4. Histograma del periodo con 10 000 muestras en LabVIEW

### Por qué no sirve marcar la hora en LabVIEW

Si LabVIEW anota la hora de llegada de cada línea, mide el USB y Windows, no el PIC: el adaptador USB-serie entrega los bytes en paquetes con latencias de milisegundos. El histograma saldría con un σ de milisegundos que no tiene nada que ver con el ADC.

### Propuesta: que el PIC mida su propio periodo y lo mande en lugar de Q

**Idea:** un Timer1 libre, contando a Fosc = 16 MHz (62,5 ns por cuenta). En cada ISR de I se lee el Timer1 y se resta la lectura anterior. El periodo ideal es **16 000 cuentas**. Como `a_decimal()` escribe hasta 4 cifras, se manda la diferencia con 16 000 corrida a la mitad de escala: `Q = periodo − 16000 + 2048` (de 1 a 4095 = ±128 µs alrededor de 1 ms). El PC hace la cuenta al revés y junta 10 000.

**Qué mide:** el periodo entre las entradas a la ISR de I. Ese es el instante en que el software arranca Q, así que es el periodo real de la muestra de Q. Incluye la latencia de interrupción (3–4 TCY, o sea ±4 cuentas) y los bloqueos por la ISR de TX (≈ 6 µs ≈ 96 cuentas).

**Qué no mide:**
- El jitter del oscilador: Timer0 y Timer1 usan el mismo reloj, así que ese jitter se cancela.
- Nada por debajo de 62,5 ns, así que no puede probar el «σ < 10 ns».

Para el instante de I, que lo fija el hardware, hace falta el método de respaldo de abajo.

**Cambio de firmware (propuesta, sin aplicar):**

```c
/* adc_iq.h */
#define MODO_PERIODO 0   /* 1 = en lugar de Q se manda el periodo medido con Timer1: Q = periodo - 16000 + 2048 */

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
    salidaQ = (uint16_t)(periodo - 16000u + 2048u);   /* ≈ 2048; fuera de 0 a 4095 si el periodo se va más de 128 µs */
#else
    salidaQ = resultado;
#endif
```

(`periodo` y `tAnterior` son `static volatile uint16_t` de `adc_iq.c`.) La primera muestra no vale, porque `tAnterior` arranca en 0: se descarta. A 500 líneas/s llega el periodo de 1 de cada 2 milisegundos, que basta para el histograma.

**El VI de LabVIEW, paso a paso** (la base es `docs/guia_labview_paso_a_paso.md`):

1. **VISA Configure Serial Port:** 115200, 8 bits, sin paridad, 1 de parada, sin control de flujo, **Enable Termination Char = True** y `termination char` = 10 (`\n`).
2. **Bucle de lectura:** *VISA Read* (una línea por vuelta) y *Scan From String* con `%d,%d`. Si falla, descartar la línea. Del segundo número: `periodo = Q − 2048 + 16000`. Repetir hasta tener 10 001 valores (20 s a 500 líneas/s).
3. **Validez:** no hay número de secuencia; revisar en MPLAB X que `adcLineasPerdidas` quede en 0 y que no haya líneas malas.
4. **Análisis:** quitar el primer valor, multiplicar por 62,5 para pasar a ns, y usar **Histogram.vi** (paleta *Mathematics → Probability & Statistics*) con un bin por cuenta (bins = máx − mín + 1), **Std Deviation and Variance.vi** para σ y **Mean.vi** para la media (debe dar 1 000 000 ns). Mostrarlo en un *XY Graph* y guardar con *Write Delimited Spreadsheet*.

**Qué esperar [ESTIMADO]:** casi todo en 16 000 ± 4 cuentas (± 250 ns, la latencia de 3–4 TCY), y dos colas en torno a ±96 cuentas (≈ ±6 µs) cuando la ISR de TX coincide con la del ADC: cada retraso alarga un periodo y acorta el siguiente en lo mismo. Por eso la media sigue en 16 000, y el σ cuenta cuántas veces pasa.

### Método de respaldo para el «σ < 10 ns» (instante de I, hardware)

- La salida del Timer0 ya sale por RC2 con `ADC_PIN_PRUEBA` = 2 (`RC2PPS = 0x17`, código tomado de MCC del PIC16F18446; confirmar en el datasheet, Tabla 16-3).
- Medir el periodo con un **osciloscopio o contador** con estadística (10 000 mediciones, resolución ≤ 1 ns). Cada medio periodo de la cuadrada es un disparo (1 ms). Exportar las mediciones a CSV y cargarlas en LabVIEW con *Read Delimited Spreadsheet*: el mismo Histogram.vi y Std Deviation.vi.
- Así se cumple «10 000 muestras en LabVIEW» con una medición que sí resuelve nanosegundos.

### Si el profesor se refería al histograma de códigos (ruido)

Es el mismo VI sin cambiar el firmware: entrada fija (J7 a GND, o a Vref2), 10 000 líneas, histograma de I (o de Q) con un bin por código, y σ en LSB y en µV (1 LSB = 0,903 mV). La interfaz de Python lo hace sola (tipo «Continua», `docs/interfaz_mediciones.md` §4). Detalle en `docs/especificaciones_preinforme.md` §8.1.
