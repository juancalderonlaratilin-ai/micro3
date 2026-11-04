# Instrucciones para redactar el informe corto de ADC y UART (Micro 3)

## Qué te pido

Eres un ingeniero electrónico que redacta la parte técnica de un pre-informe universitario de Microcontroladores. Escribe en **español**, con coma decimal, un informe corto (4 a 6 páginas) que explique y justifique **todo lo escogido para el ADC y la UART** de este proyecto, con sus cálculos.

Reglas:
1. **Usa solo los datos de este documento.** No inventes valores, registros, páginas del datasheet ni resultados. Si algo falta, escribe «por definir» o «por medir».
2. **Conserva las etiquetas de origen** de cada número: **[DS]** datasheet, **[CÁLCULO]** cuenta exacta, **[ESTIMADO]** previsión por conteo de instrucciones, **[POR MEDIR]**. Nada de esto se ha medido todavía en la placa, salvo lo marcado como «medido». Lo estimado se redacta como «se prevé», nunca como resultado.
3. Muestra cada cálculo con la fórmula, los valores sustituidos y el resultado.
4. En la UART **presenta las dos opciones de formato** (ASCII y binario de 9 bytes) con ventajas, desventajas y números, y explica por qué se eligió el binario.
5. Estructura sugerida:
   1. Descripción breve del sistema
   2. UART: configuración y cálculos
   3. Formato de los datos: ASCII frente a binario (con tabla comparativa)
   4. ADC: configuración y cálculos
   5. Hardware de entrada del ADC: resistencia de manejo del S&H y salida del operacional
   6. Diagramas de tiempo (dejar espacio para las Figuras 1 a 5 con su pie)
   7. Mediciones pendientes y cómo se harán
   8. Referencias
6. Tono técnico y directo, frases cortas, tablas donde ayuden. Nada de relleno ni frases de cierre genéricas.

---

## A. Sistema

- Instrumento de corrientes de Foucault. PCB1 excita la sonda, PCB2 demodula y entrega las componentes I y Q en banda base (filtros de 20 Hz, fn ≈ 19,4 Hz), restadas y amplificadas. La placa Micro3 tiene el PIC, que digitaliza I y Q y las manda al PC (LabVIEW) por UART.
- Microcontrolador: **PIC16F18426** (14 pines, 8 bits). Oscilador interno **Fosc = 16 MHz** (HFINTOSC, ±2 %), TCY = 4/Fosc = 250 ns. Alimentación **VDD = 3,7 V**.
- Firmware en C con XC8 2.40 y MPLAB X, sin esperas bloqueantes (prohibido `while(!TXIF)` y `__delay_ms`).
- Fuente accesible en la placa: la alimentación entra por **J1** (VDD_D y GND) y está en el punto de prueba **TP3** (VAA, que es VDD_D detrás de un filtro de 10 Ω con 2,2 µF) y en el pin 2 de J4 (conector del programador).

## B. UART: valores y cálculos

| Parámetro | Valor | Origen |
|---|---|---|
| Módulo | EUSART1, asíncrono | [DS] |
| Pines | TX = RC4 (pin 6), RX = RC5 (pin 5), por PPS | código |
| Formato de byte | **8N1**: 1 inicio + 8 datos (LSB primero) + 1 parada = 10 bits | código |
| Configuración del baud | `BRGH` = 1, `BRG16` = 1 → baud = Fosc / (4·(SPBRG + 1)) | [DS] |
| `SP1BRG` | 34 | código |
| Baud real | 16 MHz / (4·35) = **114 286 baud** (pedido 115200; error **−0,79 %**) | [CÁLCULO] |
| Tiempo de bit | 1/114 286 = **8,75 µs** (8,68 µs a 115200 exactos) | [CÁLCULO] |
| Byte 8N1 | 10 × 8,75 = **87,5 µs** (86,8 µs nominal) | [CÁLCULO] |
| Capacidad de la línea | 115200 / 10 = **11 520 bytes/s** | [CÁLCULO] |
| Entrada RX | nivel TTL (`INLVLC5 = 0`), VIH = 1,73 V | código / [DS] |
| FIFO de recepción del chip | 2 bytes → la ISR debe leer antes de ≈ 2 bytes = **≈ 174 µs** o hay overrun | [DS] / [CÁLCULO] |
| Buffers en RAM | circulares: **RX 32 bytes, TX 64 bytes** (potencias de 2: el índice se envuelve con `& (N−1)`, sin división) | código |
| Interrupciones | RX siempre activa; TX activa solo mientras hay datos; un solo vector | código |
| Funciones pedidas en clase | `bool tx_DAT(const char *DATA, uint16_t size)` y `bool tx_string(const char *data)`: encolan todo o nada y regresan enseguida; `false` si no cabe | código |
| Capa física | módulo USB-serie **CP2102** a 3,3 V. En la placa Micro3: dos traductores de nivel **SN74LVC1T45** (lado PIC a 3,7 V, lado USB a 3,3 V) y jumpers JP_TX/JP_RX. Diseño anterior: divisor 1 kΩ + 2,2 kΩ en el TX (3,7 × 2,2/3,2 = 2,54 V) | esquemático |

