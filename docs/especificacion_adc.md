# Especificación del ADC: muestreo de I y Q (tarea T-09)

Proyecto Micro 3 · Fase 2 · Fecha: 2026-10-30 · Estado: **código implementado (T-09), sin probar en el chip**. Los requisitos de §1 vienen del profesor y de la persona.

> **Para qué sirve:** reunir en un solo sitio qué pidió el profesor para el ADC, qué hace el código que ya existe, los presupuestos de tiempo, ruido, jitter y SINAD, y las recomendaciones de hardware y firmware. Reemplaza al borrador suelto `ADC_especificaciones_y_recomendaciones.md`.
>
> **Etiquetas:** **[VERIFICADO]** = sale del datasheet (con página), del código o de un documento de `docs/`. **[CÁLCULO]** = cuenta propia comprobada con un script. **[PROPUESTA]** = decisión por aprobar. **[SIN VERIFICAR]** = dudas que se resuelven midiendo.
>
> **Fuentes:** indicaciones del profesor (chat, 2026-10-28); *Fundamentals of Precision ADC Noise Analysis* de TI (SLYY192A, marzo 2025), citado como «TI»; el preinforme de la Práctica 2; el datasheet DS40001985C («DS»); y el código recibido el 2026-10-30 (`adc_iq.c/.h`, `adc_prueba.c/.h`, `main.c`, `protocol.c/.h`, `mcc_generated_files/adcc.*` y `tmr0.*`).

---

## 1. Requisitos

| # | Requisito | Valor | Origen |
|---|---|---|---|
| R1 | Frecuencia de muestreo | `fs = 1 kHz` (`Ts = 1 ms`) | profesor |
| R2 | Tiempo de conversión | I y Q convertidos **dentro de 1 ms** | derivado de R1 |
| R3 | Resolución | 12 bits (`AD01`, DS p.632) | [VERIFICADO] |
| R4 | Referencia | `VREF+ = VDD = 3,7 V`, `VREF− = VSS` (`ADREF = 0x00`) | código; falta confirmar con el profesor |
| R5 | **SINAD del ADC** | **> 60 dB** (equivale a ENOB > 9,67 bits) | persona, 2026-10-30 |
| R6 | Ruido de cuantización ideal | 352 µV (cifra del profesor; ver §5) | profesor |
| R7 | Jitter | importa el del **reloj/disparo de muestreo**, no el del reloj de conversión | profesor |
| R8 | Medición de ruido | histograma de muestras y conteo de códigos | profesor |
| R9 | Canales | I = RC0 (`ANC0`), Q = RC1 (`ANC1`), un solo ADC multiplexado | decisión de la persona |
| R10 | Criterio de la referencia | justificar la `Vref` y el ruido de referencia y fuente | profesor |

## 2. Qué hace el código actual (T-09) [VERIFICADO en el código del zip]

```
Timer0 (cada 1 ms, por hardware) ──► ADGO = 1 ──► convierte I (RC0)
        adc_iq_isr: guarda I, ADPCH = Q, ADGO = 1 ──► convierte Q (RC1)
        adc_iq_isr: guarda Q, ADPCH = I, suma el par ──► cada 5 pares avisa a main
main:   adc_iq_get() ──► promedio de 5 ──► protocol_send_iq() ──► trama DATA_IQ (200/s)
```

