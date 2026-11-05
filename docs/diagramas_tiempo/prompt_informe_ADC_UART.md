# Instrucciones para redactar el informe corto de ADC y UART (Micro 3)

## Qué te pido

Eres un ingeniero electrónico que redacta la parte técnica de un pre-informe universitario de Microcontroladores. Escribe en **español**, con coma decimal, un informe corto (4 a 6 páginas) que explique y justifique **todo lo escogido para el ADC y la UART** de este proyecto, con sus cálculos.

Reglas:
1. **Usa solo los datos de este documento.** No inventes valores, registros, páginas del datasheet ni resultados. Si algo falta, escribe «por definir» o «por medir».
2. **Conserva las etiquetas de origen** de cada número: **[DS]** datasheet, **[CÁLCULO]** cuenta exacta, **[ESTIMADO]** previsión por conteo de instrucciones, **[POR MEDIR]**. Nada de esto se ha medido todavía en la placa, salvo lo marcado como «medido». Lo estimado se redacta como «se prevé», nunca como resultado.
3. Muestra cada cálculo con la fórmula, los valores sustituidos y el resultado.
4. En la UART explica el **formato de texto ASCII** que se usa (`xxxx,xxxx\r\n`, 500 líneas/s) con sus números, por qué cabe y por qué se manda 1 línea cada 2 pares, y sus ventajas y límites.
5. Estructura sugerida:
   1. Descripción breve del sistema
   2. UART: configuración y cálculos
   3. Formato de los datos: líneas de texto ASCII (con tabla de ocupación)
   4. ADC: configuración y cálculos
   5. Hardware de entrada del ADC: resistencia de manejo del S&H y salida del operacional
   6. Diagramas de tiempo (dejar espacio para las Figuras 1 a 5 con su pie)
   7. Mediciones pendientes y cómo se harán
   8. Referencias
6. Tono técnico y directo, frases cortas, tablas donde ayuden. Nada de relleno ni frases de cierre genéricas.

---

## A. Sistema

- Instrumento de corrientes de Foucault. PCB1 excita la sonda, PCB2 demodula y entrega las componentes I y Q en banda base (filtros de 20 Hz, fn ≈ 19,4 Hz), restadas y amplificadas. La placa Micro3 tiene el PIC, que digitaliza I y Q y las manda al PC (LabVIEW o Python) por UART, como texto.
- Microcontrolador: **PIC16F18426** (14 pines, 8 bits). Oscilador interno **Fosc = 16 MHz** (HFINTOSC, ±2 %), TCY = 4/Fosc = 250 ns. Alimentación **VDD = 3,7 V**.
- Firmware en C con XC8 2.40 y MPLAB X, sin esperas bloqueantes (prohibido `while(!TXIF)` y `__delay_ms`).
- El ADC muestrea I y Q a **1 kHz por canal** (Timer0, 1000 pares/s). Con **cada par** se actualiza un PWM por canal (PWM6 en RA2 = I, PWM7 en RA4 = Q, Timer2 a 15,625 kHz, 10 bits, `duty = (código + 2) >> 2`); la UART manda 1 de cada 2 pares (500 líneas/s). Detalle del PWM: `docs/especificacion_pwm.md` (fuera del alcance de este informe corto, salvo que se pida).
- **En esta entrega no hay filtro anti-aliasing** (indicación del equipo).
- Fuente accesible en la placa: la alimentación entra por **J1** (VDD_D y GND) y está en el punto de prueba **TP3** (VAA, que es VDD_D detrás de un filtro de 10 Ω con 2,2 µF) y en el pin 2 de J4 (conector del programador).

## B. UART: valores y cálculos