**Por qué `BRG16` = 1:** con la fórmula de clase (`BRGH` = 1, `BRG16` = 0), SPBRG = Fosc/(16·baud) − 1 = 16 MHz/(16·115200) − 1 = 7,68 → 8, que da 16 MHz/(16·9) = 111 111 baud (**−3,55 %**). Con `BRG16` = 1 el divisor es 4·(n + 1) y el error baja a −0,79 %. [CÁLCULO]

**Tolerancia de baud:** el receptor muestrea cada bit a la mitad. El bit de parada está a 9,5 bits del flanco de inicio y el desfase acumulado debe ser menor que medio bit: 0,5/9,5 = **±5,3 %** teórico entre los dos extremos; en la práctica se reserva la mitad (±2 a 3 %). Peor caso: −0,79 % del baud más ±2 % del oscilador interno ≈ **−2,8 %**: funciona, con poco margen. [CÁLCULO]

**Por qué 115200 y no 9600/19200** (con 1000 muestras/s de 9 bytes = 9000 B/s):

| Baud | µs por byte | Bytes/s | Ocupación con 9000 B/s |
|---|---|---|---|
| 9600 | 1041,7 | 960 | 938 %: no cabe |
| 19200 | 520,8 | 1920 | 469 %: no cabe |
| 57600 | 173,6 | 5760 | 156 %: no cabe |
| **115200** | **86,8** | **11 520** | **78 %** |

Resultado medido el 2026-10-19 (eco a 115200): 20/20, 256/256 y 1000/1000 bytes sin errores.

## C. Formato de los datos: las dos opciones

### C.1 Binario (el implementado en el protocolo)

Trama fija de **9 bytes**, igual en los dos sentidos, big-endian:

| Byte | Campo | Valor |
|---|---|---|
| 0–1 | sincronía | `A5 5A` |
| 2 | TYPE | `A1` = DATA_IQ (otros: `01` PING, `81` PONG, `7F` ERROR, `20` ADC_STREAM, `A0` ADC_ACK) |
| 3 | SEQ | contador 0–255; sube aunque una trama no quepa, para que el PC detecte pérdidas |
| 4–5 | I | 12 bits en 16, byte alto primero |
| 6–7 | Q | igual |
| 8 | CRC-8 | polinomio 0x07 (x⁸ + x² + x + 1), valor inicial 0x00, sin reflejo, sobre los bytes 2 a 7 |

Ejemplo verificado: I = 2048, Q = 2051, SEQ = 5 → `A5 5A A1 05 08 00 08 03 02` (CRC = 0x02).

Números [CÁLCULO]:
- Duración: 9 × 87,5 µs = **787,5 µs** (781 µs nominal).
- A 1000 tramas/s: 9000 B/s = **78,1 %** de la línea. Máximo teórico: 11 520/9 = 1280 tramas/s.
- Conversión de los datos en el PIC: solo separar en bytes (`I>>8`, `I & 0xFF`): **≈ 2,5 µs** [ESTIMADO].
- CRC-8 bit a bit sobre 6 bytes: **≈ 150 µs** (100 a 200) [ESTIMADO]. Con una tabla de 256 bytes en flash bajaría a unos pocos µs por byte (no implementado).
- El polinomio tiene el factor (x + 1), así que detecta todos los errores de un número impar de bits y todas las ráfagas de hasta 8 bits; un error aleatorio pasa sin detectar con probabilidad ≈ 1/256.