| Dato | Valor | Dónde / fuente |
|---|---|---|
| Disparo | `ADACT = 0x02` = *TMR0_overflow* (DS Tabla 32-6, p.538) | `adcc.c` |
| Timer0 | 8 bits, reloj Fosc/4 = 4 MHz, prescaler 1:16 → 250 kHz, `TMR0H = 249` → 250 cuentas × 4 µs = **1000 µs** [CÁLCULO] | `tmr0.c` (`T0CON1 = 0x44`, `T0CON0 = 0x80`) |
| Reloj del ADC | `ADCLK = 0x0F` → Fosc/32 → `TAD = 2 µs` | `adcc.c`; MCC lo confirma |
| Conversión | `14·TAD + 2·TCY = 28,5 µs` (`AD22`, DS p.633) [CÁLCULO] | MCC: 28,5 µs |
| Adquisición | `ADACQ = 160` | **ver §3: 10 µs o 320 µs** |
| Resultado | 12 bits, justificado a la derecha (`ADFM`), modo básico (`ADCON2 = 0`) | `adcc.c` |
| Canales | `CANAL_I = 0x10` (RC0/ANC0), `CANAL_Q = 0x11` (RC1/ANC1) (DS p.528) | `adcc.h` |
| Referencia | `ADREF = 0x00`: `ADPREF = VDD`, `ADNREF = VSS` | `adcc.c` |
| Interrupción | `ADIE = 1`; `interrupt_manager.c` llama a `adc_iq_isr()` si `ADIE` y `ADIF` | cambio C-11 (§11) |
| Promedio | `ADC_N_PROMEDIO = 5` (decisión D-1): 1 kHz de muestreo y 200 promedios/s | `adc_iq.h` |
| Pin de prueba | `ADC_PIN_PRUEBA = 1`: RC2 sube mientras corre `adc_iq_isr()` (D-4) | `adc_iq.h` |
| Contadores | `adcParesPerdidos` y `adcTramasPerdidas` deben quedar en 0 | `adc_iq.c`, `main.c` |

Las divisiones del promedio las hace `main` (`adc_iq_get`), no la ISR. El acumulador es de 16 bits: 16 × 4095 cabe y 17 × 4095 no, por eso `ADC_N_PROMEDIO` va de 1 a 16 (`#error` si no).

**Cuidado con el modo actual de `main.c`:** el `main.c` copiado del zip trae `MODO_PRUEBA_ADC = 1`. Con eso el bucle solo manda texto para el Serial Plotter (§9) y **no atiende el protocolo ni el eco**. Para volver al protocolo hay que poner 0 y recompilar.

## 3. Presupuesto de tiempo (R2: menos de 1 ms)

Por cada canal: `T_canal = T_acq + T_conv`. Con dos canales en ráfaga: `T_total = 2·T_canal` más la latencia de la ISR.

| Caso | `T_acq` | `T_conv` | Por canal | I + Q | Margen sobre 1 ms |
|---|---|---|---|---|---|
| **A.** `ADACQ` cuenta ciclos de Fosc (lo que supone el comentario de `adc_iq.c`, línea 101) | 10 µs | 28,5 µs | 38,5 µs | **77 µs** | 923 µs |
| **B.** `ADACQ` cuenta periodos del reloj del ADC, es decir `TAD` (lo que dice el DS p.529: «*Number of ADC clock periods*») | 160 × 2 µs = 320 µs | 28,5 µs | 348,5 µs | **697 µs** | 303 µs (30 %) |

- **Los dos casos cumplen R2.** El peor caso (B) deja el 30 % de margen.
- **Duda [SIN VERIFICAR]:** la figura 32-2 del DS rotula la adquisición como «1-8191 `TCY`» (DS p.501) y el texto del registro en periodos del reloj del ADC (p.529). El comentario de `adc_iq.c` supone 10 µs; no se pudo ver qué muestra la ventana de MCC. Se resuelve midiendo (prueba 4.16): la separación entre los dos pulsos de RC2 es ≈ 38,5 µs en el caso A y ≈ 349 µs en el B.
- **Si resulta ser el caso B**, la adquisición de 320 µs es excesiva: `AD23` pide `TACQ` típico de 2 µs (DS p.633) y `AD08` recomienda una fuente de ≤ 10 kΩ (DS p.632). Ver el sesgo entre I y Q en §10 y el cambio propuesto CP-7.
- El disparo automático no garantiza los tiempos: *«It is the user's responsibility to ensure that the ADC timing requirements are met»* (DS §32.2.6).

## 4. Jitter (R7)

**Idea del profesor:** el jitter que importa es el del reloj que **define el instante de muestreo**. El del reloj de conversión (`TAD`) actúa *después* de capturar la señal y no cambia la tensión muestreada, siempre que la conversión termine antes del siguiente disparo.

Límite de TI (Cap. 5, Ec. 28): `SNR_jitter = −20·log10(2π·f_in·t_jitter)`.

