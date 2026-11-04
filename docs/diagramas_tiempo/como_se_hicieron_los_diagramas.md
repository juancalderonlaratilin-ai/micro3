# Cómo se obtuvieron los diagramas de tiempo

Proyecto Micro 3 · PIC16F18426 · Firmware con el ADC a 1 kHz sin promedio y tramas binarias de 9 bytes a 115200 baud.

## 1. Qué muestran

| Figura | Contenido |
|---|---|
| 1 | Un periodo de muestreo completo (1 ms): Timer0, ADC, interrupciones, programa principal, línea UART y pin marcador |
| 2 | Detalle de la lectura del ADC: adquisición, conversión, latencia, ISR de I y de Q, cambio de canal |
| 3 | Conversión de los datos y armado de la trama `DATA_IQ` |
| 4 | Un byte 8N1 en la línea (0xA5) y la trama completa de 9 bytes |
| 5 | Línea ASCII del modo de prueba frente a la trama binaria |

## 2. De dónde sale cada tiempo

Los tiempos se separan en tres clases. Cada valor de las figuras pertenece a una de ellas.

**a) Datasheet (DS40001985 del PIC16F18426)**
- Conversión de 12 bits: `Tconv = 14·TAD + 2·TCY` (parámetro AD22). Con `ADCLK` = Fosc/32, TAD = 32/16 MHz = 2 µs y TCY = 4/16 MHz = 250 ns → **28,5 µs**.
- Latencia de interrupción: 3 a 4 TCY = 0,75 a 1 µs.

**b) Calculados con los registros del código**
- Periodo de muestreo (Timer0): reloj Fosc/4 = 4 MHz, prescaler 1:16 → 4 µs por cuenta; `TMR0H` = 249 → 250 cuentas → **1000 µs**.
- Baud rate: `BRGH` = 1, `BRG16` = 1, `SP1BRG` = 34 → baud = Fosc / (4·(34 + 1)) = **114 286** (error −0,79 % frente a 115200).
- Bit = 1 / 114 286 = 8,75 µs; byte 8N1 = 10 bits = 87,5 µs; trama de 9 bytes = **787,5 µs**; línea ASCII de 15 bytes = 1312,5 µs.
- Adquisición: `ADACQ` = 160. No está claro en qué unidad cuenta el chip: 10 µs si cuenta ciclos de Fosc (lo que guarda MCC), 40 µs si cuenta TCY y 320 µs si cuenta TAD. Las figuras usan **10 µs (caso A)** hasta medirlo con la prueba 4.16.

**c) Estimados (CPU)**
No había un listado del compilador con el código actual, así que la duración de cada función se estimó contando las instrucciones que XC8 genera para ese código en C, a 4 instrucciones por µs (TCY = 250 ns):

| Función | Estimado | Por qué |
|---|---|---|
| ISR del ADC (I y Q) | ≈ 10–12 µs cada una | despacho en `interrupt_manager.c` + lectura de `ADRESH:ADRESL`, guardar, cambiar `ADPCH`, `ADGO` |
| `adc_iq_get()` | ≈ 5 µs | copia de 2 variables de 16 bits con `ADIE` apagado |
| Separar I y Q en bytes | ≈ 2,5 µs | 4 desplazamientos y máscaras |
| Cabecera y payload | ≈ 7,5 µs | 8 asignaciones a un arreglo |
| `crc8()` sobre 6 bytes | ≈ 150 µs (100 a 200) | 48 vueltas bit a bit; depende de los datos |
| `tx_DAT()` de 9 bytes | ≈ 50 µs | copia al buffer circular byte a byte |
| ISR de TX | ≈ 6 µs | una por byte |
| `a_decimal()` (ASCII) | ≈ 95 µs por valor | restas sucesivas de 1000, 100, 10 y 1 (hasta 18 restas de 16 bits) |

Estos valores son una **previsión** y así deben ir en el informe hasta medirlos.

## 3. Supuestos de las figuras

- Caso A de adquisición (10 µs).
- Datos de ejemplo: I = 2048 (1,85 V), Q = 2051, SEQ = 5 → trama `A5 5A A1 05 08 00 08 03 02`. El CRC-8 (polinomio 0x07, valor inicial 0x00, sobre los bytes 2 a 7) se comprobó con un script: da 0x02.
- La ISR de TX tiene prioridad sobre la del ADC en `interrupt_manager.c`: si coinciden, el arranque de Q se retrasa.
- El pin marcador (RC2, `ADC_PIN_PRUEBA = 1`) sube durante cada ISR del ADC.

## 4. Cómo se generan las imágenes

El script `generar_diagramas_tiempo.m` (MATLAB R2020a o más nuevo; también corre en GNU Octave):

1. Define en la sección 1 los datos del chip (Fosc, TAD, `SP1BRG`, Timer0) y calcula Tconv, Ts, baud, bit y byte.
2. En la sección 2 están todos los tiempos de CPU, uno por variable y con su etiqueta (`[ESTIMADO]`).
3. Calcula los instantes de cada evento a partir de esos tiempos y los imprime en la consola (tabla de resumen).
4. Dibuja cada fila con `rectangle` (bloques de actividad) y `stairs` (señales digitales: Timer0, RC2 y el byte 8N1).
5. Guarda las cinco figuras en PNG a 200 dpi en la carpeta `figuras/`.

## 5. Cómo reemplazar los estimados por mediciones

1. Subir un pin al inicio de la función que se quiere medir y bajarlo al final (por ejemplo, alrededor de `protocol_send_iq()` o de `crc8()`).
2. Medir el ancho del pulso con el osciloscopio. Para las ISR del ADC sirve el marcador que ya existe: en la placa nueva RC2 es `EN_AZ_PIC`, así que se mide con JP_AZEN en 1–2, en el pin 3 de JP_AZEN.
3. La separación entre los dos pulsos de RC2 da el caso de adquisición real (≈ 38,5 µs en el caso A, ≈ 68,5 µs si son 40 µs, ≈ 349 µs si son 320 µs).
4. Escribir el valor medido en la sección 2 del script y volver a correrlo.

Alternativa sin osciloscopio: el *Stopwatch* del simulador de MPLAB X cuenta los ciclos exactos de una función.