Ventajas: largo fijo (tiempo de transmisión constante y predecible), 2 bytes por valor, verificación de integridad (CRC), detección de pérdidas (SEQ), resincronización con `A5 5A`, conversión casi gratis en el PIC. Lo pidió el profesor: «el ASCII es pesado para el procesador».

Desventajas: no se lee en una terminal ni en el Serial Plotter; el PC necesita un parser (buscar `A5 5A`, tomar 9 bytes, comprobar el CRC, unir bytes alto y bajo); en LabVIEW hay que desactivar el carácter de terminación del puerto, porque un dato puede valer 0x0A; depurar es más difícil.

### C.2 ASCII (sugerido en el enunciado; implementado solo como prueba)

Formatos posibles:

| Formato | Bytes por muestra | Ocupación a 1000/s (115200) |
|---|---|---|
| `I:2048,Q:2051\r\n` (Serial Plotter de Arduino) | 9 a 15 (15 con 4 cifras) | 15 000 B/s = **130 %: no cabe** |
| `2048,2051\n` | 4 a 10 | 10 000 B/s = **86,8 %**: cabe sin margen |
| `$DAT,<I>,<Q>*<chk>\n` (enunciado, con checksum en hex) | hasta 18 | 18 000 B/s = **156 %: no cabe** |

- Línea de 15 bytes = 15 × 87,5 = **1312,5 µs**: dura más que el periodo de muestreo (1 ms). [CÁLCULO]
- Conversión a decimal en el PIC (`a_decimal()`): sin dividir, restando 1000, 100, 10 y 1 tantas veces como quepa (hasta 18 restas de 16 bits para 4095): **≈ 95 µs por valor**, ≈ 190 µs para I y Q, más armar la línea, `strlen` y encolar: **≈ 305 µs por línea** [ESTIMADO]. Para un dígito: `'0' + cifra` (0x30 + n).
- Implementado como prueba: `MODO_PRUEBA_ADC = 1` manda `I:…,Q:…\r\n` **una vez cada 50 muestras** (20 líneas/s, 300 B/s = 2,6 % de la línea) para ver la forma de onda en el Serial Plotter.

Ventajas: legible directamente en una terminal o el Serial Plotter; fácil de leer en LabVIEW (*Scan From String*); el fin de línea `\n` sincroniza solo; sin problemas de orden de bytes.

Desventajas: más bytes por muestra y largo variable (tiempo de transmisión variable); conversión lenta en el PIC (≈ 76 veces más que separar bytes); sin verificación de integridad ni detección de pérdidas, salvo que se agregue checksum y contador (más bytes); a 1000 muestras/s el formato del Serial Plotter no cabe a 115200.

Cómo podría caber el ASCII (no implementado): subir a **250 000 baud** (SPBRG = 15 → 16 MHz/(4·16) = 250 000 exactos, 0 % de error; capacidad 25 000 B/s → el formato de 15 bytes ocuparía el 60 %; [POR VERIFICAR] que el CP2102 acepte esa velocidad), usar el formato corto de 10 bytes, o mandar menos muestras por segundo.

**Decisión:** binario de 9 bytes para los datos; ASCII solo para la prueba visual.

## D. ADC: valores y cálculos