| `t_jitter` (rms) | SNR por jitter a 20 Hz | a 5 Hz |
|---|---|---|
| 10 µs | 58 dB | 70 dB |
| 5 µs | 64 dB | 76 dB |
| 1 µs | 78 dB | 90 dB |
| 100 ns | 98 dB | 110 dB |

[CÁLCULO] Se muestrea la banda base (≲ 20 Hz), no la portadora de ≈ 9,6 kHz, que PCB2 ya demodula. Para que el jitter solo cueste −70 dB a 20 Hz a escala completa, `t_rms ≤ 2,5 µs`. Para gastar los 60 dB enteros serían 8 µs.

**Qué jitter tiene este firmware** (hallazgo de esta revisión):

| Canal | Quién fija el instante de muestreo | Jitter esperado |
|---|---|---|
| I | el hardware: Timer0 → `ADGO` | solo el del oscilador HFINTOSC (muy bajo) |
| Q | el **software**: `adc_iq_isr` pone `ADGO = 1` | la **variación de la latencia de interrupción**: si una ISR de la UART está corriendo, la del ADC espera a que termine (hay un solo vector y no hay anidamiento) |

Si la latencia varía entre 0 y `T`, el rms es `T/√12`. Con `T = 25 µs` (el objetivo de las pruebas 1.10 y 1.11) salen 7,2 µs rms → **61 dB** a 20 Hz y escala completa: se come todo el presupuesto de SINAD. Con `T = 8,7 µs` el costo baja a −70 dB. No se conoce la duración real de las ISR de la UART (pruebas 1.10 y 1.11 sin hacer), así que **se mide** con la prueba 4.17. Si no cumple, las ISR de la UART se acortan o el disparo de Q pasa a hardware (decisión del planificador).

El promedio de 5 muestras reduce la parte aleatoria del jitter de Q. La del reloj de conversión no cuenta.

## 5. Cuantización y resolución (R3, R6)

`q = FSR / 2^N` (TI Ec. 2, p.7) y `σq = q / √12 = 0,289·q`, el valor rms del error de cuantización uniforme entre ±½ LSB [teoría estándar, no es una ecuación de TI; es la que usa el profesor]. Con `N = 12`:

| `FSR` | `q` | `σq` | Comentario |
|---|---|---|---|
| 5,0 V | 1,221 mV | **352 µV** | es la cifra del profesor |
| **3,7 V** | **0,903 mV** | **261 µV** | **nuestra referencia (`VDD`)** |
| 2,5 V | 0,610 mV | 176 µV | no existe como FVR en este chip |
| 2,048 V | 0,500 mV | 144 µV | FVR de 2,048 V (DS p.491, cap. 30) |

- [CÁLCULO] **Los 352 µV corresponden a 12 bits con FSR = 5 V.** Con `VREF = 3,7 V` saldrían 261 µV, y para obtener 352 µV con 3,7 V harían falta 11,57 bits, que no es un ADC real. Hay que confirmar con el profesor con qué ADC y `Vref` hizo su cuenta (§13).
- Un LSB de 0,903 mV equivale a **90 µV referidos a la entrada de la etapa de banda base** (÷ `G = 10`).
- El rango lineal de banda base de PCB2 es ±185 mV (preinforme, ec. 8): 370 mV = **410 códigos (≈ 8,7 bits útiles)**. Las variaciones controladas del preinforme (79,2 mV y 46,5 mV) son ≈ 88 y ≈ 51 códigos. El rizado de 40,4 mV pico a pico de una de sus pruebas son ≈ 45 códigos; el preinforme advierte que no se separó el transitorio del rizado estacionario.
- TI (págs. 8–10): bajo 16 bits el ruido del ADC está dominado por la cuantización (el ruido de la hoja de datos coincide con el tamaño del LSB). El ruido analógico previo y el de la referencia son los que hay que vigilar.
- TI (pág. 16): recomienda definir el requisito con el **ruido referido a la entrada** (absoluto) y no con bits efectivos (relativos). **Falta fijar con el profesor la variación mínima de `V_I,OUT` y `V_Q,OUT` que se quiere distinguir.**