| Parámetro | Valor | Origen |
|---|---|---|
| Módulo | EUSART1, asíncrono | [DS] |
| Pines | TX = RC4 (pin 6), RX = RC5 (pin 5), por PPS. Hoy el PIC no lee el RX | código |
| Formato de byte | **8N1**: 1 inicio + 8 datos (LSB primero) + 1 parada = 10 bits | código |
| Configuración del baud | `BRGH` = 1, `BRG16` = 1 → baud = Fosc / (4·(SPBRG + 1)) | [DS] |
| `SP1BRG` | 34 | código |
| Baud real | 16 MHz / (4·35) = **114 286 baud** (pedido 115200; error **−0,79 %**) | [CÁLCULO] |
| Tiempo de bit | 1/114 286 = **8,75 µs** (8,68 µs a 115200 exactos) | [CÁLCULO] |
| Byte 8N1 | 10 × 8,75 = **87,5 µs** (86,8 µs nominal) | [CÁLCULO] |
| Capacidad de la línea | 114 286 / 10 = **11 429 bytes/s** (11 520 a 115200 exactos) | [CÁLCULO] |
| Entrada RX | nivel **Schmitt** (`INLVLC = 0x3F` en `pin_manager.c`), VIH = 0,8·VDD = 2,96 V. El diseño pedía TTL (`INLVLC5 = 0`, 1,73 V), sin aplicar | código / [DS] |
| FIFO de recepción del chip | 2 bytes → la ISR debe leer antes de ≈ 2 bytes = **≈ 174 µs** o hay overrun | [DS] / [CÁLCULO] |
| Buffers en RAM | circulares: **RX 32 bytes, TX 64 bytes** (potencias de 2: el índice se envuelve con `& (N−1)`, sin división) | código |
| Interrupciones | RX siempre activa; TX activa solo mientras hay datos; un solo vector | código |
| Funciones pedidas en clase | `bool tx_DAT(const char *DATA, uint16_t size)` y `bool tx_string(const char *data)`: encolan todo o nada y regresan enseguida; `false` si no cabe | código |
| Capa física | módulo USB-serie **CP2102** a 3,3 V. En la placa Micro3: dos traductores de nivel **SN74LVC1T45** (lado PIC a 3,7 V, lado USB a 3,3 V) y jumpers JP_TX/JP_RX. Diseño anterior: divisor 1 kΩ + 2,2 kΩ en el TX (3,7 × 2,2/3,2 = 2,54 V) | esquemático |

**Por qué `BRG16` = 1:** con la fórmula de clase (`BRGH` = 1, `BRG16` = 0), SPBRG = Fosc/(16·baud) − 1 = 16 MHz/(16·115200) − 1 = 7,68 → 8, que da 16 MHz/(16·9) = 111 111 baud (**−3,55 %**). Con `BRG16` = 1 el divisor es 4·(n + 1) y el error baja a −0,79 %. [CÁLCULO]

**Tolerancia de baud:** el receptor muestrea cada bit a la mitad. El bit de parada está a 9,5 bits del flanco de inicio y el desfase acumulado debe ser menor que medio bit: 0,5/9,5 = **±5,3 %** teórico entre los dos extremos; en la práctica se reserva la mitad (±2 a 3 %). Peor caso: −0,79 % del baud más ±2 % del oscilador interno ≈ **−2,8 %**: funciona, con poco margen. [CÁLCULO]

**Por qué 115200 y no 9600/19200** (con 500 líneas/s de 11 bytes = 5500 B/s):

| Baud | µs por byte | Bytes/s | Ocupación con 5500 B/s |
|---|---|---|---|
| 9600 | 1041,7 | 960 | 573 %: no cabe |
| 19200 | 520,8 | 1920 | 286 %: no cabe |
| 57600 | 173,6 | 5760 | 95 %: sin margen |
| **115200** | **86,8** | **11 520** | **48 %** |

Resultado medido el 2026-10-19 (eco a 115200): 20/20, 256/256 y 1000/1000 bytes sin errores.

## C. Formato de los datos: líneas de texto ASCII

Decisión del equipo (2026-11-04): el PIC manda **texto**, una línea cada 2 pares, sin que el PC le pida nada. Primero va I y después Q, separados por una coma y sin nombres:

```
2048,2051\r\n
```

| Campo | Bytes |
|---|---|
| I en decimal, 0 a 4095, sin ceros a la izquierda | 1 a 4 |
| `,` | 1 |
| Q en decimal | 1 a 4 |
| `\r\n` (0x0D 0x0A) | 2 |

Ejemplo: I = 2048, Q = 2051 → `32 30 34 38 2C 32 30 35 31 0D 0A` (11 bytes).

Números [CÁLCULO]:
- Línea de 11 bytes = 11 × 87,5 µs = **962,5 µs**: casi todo el periodo de muestreo (1 ms).
- Por eso se manda **una línea cada 2 pares** (`ADC_PARES_POR_LINEA` = 2): **500 líneas/s** → 5500 B/s = **48,1 %** de la línea. Una por par serían 11 000 B/s = 96 %: cabe apenas, sin margen.
- fs en el PC = 500 Hz. El ADC sigue muestreando a 1 kHz con el Timer0: el PIC descarta 1 de cada 2 pares (sin promediar). Nyquist en el PC: 250 Hz, por encima de las señales de 5 y 20 Hz y sus armónicos 2 a 5 (hasta 100 Hz).