| Parámetro | Valor | Origen |
|---|---|---|
| Módulo | ADCC de 12 bits, SAR, modo básico (`ADCON2` = 0), resultado justificado a la derecha | [DS] / código |
| Referencia | `ADREF` = 0x00: Vref+ = **VDD = 3,7 V**, Vref− = VSS | código |
| LSB | 3,7 V / 4096 = **0,9033 mV** | [CÁLCULO] |
| Código ideal | N = Vin · 4096 / VDD; **1,85 V → 2048** (0x800, `08 00` en la trama) | [CÁLCULO] |
| Canales | I = RC0 (ANC0, `ADPCH` = 0x10), Q = RC1 (ANC1, 0x11); un solo ADC multiplexado, I y luego Q en ráfaga | código |
| Reloj del ADC | `ADCLK` = Fosc/32 → **TAD = 2 µs** (rango permitido 0,5 a 9 µs, AD20) | [DS] |
| Conversión | 14·TAD + 2·TCY = 28 + 0,5 = **28,5 µs** por canal (AD22) | [DS] |
| Adquisición | `ADACQ` = 160: **10 µs** si cuenta ciclos de Fosc, **40 µs** si cuenta TCY, **320 µs** si cuenta TAD (el datasheet se contradice) | [POR MEDIR], prueba 4.16 |
| Disparo | `ADACT` = 0x02: coincidencia del **Timer0** arranca la conversión de I por hardware; Q la arranca la ISR (`ADGO` = 1) | código |
| Periodo de muestreo | Timer0 de 8 bits, Fosc/4 = 4 MHz, prescaler 1:16 → 4 µs/cuenta; `TMR0H` = 249 → 250 × 4 µs = **1000 µs → fs = 1 kHz** (pedido por el profesor) | [CÁLCULO] |
| Promedio | **ninguno**: cada trama lleva una sola muestra de I y una de Q (decisión del 2026-11-03) | código |
| Interrupción | `ADIE` = 1; la ISR guarda el resultado, cambia de canal y deja el par listo para el programa principal | código |

**Por qué Vref = VDD:** cubre 0 a 3,7 V (los rieles de PCB2) sin recorte, no gasta pines (RA1/ADCVREF+ es ICSPCLK del programador), y como el nivel de reposo Vref2 = 1,85 V sale de un divisor del mismo riel, el código de reposo es **ratiométrico**: si VDD cambia, Vref2 y la referencia cambian juntos y el código sigue en 2048. Descartados: FVR de 4,096 V (no puede superar VDD), FVR de 2,048 V (recorta todo lo que pase de 2,048 V; el rango lineal 1,85 ± 0,185 V llega a 2,035 V, a solo 13 mV).

**Nyquist y anti-alias:** la señal útil está por debajo de ≈ 20 Hz; fs = 1 kHz da un sobremuestreo de 1000/(2·20) = 25 y Nyquist en 500 Hz. Se cumple fc ≤ fs/2: el filtro de PCB2 (19,4 Hz) atenúa ≈ −56 dB a 500 Hz. La placa Micro3 agrega un Sallen-Key con ganancia 1,62 y ganancia total ≈ 1; con C = 470 nF corta a **≈ 5 Hz** (−24 dB a 20 Hz). Banda de diseño **por definir**: con 68 nF cortaría a ≈ 35 Hz (−0,3 dB a 20 Hz) y con 100 nF a ≈ 24 Hz. [CÁLCULO nodal]

**Cuantización y SINAD:**
- Ruido de cuantización: LSB/√12 = 0,9033/3,464 = **0,261 mV rms**. [CÁLCULO]
- SINAD ideal de 12 bits con senoidal a escala completa: 6,02·12 + 1,76 = **74,0 dB**. [CÁLCULO]
- Requisito: **SINAD > 60 dB** → ENOB = (60 − 1,76)/6,02 = **9,67 bits**. Senoidal a escala completa: 1,85 V pico = 1,308 V rms → ruido + distorsión permitidos = 1,308 V/10³ = **1,31 mV rms = 1,45 LSB rms**. Margen sobre el ideal: 14 dB. [CÁLCULO]
- Con la señal real de PCB2 (±185 mV, 10 % del rango), un 12 bits ideal da ≈ 54 dB: no alcanza 60 dB. Se adoptó que el requisito es del ADC medido con una senoidal casi a escala completa (pendiente de confirmar con el profesor).