## 6. SINAD > 60 dB (R5, requisito nuevo)

**Definición:** `SINAD = 20·log10(V_señal,rms / V_(ruido+distorsión),rms)`, medido sobre los códigos del ADC con una senoidal de entrada (TI Tabla 2, p.13); «ruido + distorsión» es todo lo que no es la fundamental ni DC. `ENOB = (SINAD − 1,76) / 6,02` (TI Tabla 2, p.14) [VERIFICADO]. TI advierte (p.14) que son parámetros **relativos** a la amplitud de la señal: si la amplitud no es la de la hoja de datos, no se debe esperar el mismo valor. Además, en una medición con senoidal el ruido de la referencia **sí** entra en el SINAD, mientras que en el histograma con la entrada en corto no (p.47): por eso los dos ensayos de §6 y §8 no dan lo mismo.

**Presupuesto** [CÁLCULO] con `VREF = 3,7 V` y una senoidal a escala completa (1,85 V pico = 1,308 V rms):

| Concepto | Valor |
|---|---|
| ENOB mínimo para 60 dB | 9,67 bits |
| Ruido + distorsión máximo permitido | **1,31 mV rms = 1,45 LSB rms** |
| SINAD ideal de 12 bits (solo cuantización) | 74,0 dB (σq = 0,29 LSB) |
| Lo que queda para ruido, jitter, referencia, fuente y distorsión | 1,28 mV rms = 1,42 LSB rms (**14 dB de margen**) |

**El SINAD depende de la amplitud de la señal.** Con solo cuantización [CÁLCULO]:

| Amplitud de la senoidal (pico) | SINAD |
|---|---|
| 1,85 V (escala completa) | 74,0 dB |
| 0,90 V | 67,7 dB |
| 0,50 V | 62,6 dB |
| **0,185 V (límite lineal de PCB2)** | **54,0 dB** |

> **Advertencia.** Si el requisito se entiende como «60 dB con la señal real de PCB2» (±185 mV, el 10 % del rango), un ADC de 12 bits ideal da 54 dB y **no alcanza**; con el promedio de 5 muestras subiría a 61 dB solo si el ruido fuera blanco y de al menos 0,5 LSB. Si se entiende como una característica del **ADC**, medida con una senoidal casi a escala completa, un 12 bits ideal pasa con 14 dB de margen. En este documento se adopta la segunda lectura, que es la del enunciado («el SINAD del ADC»). **Hay que confirmarla con el profesor** (§13).

**Qué consume el presupuesto de 1,42 LSB rms:** el ruido de la referencia y de la fuente (§7), el jitter del disparo de Q (§4), la no linealidad del ADC (`AD02` ±1 LSb típ., ±2 máx.; `AD03` ±1 LSb; DS p.632, medidas a `VDD = 3 V` y `TAD = 1 µs`, no en nuestras condiciones), el rizado de los PWM de auto-cero (≈ 1 mV pp a la salida según `docs/especificacion_pwm.md`, que se pliega a la banda del ADC) y el ruido de PCB2. El offset y la ganancia (`AD04`, `AD05`, ±6 LSb máx.) **no** entran en el SINAD: son errores de continua.

**Cómo se mide** [PROPUESTA]:

1. Generador de funciones conectado **directo** al pin RC0 (y luego a RC1), a través de ≤ 10 kΩ, **sin PCB2**: el requisito es del ADC. Senoidal centrada en 1,85 V con 1,7 V de amplitud (sin recorte: de 0,15 V a 3,55 V). Se repite con una de 0,5 V para ver la dependencia.
2. Frecuencias de 5 Hz y de 20 Hz (la banda del diseño). Para que quepan ciclos enteros: con `fs = 200 Hz`, `M = 4096` y `k = 410` ciclos, `f = 20,02 Hz`; con `k = 82`, `f = 4,004 Hz`. Si el generador no se puede sincronizar, la ventana de Kaiser del script reduce el error.
3. Captura de 4096 pares por `DATA_IQ` (`ADC_N_PROMEDIO = 5`, 200 tramas/s, 21 s). Para el ADC puro se repite con `ADC_N_PROMEDIO = 1` (1000 tramas/s = 78 % de la línea; si `adcTramasPerdidas` sube, no vale esa captura).
4. Calcular con el script de abajo.