Conversión a decimal en el PIC (`a_decimal()`): sin dividir, restando 1000, 100, 10 y 1 tantas veces como quepa (hasta 18 restas de 16 bits para 4095), y cada cifra se pasa a texto con `'0' + cifra` (0x30 + n): **≈ 95 µs por valor**, ≈ 190 µs para I y Q; más el texto fijo (≈ 5 µs) y `tx_string` (≈ 80 µs): **≈ 280 µs por línea** [ESTIMADO].

Ventajas: legible directamente en una terminal y en el Serial Plotter del Arduino IDE (números separados por coma); en LabVIEW basta *VISA Read* con el carácter de fin `\n` y *Scan From String* con `%d,%d`; el fin de línea sincroniza solo; sin problemas de orden de bytes; la línea es corta (11 bytes) porque no lleva nombres; el PIC empieza a mandar al encenderse. Es lo que sugiere el enunciado.

Límites: el PC depende del orden (primer número = I, segundo = Q); más bytes por muestra que un formato binario, así que llega 1 de cada 2 pares (500 Hz en el PC); largo variable; la conversión a decimal cuesta ≈ 280 µs de CPU por línea; sin verificación de integridad ni número de secuencia (una línea dañada solo se nota si pierde la forma `número,número`; una perdida no se ve: se revisa el contador `adcLineasPerdidas` en el PIC y que el PC reciba 500 líneas/s); el PC no le manda comandos al PIC.

## D. ADC: valores y cálculos

| Parámetro | Valor | Origen |
|---|---|---|
| Módulo | ADCC de 12 bits, SAR, modo básico (`ADCON2` = 0), resultado justificado a la derecha | [DS] / código |
| Referencia | `ADREF` = 0x00: Vref+ = **VDD = 3,7 V**, Vref− = VSS | código |
| LSB | 3,7 V / 4096 = **0,9033 mV** | [CÁLCULO] |
| Código ideal | N = Vin · 4096 / VDD; **1,85 V → 2048** (en la línea: `2048,…`) | [CÁLCULO] |
| Canales | I = RC0 (ANC0, `ADPCH` = 0x10), Q = RC1 (ANC1, 0x11); un solo ADC multiplexado, I y luego Q en ráfaga | código |
| Reloj del ADC | `ADCLK` = Fosc/32 → **TAD = 2 µs** (rango permitido 0,5 a 9 µs, AD20) | [DS] |
| Conversión | 14·TAD + 2·TCY = 28 + 0,5 = **28,5 µs** por canal (AD22) | [DS] |
| Adquisición | `ADACQ` = 160: **10 µs** si cuenta ciclos de Fosc, **40 µs** si cuenta TCY, **320 µs** si cuenta TAD (el datasheet se contradice) | [POR MEDIR], prueba 4.16 |
| Disparo | `ADACT` = 0x02: coincidencia del **Timer0** arranca la conversión de I por hardware; Q la arranca la ISR (`ADGO` = 1) | código |
| Periodo de muestreo | Timer0 de 8 bits, Fosc/4 = 4 MHz, prescaler 1:16 → 4 µs/cuenta; `TMR0H` = 249 → 250 × 4 µs = **1000 µs → fs = 1 kHz** (pedido por el profesor) | [CÁLCULO] |
| Promedio | **ninguno**: cada línea lleva una sola muestra de I y una de Q (decisión del 2026-11-03) | código |
| Interrupción | `ADIE` = 1; la ISR guarda el resultado, cambia de canal y deja el par listo para el programa principal | código |

**Por qué Vref = VDD:** cubre 0 a 3,7 V (los rieles de PCB2) sin recorte, no gasta pines (RA1/ADCVREF+ es ICSPCLK del programador), y como el nivel de reposo Vref2 = 1,85 V sale de un divisor del mismo riel, el código de reposo es **ratiométrico**: si VDD cambia, Vref2 y la referencia cambian juntos y el código sigue en 2048. Descartados: FVR de 4,096 V (no puede superar VDD), FVR de 2,048 V (recorta todo lo que pase de 2,048 V; el rango lineal 1,85 ± 0,185 V llega a 2,035 V, a solo 13 mV).

**Nyquist y anti-alias:** la señal útil está por debajo de ≈ 20 Hz; fs = 1 kHz da un sobremuestreo de 1000/(2·20) = 25 y Nyquist en 500 Hz (en el PC, con 500 líneas/s, Nyquist en 250 Hz). **En esta entrega no hay filtro anti-aliasing**: el criterio fc ≤ fs/2 del enunciado queda **por definir** (prueba 4.7). Lo que llegue al pin por encima de 500 Hz se pliega en el ADC, y por encima de 250 Hz en los datos del PC. El filtro de 19,4 Hz de PCB2 es parte de la demodulación (Práctica 2), no de esta entrega; las pruebas del ADC se hacen con el generador directo al pin. *(El Sallen-Key de ≈ 5 Hz de la placa Micro3 queda puenteado con JP_ADC_I/Q en 2–3 y no se usa en esta entrega.)*

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