**Sesgo y jitter entre I y Q:**
- La muestra de Q se toma ≈ **46 µs** después de la de I (caso A) [ESTIMADO]: error de fase de 360° · 20 Hz · 46 µs = **0,33° a 20 Hz** (0,08° a 5 Hz). Con 320 µs de adquisición serían 28,5 + 1 + 6,5 + 320 ≈ 356 µs → 2,6° a 20 Hz. [CÁLCULO]
- I lo dispara el hardware (solo el oscilador aporta jitter; el datasheet no lo especifica). Q lo dispara el software: latencia de 3 a 4 TCY (rango de 250 ns) y, si coincide con la ISR de TX, unos **6 µs** más [ESTIMADO].
- SNR limitada por jitter: SNR = −20·log10(2π·f·σt). Con σt = 10 ns: 118 dB a 20 Hz. Para no gastar más de 72 dB a 20 Hz basta σt ≤ 2 µs. [CÁLCULO]

## E. Hardware de entrada: resistencia de manejo del S&H y salida del operacional

**Modelo del S&H [DS §32.3]:** CHOLD = 28 pF, CPIN ≈ 5 pF, RIC = 1 kΩ, RSS ≈ 7 kΩ a 5 V (a 3,7 V se usa 10 kΩ, conservador). Fuente recomendada: **RS ≤ 10 kΩ** (AD08).

**Tiempo de adquisición (Ec. 32-1):**
TACQ = TAMP + TC + TCOFF, con TAMP = 2 µs, TC = CHOLD·(RIC + RSS + RS)·ln(2¹³ − 1) = CHOLD·(RIC + RSS + RS)·9,011 y TCOFF = 0,05 µs/°C·(T − 25 °C).

| RS | TACQ a 25 °C | TACQ a 50 °C |
|---|---|---|
| 200 Ω | 4,83 µs | 6,08 µs |
| 1 kΩ | 5,03 µs | 6,28 µs |
| 10 kΩ | 7,30 µs | 8,55 µs |

[CÁLCULO; con RSS = 7 kΩ, RS = 10 kΩ y 50 °C la fórmula reproduce los 7,79 µs del ejemplo del datasheet.]

**Operacional:** el acondicionamiento usa **MCP604** (Microchip, GBW 2,8 MHz, margen de fase 50°, asentamiento 4,5 µs al 0,01 % con ganancia 1). Su datasheet **no da la impedancia de salida**: solo dice que es una «fuente de baja impedancia». En lazo cerrado Zout ≈ Ro/(1 + A(f)·β), muy baja en continua y mayor a las frecuencias del golpe de carga del S&H. Valor [POR MEDIR] o simulable con el modelo SPICE. Con cargas de más de 40 pF el fabricante recomienda una resistencia en serie (RISO).

**Red en el pin del ADC (placa Micro3):** salida del operacional → **R = 1 kΩ** → pin, con **C = 10 nF** a GND (C22 y C32).
- La resistencia aísla al operacional de la capacidad del pin (cumple la recomendación de RISO) y queda muy por debajo de los 10 kΩ máximos.
- El capacitor es un depósito: al cerrarse el S&H, la carga de CHOLD se reparte y el pin salta solo CHOLD/(C + CHOLD) = 28 pF/10,03 nF = **0,28 %** de la diferencia con el canal anterior; luego se recupera por la resistencia con τ = 1 kΩ·10 nF = **10 µs**. Polo de la red: 1/(2π·1 kΩ·10 nF) = **15,9 kHz**.
- Diafonía de I en Q que deja el CHOLD al cambiar de canal: 0,0028·e^(−TACQ/10 µs) → **−60 dB con 10 µs**, −86 dB con 40 µs. Por eso no conviene bajar la adquisición de unos 40 µs con esta red. [CÁLCULO]

## F. Tiempos para los diagramas (Figuras 1 a 5)

Caso A (adquisición de 10 µs), I = 2048, Q = 2051:

| Etapa | Tiempo | Origen |
|---|---|---|
| Periodo de muestreo | 1000 µs | [CÁLCULO] |
| Adquisición de I | 0 → 10 µs | caso A |
| Conversión de I | 10 → 38,5 µs (28,5 µs) | [DS] |
| Latencia de interrupción | 0,75 a 1 µs | [DS] |
| **Lectura del ADC**: ISR de I (lee `ADRESH:ADRESL`, cambia a Q, `ADGO` = 1) | ≈ 39,5 → 50 µs | [ESTIMADO] |
| Adquisición + conversión de Q | ≈ 46 → 84,5 µs | [DS] + [ESTIMADO] |
| ISR de Q (guarda Q, vuelve a I, avisa a main) | ≈ 85,5 → 97 µs | [ESTIMADO] |
| `adc_iq_get()` | ≈ 5 µs | [ESTIMADO] |
| **Conversión de datos** (separar en bytes) | ≈ 2,5 µs | [ESTIMADO] |
| **Armado de la trama**: cabecera + CRC-8 + `tx_DAT` | 7,5 + 150 + 50 µs | [ESTIMADO] |
| Total de «par listo» a «trama encolada» | ≈ 215 µs | [ESTIMADO] |
| Transmisión de la trama | ≈ 316 → 1103 µs (787,5 µs) | [CÁLCULO] |
| ISR de TX | ≈ 6 µs, una por byte | [ESTIMADO] |
| Línea ASCII de 15 bytes (modo prueba) | ≈ 305 µs de CPU + 1312,5 µs en la línea | [ESTIMADO] + [CÁLCULO] |

Observación para el texto: la trama termina pasado el siguiente disparo (1000 µs). La última ISR de TX (≈ 1016 µs) puede coincidir con la ISR de I del periodo siguiente y retrasar el arranque de Q unos 6 µs: es la principal fuente de jitter de Q.

Pies de figura sugeridos:
- Figura 1. Periodo de muestreo completo a 1 kHz.
- Figura 2. Adquisición, conversión e interrupciones de I y Q.
- Figura 3. Conversión de los datos y armado de la trama DATA_IQ.
- Figura 4. Byte 0xA5 en 8N1 y trama de 9 bytes en la línea.
- Figura 5. Línea ASCII frente a trama binaria.

## G. Mediciones pendientes (cómo se harán)

| Medición | Método |
|---|---|
| Tiempos de CPU (ISR, CRC, armado, conversión ASCII) | pin que sube al entrar y baja al salir de cada función + osciloscopio; o *Stopwatch* del simulador de MPLAB X |
| Unidad real de `ADACQ` | separación entre los dos pulsos del marcador de la ISR: ≈ 38,5 µs (10 µs), ≈ 68,5 µs (40 µs) o ≈ 349 µs (320 µs) |
| Histograma del periodo (10 000 muestras en LabVIEW) y su σ | el PIC mide su periodo con Timer1 a 16 MHz (62,5 ns/cuenta; ideal 16 000 cuentas) y lo manda en la trama; LabVIEW junta 10 000 valores y usa Histogram.vi y Std Deviation and Variance.vi. Marcar la hora de llegada en LabVIEW no sirve: mide la latencia del USB (milisegundos). Para resolver nanosegundos: osciloscopio o contador sobre la salida del Timer0 |
| Histograma de códigos (ruido) | entrada fija (0 V y 1,85 V), 10 000 tramas, σ en LSB y µV; incertidumbre de σ = 1/√(2·(M − 1)) = 0,71 % con M = 10 000 |
| SINAD | generador directo al pin (≤ 10 kΩ, sin PCB2), senoidal centrada en 1,85 V de 1,7 V de amplitud, a 5 Hz y 20 Hz, 4096 pares y FFT con ventana de Kaiser; criterio ≥ 60 dB |

## H. Referencias que puedes citar

- Microchip, *PIC16(L)F18426/46 Data Sheet*, DS40001985 (ADCC cap. 32; EUSART; tabla de parámetros AD20, AD22, AD08).
- Microchip, *PIC16(L)F18426/46 Silicon Errata*, DS80000799.
- Microchip, *MCP601/1R/2/3/4 Data Sheet*, DS21314G.
- Microchip, *MPLAB XC8 C Compiler User's Guide for PIC*, DS50002737.
- Texas Instruments, *SN74LVC1T45 Single-Bit Dual-Supply Bus Transceiver* (datasheet).
- Silicon Labs, *CP2102 Single-Chip USB to UART Bridge* (datasheet).