```python
import numpy as np

def sinad_db(codes, fs, f_in, N=12, vref=3.7):
    """SINAD (dB) y ENOB de una captura de una senoidal de frecuencia f_in."""
    v = (np.asarray(codes, float) - np.mean(codes)) * vref / 2**N  # quita el DC; a voltios
    w = np.kaiser(len(v), 14)                                      # ventana de lóbulos bajos
    spec = np.abs(np.fft.rfft(v * w)) ** 2                         # potencia por bin
    k0 = int(round(f_in * len(v) / fs))                            # bin de la fundamental
    p_sig = spec[max(k0 - 6, 1):k0 + 7].sum()                      # fundamental: ±6 bins
    p_nd = spec[1:].sum() - p_sig                                  # todo lo demás, sin DC
    sinad = 10 * np.log10(p_sig / p_nd)
    return sinad, (sinad - 1.76) / 6.02                            # dB, bits
```

Comprobado con capturas sintéticas (12 bits, 3,7 V, 1,7 V de amplitud): sin ruido 73,5 dB (teoría 73,3 dB); con 1 LSB rms de ruido 61,9 dB; con 1,45 LSB rms 59,1 dB. **Criterio de aceptación:** SINAD ≥ 60 dB en 5 Hz y 20 Hz, en RC0 y RC1.

## 7. Criterio para escoger la referencia (R4, R10)

**Criterios, en orden de prioridad:**

1. **Cubrir la señal sin recorte.** Las salidas de PCB2 pueden llegar a los rieles (0,102 V y 3,598 V en la simulación saturada del preinforme). La referencia debe ser ≥ la tensión máxima que llegue al pin.
2. **No superar la alimentación del chip.** Una entrada analógica no debe pasar de `VDD + 0,3 V`.
3. **Aprovechar el rango.** Con menos de 16 bits, TI (Tabla 3 y Fig. 7a) indica que bajar `Vref` baja el LSB y el ruido de cuantización, siempre que la señal siga cabiendo.
4. **Ruido de la referencia mucho menor que `σq`.**
5. **Ratiométrico si es posible.** El código es proporcional a `Vin/Vref` (TI Ec. 26).
6. **Disponibilidad** en el chip (pin `ADCVREF+` en RA1 está ocupado por ICSPCLK, ver §3.1 de `especificacion_diseno_uart.md`).

**Opciones con 12 bits y señal de 0 a 3,7 V:**

| Opción | `FSR` | `σq` | Veredicto |
|---|---|---|---|
| **`VDD = 3,7 V` (actual)** | 3,7 V | 261 µV | **Elegida:** cubre todo el rango, es ratiométrica con `Vref2` y no usa pines. Hereda el ruido de la fuente |
| FVR de 4,096 V | 4,096 V | 289 µV | **No sirve:** el FVR no puede superar `VDD` (DS p.491) |
| FVR de 2,048 V | 2,048 V | 144 µV | Recorta toda señal sobre 2,048 V. El rango lineal es 1,85 ± 0,185 V = hasta 2,035 V: margen de 13 mV. Solo si la salida está muy bien centrada |
| `VDD = 5 V` | 5 V | 352 µV | Es el caso de la cifra del profesor, pero la placa trabaja a 3,7 V |

**Ruido de la referencia y de la fuente.** TI (Cap. 4, Ecs. 26 y 27) combina ADC y referencia en cuadratura:

`V_N,total = √(σq² + (%FS · V_N,ref)²)`

**Criterio propuesto** [PROPUESTA]: que la referencia aporte ≤ 10 % de `σq`. Con `σq = 261 µV`, `V_N,ref ≤ 26 µV rms` a escala completa. El total sube solo de 261 a 262 µV.

> El PDF de TI analiza ADC **delta-sigma**, cuyo filtro digital limita el ancho de banda de ruido. El ADC del PIC es un **SAR sin ese filtro**: el ruido de banda ancha de `VDD` se muestrea y se pliega a 0–500 Hz. Se filtra con capacitores en el pin y con el promedio de 5.

