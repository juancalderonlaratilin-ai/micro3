# Cómo se obtuvieron los diagramas de tiempo

Proyecto Micro 3 · PIC16F18426 · Firmware con el ADC a 1 kHz sin promedio y envío en texto ASCII (`xxxx,xxxx\r\n`, una línea cada 2 pares = 500 líneas/s) a 115200 baud.

## 1. Qué muestran

| Figura | Archivo | Contenido |
|---|---|---|
| 1 | `D1_periodo_2ms.png` | Dos periodos de muestreo (2 ms): Timer0, ADC, interrupciones, programa principal, la línea en la UART y RC2 con la salida del Timer0 |
| 2 | `D2_rafaga_IQ.png` | Detalle de la lectura del ADC: adquisición, conversión, latencia, ISR de I y de Q, cambio de canal |
| 3 | `D3_armado_linea.png` | Conversión a decimal (`a_decimal`) y armado de la línea `2048,2051\r\n` |
| 4 | `D4_byte_y_linea.png` | Un byte 8N1 en la línea (`'2'` = 0x32) y la línea completa de 11 bytes |
| 5 | `D5_ocupacion_linea.png` | Por qué se manda 1 línea cada 2 pares: una por par ocupa el 96 % (sin margen); cada 2 pares, el 48 % |

## 2. De dónde sale cada tiempo

Los tiempos se separan en tres clases. Cada valor de las figuras pertenece a una de ellas.

**a) Datasheet (DS40001985 del PIC16F18426)**
- Conversión de 12 bits: `Tconv = 14·TAD + 2·TCY` (parámetro AD22). Con `ADCLK` = Fosc/32, TAD = 32/16 MHz = 2 µs y TCY = 4/16 MHz = 250 ns → **28,5 µs**.
- Latencia de interrupción: 3 a 4 TCY = 0,75 a 1 µs.

**b) Calculados con los registros del código**
- Periodo de muestreo (Timer0): reloj Fosc/4 = 4 MHz, prescaler 1:16 → 4 µs por cuenta; `TMR0H` = 249 → 250 cuentas → **1000 µs**.
- Baud rate: `BRGH` = 1, `BRG16` = 1, `SP1BRG` = 34 → baud = Fosc / (4·(34 + 1)) = **114 286** (error −0,79 % frente a 115200).
- Bit = 1 / 114 286 = 8,75 µs; byte 8N1 = 10 bits = 87,5 µs; línea de 11 bytes = **962,5 µs**, cada 2 ms = 48,1 % de la línea.
- Adquisición: `ADACQ` = 160. No está claro en qué unidad cuenta el chip: 10 µs si cuenta ciclos de Fosc (lo que guarda MCC), 40 µs si cuenta TCY y 320 µs si cuenta TAD. Las figuras usan **10 µs (caso A)** hasta medirlo con la prueba 4.16.

**c) Estimados (CPU)**
No había un listado del compilador con el código actual, así que la duración de cada función se estimó contando las instrucciones que XC8 genera para ese código en C, a 4 instrucciones por µs (TCY = 250 ns):

| Función | Estimado | Por qué |
|---|---|---|
| ISR del ADC (I y Q) | ≈ 10–12 µs cada una | despacho en `interrupt_manager.c` + lectura de `ADRESH:ADRESL`, guardar, cambiar `ADPCH`, `ADGO` |
| `adc_iq_get()` y contar el par | ≈ 5 µs | copia de 2 variables de 16 bits con `ADIE` apagado |
| `a_decimal()` | ≈ 95 µs por valor | restas sucesivas de 1000, 100, 10 y 1 (hasta 18 restas de 16 bits) |
| Texto fijo (`,` y `\r\n`) | ≈ 5 µs | 3 asignaciones a un arreglo |
| `tx_string()` de 11 bytes | ≈ 80 µs | largo de la cadena + copia al buffer circular byte a byte |
| ISR de TX | ≈ 6 µs | una por byte |

Total del armado de una línea: **≈ 280 µs**. Estos valores son una **previsión** y así deben ir en el informe hasta medirlos.

## 3. Supuestos de las figuras

- Caso A de adquisición (10 µs).
- Datos de ejemplo: I = 2048 (1,85 V), Q = 2051 → línea `2048,2051\r\n` = `32 30 34 38 2C 32 30 35 31 0D 0A`.
- En el tick en que no toca mandar línea, `main` solo actualiza el PWM y cuenta el par.
- Las figuras **no incluyen** `pwm_desde_adc()` (T-06), que corre con cada par antes de `adc_ascii_par()`; su tiempo está sin estimar. Al medirlo, sumarlo en la sección 2 del script.
- La ISR de TX tiene prioridad sobre la del ADC en `interrupt_manager.c`: si coinciden, el arranque de Q se retrasa.
- RC2 con `ADC_PIN_PRUEBA = 2`: saca la salida del Timer0, que cambia de nivel en cada disparo (cuadrada de 500 Hz).

## 4. Cómo se generan las imágenes

El script `matlab/generar_diagramas_tiempo.m` (MATLAB R2020a o más nuevo; también corre en GNU Octave):

1. Define en la sección 1 los datos del chip (Fosc, TAD, `SP1BRG`, Timer0, `ADC_PARES_POR_LINEA`) y calcula Tconv, Ts, baud, bit y byte.
2. En la sección 2 están todos los tiempos de CPU, uno por variable y con su etiqueta (`[ESTIMADO]`).
3. Calcula los instantes de cada evento a partir de esos tiempos y los imprime en la consola (tabla de resumen).
4. Dibuja cada fila con `rectangle` (bloques de actividad) y `stairs` (señales digitales: Timer0, RC2 y el byte 8N1).
5. Guarda las cinco figuras en PNG a 200 dpi en la carpeta `figuras/` junto al script. Las copias de `docs/informe/figuras/` y los `D1` a `D5` de esta carpeta salen de ahí.

## 5. Cómo reemplazar los estimados por mediciones

1. Para medir una función, poner `ADC_PIN_PRUEBA` en 1 o agregar un pin que suba al inicio de la función y baje al final (por ejemplo, alrededor de `adc_ascii_par()`).
2. Medir el ancho del pulso con el osciloscopio. En la placa nueva RC2 es `EN_AZ_PIC`, así que se mide con JP_AZEN en 1–2, en el pin 3 de JP_AZEN.
3. Con `ADC_PIN_PRUEBA` = 1, la separación entre los dos pulsos de RC2 da el caso de adquisición real (≈ 38,5 µs en el caso A, ≈ 68,5 µs si son 40 µs, ≈ 349 µs si son 320 µs). Con 2, el periodo de la onda en RC2 (2 ms) da el periodo de muestreo.
4. En RC4 (TX) se ve cada línea como una ráfaga de 11 bytes de 0,96 ms cada 2 ms.
5. Escribir el valor medido en la sección 2 del script y volver a correrlo.

Alternativa sin osciloscopio: el *Stopwatch* del simulador de MPLAB X cuenta los ciclos exactos de una función.