**Red en el pin del ADC (placa Micro3):** salida del operacional → **R = 1 kΩ** → pin, con **C = 10 nF** a GND (C22 y C32). No es un filtro anti-aliasing: su polo (15,9 kHz) está muy por encima de fs/2; sirve de depósito de carga para el S&H.
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
| **Conversión a decimal** (`a_decimal` de I y de Q) | ≈ 95 + 95 µs | [ESTIMADO] |
| **Armado de la línea**: texto fijo + `tx_string` | 5 + 80 µs | [ESTIMADO] |
| Total de «par listo» a «línea encolada» | ≈ 280 µs (solo cada 2 pares) | [ESTIMADO] |
| Transmisión de la línea | ≈ 380 → 1345 µs (962,5 µs) | [CÁLCULO] |
| ISR de TX | ≈ 6 µs, una por byte | [ESTIMADO] |
| Periodo de las líneas | 2000 µs (48,1 % ocupado) | [CÁLCULO] |

Observación para el texto: la línea termina pasado el siguiente disparo (1000 µs). Las ISR de TX de ese tramo pueden coincidir con la ISR de I del periodo siguiente y retrasar el arranque de Q unos 6 µs: es la principal fuente de jitter de Q.

Pies de figura sugeridos:
- Figura 1. Dos periodos de muestreo a 1 kHz y una línea ASCII cada 2 pares.
- Figura 2. Adquisición, conversión e interrupciones de I y Q.
- Figura 3. Conversión a decimal y armado de la línea.
- Figura 4. Byte '2' (0x32) en 8N1 y línea de 11 bytes.
- Figura 5. Una línea por par ocupa el 96 %; una cada 2 pares, el 48 %.

## G. Mediciones pendientes (cómo se harán)

| Medición | Método |
|---|---|
| Tiempos de CPU (ISR, conversión ASCII, armado de la línea) | pin que sube al entrar y baja al salir de cada función + osciloscopio; o *Stopwatch* del simulador de MPLAB X |
| Unidad real de `ADACQ` | con `ADC_PIN_PRUEBA` = 1, separación entre los dos pulsos del marcador de la ISR en RC2: ≈ 38,5 µs (10 µs), ≈ 68,5 µs (40 µs) o ≈ 349 µs (320 µs) |
| Histograma del periodo (10 000 muestras en LabVIEW) y su σ | el PIC mide su periodo con Timer1 a 16 MHz (62,5 ns/cuenta; ideal 16 000 cuentas) y lo manda en lugar de Q (`Q = periodo − 16000 + 2048`); LabVIEW junta 10 000 valores y usa Histogram.vi y Std Deviation and Variance.vi. Marcar la hora de llegada en LabVIEW no sirve: mide la latencia del USB (milisegundos). Para resolver nanosegundos: osciloscopio o contador sobre la salida del Timer0, que ya sale por RC2 con `ADC_PIN_PRUEBA` = 2 |
| Histograma de códigos (ruido) | entrada fija (0 V y 1,85 V), 10 000 líneas, σ en LSB y µV; incertidumbre de σ = 1/√(2·(M − 1)) = 0,71 % con M = 10 000 |
| SINAD | generador directo al pin (≤ 10 kΩ, sin PCB2), senoidal centrada en 1,85 V de 1,7 V de amplitud, a 5 Hz y 20 Hz, 4096 pares y FFT con ventana de Kaiser; criterio ≥ 60 dB |

## H. Referencias que puedes citar

- Microchip, *PIC16(L)F18426/46 Data Sheet*, DS40001985 (ADCC cap. 32; EUSART; tabla de parámetros AD20, AD22, AD08).
- Microchip, *PIC16(L)F18426/46 Silicon Errata*, DS80000799.
- Microchip, *MCP601/1R/2/3/4 Data Sheet*, DS21314G.
- Microchip, *MPLAB XC8 C Compiler User's Guide for PIC*, DS50002737.
- Texas Instruments, *SN74LVC1T45 Single-Bit Dual-Supply Bus Transceiver* (datasheet).
- Silicon Labs, *CP2102 Single-Chip USB to UART Bridge* (datasheet).