- Si `VREF = VDD`, un rizado de 10 mV en la fuente con `Vin/Vref = 0,5` aporta 5 mV, unos 5,5 LSB, **salvo que la señal también varíe con `VDD`**. `Vref2 ≈ 1,85 V` sale de un divisor del riel de 3,7 V (preinforme §1.2), así que el nivel de reposo es ratiométrico y esa parte se cancela [CÁLCULO, supuesto: ambos del mismo riel]. Solo la parte de la señal (±185 mV) no se cancela.
- TI (Cap. 6, págs. 65–79) trata el ruido de la fuente con el PSRR. Para mejorarlo propone optimizar el *layout* y planificar las frecuencias; para depurar, agregar un LDO, comprobar las especificaciones de la fuente, aumentar los capacitores de desacoplo del ADC y **reemplazar las fuentes por una de banco, una por una** (p.79). El LDO y el desacoplo son lo aplicable aquí; el análisis de TI es de ADC delta-sigma.
- El rizado de los trimmers sustituidos por PWM se multiplica por 10 en la etapa de resta (preinforme ec. 5). Para quedar bajo 1 LSB (0,9 mV) hace falta ≲ 90 µV pp en I0/Q0 (es el CP-2 de `MASTER_PLAN.md`).

## 8. Medición del ruido: histograma y conteo (R8)

TI (págs. 11–13): es la medición de **ruido en DC** («input-short»): la entrada fija, el histograma de códigos es gaussiano, el ruido rms es una desviación estándar y el pico a pico es 6 o 6,6 σ. Con la entrada en 0 V **no entra** el ruido de la referencia (la razón `Vin/Vref` es cero, p.47); con la entrada en la mitad de la escala entra la mitad (Ec. 27 con %FS = 50 %).

1. Dos puntos: **entrada en 0 V** (pin a GND, para el ruido propio del ADC) y **entrada en `Vref2 ≈ 1,85 V`** (código ≈ 2048, el punto de trabajo, que incluye la referencia).
2. Capturar `M ≥ 4096` muestras (cifra propia; TI no fija un número). Con `ADC_N_PROMEDIO = 1` para ver el ADC solo, y con 5 para ver el resultado final.
3. Contar las ocurrencias de cada código y calcular:

| Métrica | Fórmula |
|---|---|
| Ruido rms | `σ` = desviación estándar de los códigos × `q` |
| Ruido pico a pico | `≈ 6·σ` a `6,6·σ` (TI p.13) |
| Resolución efectiva | `log2(FSR / σ)` bits |
| Resolución libre de ruido | `log2(FSR / V_N,pp)` bits |

**Lectura:** si el histograma ocupa 1 a 3 códigos, el ruido analógico es menor que 1 LSB y se ve cuantización (en DC aparece como offset, no como ruido, TI pág. 7). Si es ancho y gaussiano, domina el ruido analógico (rizado, fuente o referencia). Se compara con 261 µV (0,29 LSB) y con el criterio de σ de la prueba 4.3. Se repite con PCB2 apagada y encendida, para separar el ruido del ADC del de la placa.

```python
import numpy as np
codes = np.loadtxt("capturas_adc.csv")          # un código por línea (0..4095)
N, FSR = 12, 3.7
lsb = FSR / 2**N
sigma = codes.std(ddof=1) * lsb
vals, counts = np.unique(codes, return_counts=True)
print(f"códigos distintos: {len(vals)}  media: {codes.mean()*lsb:.4f} V")
print(f"rms: {sigma*1e6:.1f} uV ({sigma/lsb:.2f} LSB)  pp: {6.6*sigma*1e6:.1f} uV")
if sigma > 0:
    print(f"resol. efectiva: {np.log2(FSR/sigma):.2f} bits  libre de ruido: {np.log2(FSR/(6.6*sigma)):.2f} bits")
```

## 9. Prueba temporal con el Serial Plotter (`adc_prueba`)

- **Qué es:** código **temporal** (`adc_prueba.c/.h`) que, con `MODO_PRUEBA_ADC = 1`, manda `I:2048,Q:2051\r\n` (hasta 15 bytes) cada 50 pares. Es el formato `nombre:valor` del Serial Plotter de Arduino IDE.
- **Límites:** 20 líneas/s, así que solo dibuja bien señales de menos de 10 Hz. **No sirve** para el SINAD ni para el histograma (decima por 50 y pliega). No usa el protocolo.
- **Para qué:** ver que I y Q siguen a un generador conectado a RC0 y RC1, y que `adcPruebaLineasPerdidas` queda en 0.
- **Resultado:** pendiente. La persona anota el resultado en `docs/estado.md` (qué señal, qué frecuencia, qué se vio y valores de los contadores).

## 10. Multiplexado de I y Q (decisión adoptada)

Un solo ADC con multiplexor analógico entre RC0 (I) y RC1 (Q), en ráfaga dentro de cada tick de 1 ms: I convertido por hardware, Q arrancado por la ISR. Ambos pines pertenecen al mismo módulo ADC y están como analógicos (`ANSELC`).

**Sesgo temporal entre I y Q** [CÁLCULO]: Q se muestrea `Δt = T_canal` después de I. El error relativo es `2π·f·Δt`:

| Caso (§3) | `Δt` | Error a 20 Hz | Desfase |
|---|---|---|---|
| A (10 µs de adquisición) | 38,5 µs | 0,48 % | 0,28° |
| B (320 µs de adquisición) | 348,5 µs | **4,4 %** | 2,5° |

En el caso A es despreciable. En el B no lo es para un plano de impedancia I vs Q, y se corregiría reduciendo `ADACQ` o compensando en el PC.

**Precauciones al cambiar de canal:** el capacitor de muestreo conserva carga del canal anterior, por eso cada salida de PCB2 debe ser de baja impedancia y `T_acq` no se recorta; si un canal «se contamina» con el otro, se alarga `T_acq` o se descarta una conversión tras cambiar de canal.

**Alternativas descartadas por ahora:** alternar canales a 2 kHz (un canal por tick: sesgo de 500 µs, error de 6 % a 20 Hz) y muestreo simultáneo con dos circuitos de muestreo y retención o un ADC externo (innecesario con 20 Hz).

## 11. Protocolo de los datos (decisión D-2)

Misma trama de 9 bytes (`docs/especificacion_protocolo.md` §2). Tipos nuevos [VERIFICADO en `protocol.h`/`protocol.c`]:

| `TYPE` | Sentido | Nombre | Payload |
|---|---|---|---|
| `0x20` | PC → PIC | `ADC_STREAM` | `P0` = 0 detener, ≠ 0 iniciar el envío |
| `0xA0` | PIC → PC | `ADC_ACK` | copia el `SEQ` de la petición; `P0` = estado resultante (0 o 1) |
| `0xA1` | PIC → PC | `DATA_IQ` | I en `P0:P1`, Q en `P2:P3`, big-endian (12 bits en 16); `SEQ` propio 0–255 que **sube aunque la trama no quepa**, para que el PC vea el salto |

- Al arrancar, el envío está **apagado**: el PC lo enciende con `ADC_STREAM`.
- Ocupación de la línea (781 µs por trama): 200 tramas/s = **15,6 %** con `N = 5`; 1000 tramas/s = **78 %** con `N = 1`.
- Falta documentar estos tipos en `docs/especificacion_protocolo.md` (hecho en la sección 2 de ese archivo) y probarlos en el chip.

## 12. Decisiones y cambios que cita el código

El código cita `docs/instrucciones_claude_code_adc.md`, que **no estaba en la carpeta**. Lo que sigue se reconstruyó de los comentarios y debe revisarse.

| ID | Qué dice el código | Dónde |
|---|---|---|
| D-1 | `ADC_N_PROMEDIO = 5` | `adc_iq.h` |
| D-2 | tipos de trama `0x20`, `0xA0`, `0xA1` | `protocol.h` |
| D-3 | no aparece en el código | – |
| D-4 | pin de prueba RC2 durante la ISR | `adc_iq.h` |
| C-11 | `interrupt_manager.c` llama directo a `adc_iq_isr()` (antes iba por el puntero `ADCC_ADI_InterruptHandler`) | `interrupt_manager.c` |
| C-12 | tras el reset `ADPCH = 0x00` = RA0 = ICSPDAT; `adc_iq_init()` lo pone en `CANAL_I` | `adc_iq.c` |

## 13. Pendientes por confirmar

1. Con qué ADC y `Vref` calculó el profesor los 352 µV (§5).
2. Cómo se entiende el SINAD > 60 dB: sobre el ADC con una senoidal casi a escala completa (lectura adoptada) o sobre la señal real de PCB2 (§6).
3. Unidad de `ADACQ`: 10 µs o 320 µs (§3, prueba 4.16).
4. Duración real de las ISR de la UART y su efecto en el jitter de Q (§4, pruebas 1.10, 1.11 y 4.17).
5. Variación mínima de `V_I,OUT` y `V_Q,OUT` que se quiere distinguir (§5).
6. Si PCB2 deja la entrada del ADC con baja impedancia (≤ 10 kΩ) y qué rizado de fuente tiene `VDD`.
7. Resultados de la prueba del Serial Plotter (§9) y de las pruebas 4.x de `docs/protocolos_prueba.md`.
8. El documento `docs/instrucciones_claude_code_adc.md` y la decisión D-3 (§12).

## 14. Recomendaciones

**Hardware**
- [ ] `VDD`/`VREF` desde un LDO limpio con desacoplo cercano (100 nF + 1 a 10 µF), lejos de reguladores conmutados.
- [ ] Ninguna salida de PCB2 debe superar `VDD + 0,3 V`; resistencia en serie si hace falta.
- [ ] Salida de PCB2 de baja impedancia (op-amp), ≤ 10 kΩ.
- [ ] Tierra analógica y digital unidas en un solo punto cerca del ADC; PWM, UART y reloj lejos de `V_I,OUT`, `V_Q,OUT` y la referencia.
- [ ] Rizado de I0/Q0 ≲ 90 µV pp.
- [ ] Revisar la red de 60 Hz: el filtro de 20 Hz la atenúa solo ≈ 20 dB [CÁLCULO, Butterworth ideal con `fn = 19,4 Hz`].

**Firmware**
- [ ] Disparo por Timer0 (ya está). Nada de retardos por software.
- [ ] Confirmar `ADACQ` y bajarlo si la adquisición real es de 320 µs.
- [ ] Mantener cortas las ISR de la UART (pruebas 1.10 y 1.11) para el jitter de Q.
- [ ] Detectar saturación (códigos cerca de 0 o 4095) y pasar a ERROR/ALARM.
- [ ] `MODO_PRUEBA_ADC = 0` para volver al protocolo.

**Validación:** histograma (§8), SINAD (§6), periodo de muestreo y duración de la ISR con RC2 (pruebas 4.5, 4.13 y 4.16) y jitter de Q (4.17).

## 15. Referencias

- DS40001985C: cap. 32 (ADC2), p.501–540; AD01–AD23, p.632–633; FVR, cap. 30, p.491; Timer0, cap. 18, p.260.
- Errata 80000799, punto 1.1.2 (ADCC con doble muestra): **no aplica**, porque el código usa el modo básico.
- TI, *Fundamentals of Precision ADC Noise Analysis* (SLYY192A, Rev. A, marzo 2025, 83 páginas): Cap. 1 (págs. 4–22), Cap. 4 (46–57), Cap. 5 (58–64), Cap. 6 (65–79). Citas usadas, ya comprobadas en el PDF: Ec. 2 (p.7), Fig. 7a (p.10), Tablas 2 y 3 (págs. 13–15), Ec. 26 (p.47), Ec. 27 (p.49), Tabla 21 (p.56), Ec. 28 (p.59).
- Preinforme de la Práctica 2: filtros de 20 Hz, `Vref2`, ecuaciones (5) y (8).
- `docs/Especificacion_Tecnica_Practica3_Microcontroladores.md`: muestreo por Timer, Nyquist y filtro antialiasing.
