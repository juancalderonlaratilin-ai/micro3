# Especificación del ADC: muestreo de I y Q (tarea T-09)

Proyecto Micro 3 · Fase 2 · Fecha: 2026-10-30 · Revisado: 2026-11-05 · Estado: **código implementado (T-09), compilado con XC8 2.40, sin probar en el chip**. Los requisitos de §1 vienen del profesor y de la persona.

> **Para qué sirve:** reunir en un solo sitio qué pidió el profesor para el ADC, qué hace el código que ya existe, los presupuestos de tiempo, ruido, jitter y SINAD, y las recomendaciones de hardware y firmware. Reemplaza al borrador suelto `ADC_especificaciones_y_recomendaciones.md`.
>
> **Etiquetas:** **[VERIFICADO]** = sale del datasheet (con página), del código o de un documento de `docs/`. **[CÁLCULO]** = cuenta propia comprobada con un script. **[PROPUESTA]** = decisión por aprobar. **[SIN VERIFICAR]** = dudas que se resuelven midiendo.
>
> **Fuentes:** indicaciones del profesor (chat, 2026-10-28); *Fundamentals of Precision ADC Noise Analysis* de TI (SLYY192A, marzo 2025), citado como «TI»; el preinforme de la Práctica 2; el datasheet DS40001985C («DS»); y el código recibido el 2026-10-30 (`adc_iq.c/.h`, `main.c`, `mcc_generated_files/adcc.*` y `tmr0.*`; el envío de texto está hoy en `adc_ascii.c/.h`).

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
| R11 | Filtro anti-aliasing | **ninguno en esta entrega**; el criterio `fc ≤ fs/2` del enunciado queda pendiente (prueba 4.7) | persona, 2026-11-05 |

## 2. Qué hace el código actual (T-09) [VERIFICADO en el código del zip]

```
Timer0 (cada 1 ms, por hardware) ──► ADGO = 1 ──► convierte I (RC0)
        adc_iq_isr: guarda I, ADPCH = Q, ADGO = 1 ──► convierte Q (RC1)
        adc_iq_isr: guarda Q, ADPCH = I, deja el par listo ──► avisa a main (cada 1 ms)
main:   adc_iq_get() ──► pwm_desde_adc() (T-06) y adc_ascii_par() ──► línea "xxxx,xxxx\r\n" (1 de cada 2 pares: 500/s, sin promedio)
```

| Dato | Valor | Dónde / fuente |
|---|---|---|
| Disparo | `ADACT = 0x02` = *TMR0_overflow* (DS Tabla 32-6, p.538); ver §2.1 | `adcc.c` |
| Timer0 | 8 bits, reloj Fosc/4 = 4 MHz, prescaler 1:16 → 250 kHz, `TMR0H = 249` → 250 cuentas × 4 µs = **1000 µs** [CÁLCULO] | `tmr0.c` (`T0CON1 = 0x44`, `T0CON0 = 0x80`) |
| Reloj del ADC | `ADCLK = 0x0F` → Fosc/32 → `TAD = 2 µs` | `adcc.c`; MCC lo confirma |
| Conversión | `14·TAD + 2·TCY = 28,5 µs` (`AD22`, DS p.633) [CÁLCULO] | MCC: 28,5 µs |
| Adquisición | `ADACQ = 160` | **ver §3: 10 µs o 320 µs** |
| Resultado | 12 bits, justificado a la derecha (`ADFM`), modo básico (`ADCON2 = 0`) | `adcc.c` |
| Canales | `CANAL_I = 0x10` (RC0/ANC0), `CANAL_Q = 0x11` (RC1/ANC1) (DS p.528) | `adcc.h` |
| Referencia | `ADREF = 0x00`: `ADPREF = VDD`, `ADNREF = VSS`; ver §7.1 | `adcc.c` |
| Interrupción | `ADIE = 1`; `interrupt_manager.c` llama a `adc_iq_isr()` si `ADIE` y `ADIF` | cambio C-11 (§11) |
| Frecuencia por canal | **1 kHz para I y 1 kHz para Q**: en cada disparo se convierten los dos (Q ≈ 38,5 µs después de I en el caso A, §10) | `adc_iq.c` |
| Quién usa cada par | el PWM, **todos** (1000/s, `pwm_desde_adc`); la UART, 1 de cada 2 (500 líneas/s, `adc_ascii_par`) | `main.c` |
| Promedio | **ninguno** (desde el 2026-11-03; reemplaza D-1): una muestra de I y una de Q por par, 1000 pares/s en el PIC; al PC llegan 500/s | `adc_iq.c`, `adc_ascii.h` |
| Pin de prueba | `ADC_PIN_PRUEBA = 2`: RC2 saca la salida del Timer0 (cuadrada de 500 Hz, cada flanco = un disparo). Con `1`, RC2 sube mientras corre `adc_iq_isr()` (D-4) | `adc_iq.h` |
| Contadores | `adcParesPerdidos` y `adcLineasPerdidas` deben quedar en 0 | `adc_iq.c`, `adc_ascii.c` |

Sin promedio, ni la ISR ni `adc_iq_get` suman o dividen: la ISR deja el par listo al terminar Q y `adc_iq_get` lo copia con `ADIE` apagado. Si `main` tarda más de 1 ms en leerlo, el par se pierde y se cuenta en `adcParesPerdidos`.

**Envío al PC:** `main.c` tiene un solo modo: cada par de `adc_iq_get()` va a `pwm_desde_adc()` (PWM de reconstrucción, `docs/especificacion_pwm.md`) y a `adc_ascii_par()`, que manda una línea `xxxx,xxxx\r\n` cada `ADC_PARES_POR_LINEA` = 2 pares (§11).

### 2.1 Fuente de disparo (`ADACT`): cuál usamos y por qué

**Usamos `ADACT.ACT` = `00010` = `TMR0_overflow`** (DS Tabla 32-6, p.538; el código escribe `ADACT = 0x02`). Cada vez que el Timer0 desborda, el hardware pone `GO` y arranca la conversión del canal que esté en `ADPCH` (I). `ADACT` solo elige **una** fuente a la vez; por eso Q no se puede disparar por hardware con otra y lo arranca la ISR con `ADGO = 1` (de ahí el jitter de Q, §4).

**Por qué el Timer0:**

1. **Periodo exacto por hardware.** En modo de 8 bits el Timer0 compara `TMR0L` con `TMR0H` y, al coincidir, se reinicia (DS p.261). Con `TMR0H = 249`, reloj Fosc/4 (`T0CS` = 010, Tabla 18-1, p.267) y prescaler 1:16 (`T0CKPS` = 0100) salen 250 cuentas × 4 µs = **1000 µs** sin que el software recargue nada ni entre en una interrupción de timer. [VERIFICADO + CÁLCULO]
   - **No hay recarga por software, así que no aporta jitter.** En modo de 8 bits el Timer0 reinicia `TMR0L` solo, por hardware, cuando coincide con `TMR0H` (DS p.261); `TMR0H` es el registro de periodo y no se toca. Ninguna parte del firmware llama a `TMR0_Reload` ni a `TMR0_WriteTimer`, y `TMR0IE` no se habilita. [VERIFICADO en el código del zip] Aquí la palabra «overflow» de la tabla del DS significa «coincidencia», no un paso de 255 a 0.
   - Si en cambio se recargara el timer desde una ISR, cada periodo se alargaría lo que tarde la CPU en llegar a esa escritura (latencia de interrupción variable: hay un solo vector y la ISR de la UART puede estar corriendo), y el instante de disparo de I sí tendría jitter. [CÁLCULO]
   - **Duda [SIN VERIFICAR]:** el DS dice que el ADC arranca con el *flanco de subida* de la fuente (p.504) y que `TMR0_out` *conmuta* en cada coincidencia con postscaler 1:1 (§18.3.2, p.262). No dice si el `TMR0_overflow` de la Tabla 32-6 es el pulso de cada coincidencia (periodo de 1000 µs) o la salida que conmuta (un flanco de subida cada 2 coincidencias: 2000 µs). Se resuelve con la prueba 4.5: si el periodo entre los pulsos de I sale 2000 µs, hay que poner `TMR0H` = 124 (125 cuentas × 4 µs = 500 µs por coincidencia).
2. **Es el timer que sobra.** El Timer2 lo usan el PWM6/PWM7 que reconstruyen I y Q (`T2PR` = 255, 15,625 kHz; `docs/especificacion_pwm.md`) y ese periodo no sirve para 1 ms. La especificación de diseño ya reservaba «Timer 1 o Timer 0» para el disparo del ADC (`especificacion_diseno_uart.md` §4). Ningún otro módulo del código actual usa el Timer0.
3. **Cumple la regla del proyecto:** sin esperas ni retardos por software.

No está escrito en ningún documento por qué se prefirió el Timer0 al Timer4 o al Timer6, que también servirían (la instrucción original del ADC no está en la carpeta). [SIN VERIFICAR: motivo exacto de esa elección]

**Todas las fuentes posibles** (DS Tabla 32-6, p.538) y por qué no se usan:

| `ACT` | Fuente | ¿Sirve para 1 kHz? | Motivo |
|---|---|---|---|
| **`00010`** | **`TMR0_overflow`** | **Sí: la que usamos** | periodo fijado por hardware (§2.1, punto 1) |
| `00100`, `00110`, `01000` | `TMR2`, `TMR4`, `TMR6` *postscaled* | Sí, técnicamente | también tienen registro de periodo; Timer2 es del PWM; Timer4 y Timer6 quedan libres como alternativa |
| `00011`, `00101`, `00111` | `TMR1`, `TMR3`, `TMR5` *overflow* | No es cómodo | son de 16 bits y desbordan cada 65 536 cuentas: para 1 ms habría que recargarlos por software en cada vuelta |
| `01010` a `01101` | `CCP1` a `CCP4` *trigger* | Posible, más complejo | no se estudió; no compensa teniendo el Timer0 |
| `01110`, `01111` | `PWM6_out`, `PWM7_out` | No | siguen el periodo del PWM (15,6 kHz) y esos módulos reconstruyen I y Q |
| `00001` | pin de `ADACTPPS` | No | pide un pin libre y un generador externo |
| `10000` a `10111` | `NCO1`, `C1`, `C2`, cambio de pin (IOC), `CLC1` a `CLC4` | No | no son un reloj de 1 kHz |
| `11111`, `11101`, `11100` | escribir `ADPCH`, leer `ADRESH`, leer `ADERRH` | No | los dispara el software: tienen jitter |
| `00000` | desactivado | Solo para `ADGO` por software | así se arranca Q |

## 3. Presupuesto de tiempo (R2: menos de 1 ms)

Por cada canal: `T_canal = T_acq + T_conv`. Con dos canales en ráfaga: `T_total = 2·T_canal` más la latencia de la ISR.

| Caso | `T_acq` | `T_conv` | Por canal | I + Q | Margen sobre 1 ms |
|---|---|---|---|---|---|
| **A.** `ADACQ` cuenta ciclos de Fosc (lo que guarda MCC: `AquisitionTime` = 10 µs en el `.mc3`) | 10 µs | 28,5 µs | 38,5 µs | **77 µs** | 923 µs |
| **B.** `ADACQ` cuenta periodos del reloj del ADC, es decir `TAD` (lo que dice el DS p.529: «*Number of ADC clock periods*») | 160 × 2 µs = 320 µs | 28,5 µs | 348,5 µs | **697 µs** | 303 µs (30 %) |

- **Los dos casos cumplen R2.** El peor caso (B) deja el 30 % de margen.
- **Duda [SIN VERIFICAR]:** la figura 32-2 del DS rotula la adquisición como «1-8191 `TCY`» (DS p.501) y el texto del registro en periodos del reloj del ADC (p.529). MCC guarda 10 µs (`AquisitionCount` = 160 y `AquisitionTime` = 10 µs en el `.mc3`: divide entre 16 MHz). Si contara `TCY` (250 ns) serían 40 µs, un tercer caso con ≈ 68,5 µs entre pulsos. Se resuelve midiendo (prueba 4.16): la separación entre los dos pulsos de RC2 es ≈ 38,5 µs en el caso A y ≈ 349 µs en el B.
- **Si resulta ser el caso B**, la adquisición de 320 µs es excesiva: `AD23` pide `TACQ` típico de 2 µs (DS p.633) y `AD08` recomienda una fuente de ≤ 10 kΩ (DS p.632). Ver el sesgo entre I y Q en §10 y el cambio propuesto CP-7.
- El disparo automático no garantiza los tiempos: *«It is the user's responsibility to ensure that the ADC timing requirements are met»* (DS §32.2.6).

### 3.1 Fórmula del tiempo de adquisición y del `ADACQ`

Fuente: DS §32.3 y Ecuación 32-1 (p.506–507). Es el tiempo que necesita el capacitor de retención `CHOLD` para cargarse hasta quedar a **½ LSB** (4096 pasos) del voltaje del pin. El interruptor de muestreo está cerrado ese tiempo; después se abre y empieza la conversión.

```
TACQ  = TAMP + TC + TCOFF
TC    = − CHOLD · (RIC + RSS + RS) · ln( 1 / (2^(n+1) − 1) )      con n = 12:  ln(8191) = 9,011
TCOFF = (T − 25 °C) · 0,05 µs/°C
ADACQ = ceil( margen · TACQ / T_unidad )
```

| Símbolo | Valor | Fuente / estado |
|---|---|---|
| `CHOLD` | 28 pF | DS Fig. 32-4, p.506 [VERIFICADO] |
| `RIC` | 1 kΩ | DS Fig. 32-4 [VERIFICADO] |
| `RSS` | 7 kΩ a 5 V; sube al bajar `VDD`. A 3,7 V se usa **10 kΩ** | la gráfica de la Fig. 32-4 no se lee en el texto del PDF: valor conservador [SIN VERIFICAR] |
| `RS` | impedancia que ve el pin: la salida del opa de PCB2 más lo que haya en serie. Máximo recomendado 10 kΩ (`AD08`, p.632) | ver §3.2 |
| `TAMP` | 2 µs (asentamiento del amplificador interno) | DS Ec. 32-1 [VERIFICADO] |
| `TCOFF` | 0,05 µs por °C sobre 25 °C | DS Ec. 32-1 [VERIFICADO] |
| `T_unidad` | 2 µs (`TAD`) si `ADACQ` cuenta periodos del reloj del ADC (DS p.529); 62,5 ns si es como lo guarda MCC | [SIN VERIFICAR] hasta la prueba 4.16 (§3) |
| `margen` | 1,2 | [PROPUESTA] |

[CÁLCULO] La fórmula reproduce el ejemplo del datasheet: con `RS` = 10 kΩ, `RSS` = 7 kΩ y 50 °C da 7,79 µs, igual que el DS. Con `RSS` = 10 kΩ y `TAD` = 2 µs:

| `RS` | `TACQ` a 25 °C | `TACQ` a 50 °C | `ADACQ` mínimo a 50 °C | con 20 % de margen |
|---|---|---|---|---|
| 200 Ω (opa, estimado) | 4,83 µs | 6,08 µs | 4 (8 µs) | 4 (8 µs) |
| 1 kΩ | 5,03 µs | 6,28 µs | 4 (8 µs) | 4 (8 µs) |
| 10 kΩ (máximo recomendado) | 7,30 µs | 8,55 µs | 5 (10 µs) | **6 (12 µs)** |

A 85 °C y con 10 kΩ saldrían 10,3 µs: con `ADACQ` = 6 sigue cumpliendo. Los 200 Ω son una estimación mía, no un dato del datasheet.

```python
from math import log, ceil

def t_acq_us(rs_ohm, rss_ohm=10e3, ric_ohm=1e3, chold_f=28e-12, bits=12, temp_c=50.0, t_amp_us=2.0):
    """Tiempo de adquisición mínimo en µs (DS Ec. 32-1, p.506-507)."""
    tc_us = chold_f * (ric_ohm + rss_ohm + rs_ohm) * log(2**(bits + 1) - 1) * 1e6
    return t_amp_us + tc_us + (temp_c - 25.0) * 0.05

def adacq(rs_ohm, unidad_us=2.0, margen=1.2, **kw):
    """Valor para el registro ADACQ. unidad_us = 2.0 si cuenta periodos de TAD."""
    return ceil(margen * t_acq_us(rs_ohm, **kw) / unidad_us)
```

**Por qué no acortarlo más.** `CHOLD` no se descarga entre conversiones (DS p.507, nota 2): al pasar de I a Q conserva el voltaje de I. Lo que no se asiente queda como error entre canales: `residuo = ΔV · e^(−t/τ)`, con `τ = CHOLD·(RIC + RSS + RS)`. Para que el residuo sea menor que ½ LSB hacen falta `t ≥ τ·ln(ΔV / ½ LSB)`: 6,7 τ para un escalón de 0,37 V (410 LSB, el rango lineal de PCB2) y 8,9 τ para 3,4 V (la senoidal de la prueba 4.15). El presupuesto total de ruido para SINAD > 60 dB es de solo 1,45 LSB rms (§6).

**Con el valor actual** (`ADACQ = 160`): 10 µs cumplen el peor caso de 8,55 µs con poco margen (17 %); 40 µs sobran; 320 µs sobran 37 veces y solo empeoran el sesgo entre I y Q (§10). El cambio CP-7 de `MASTER_PLAN.md` aplica esta fórmula una vez medida la unidad (prueba 4.16).

### 3.2 Impedancia de salida del opa (`RS` de la fórmula): hay que tenerla en cuenta

`RS` no es la resistencia de un cable. Es **la impedancia que ve el pin del ADC**: para RC0 y RC1 es la **salida del amplificador operacional de la etapa final de PCB2** (resta + ganancia, preinforme ec. 5), más cualquier resistencia o filtro RC que haya en serie o en el pin. Hay que tenerla en cuenta por tres motivos:

1. **Entra directo en `TC`** (§3.1). Cada 1 kΩ de `RS` suma ≈ 0,25 µs a `TACQ` [CÁLCULO].
2. **La impedancia de salida de un opa no es constante.** En lazo cerrado vale aproximadamente `Ro / (1 + A(f)·β)`: es muy baja en continua y sube hacia `Ro` cuando la ganancia del lazo cae con la frecuencia. El golpe de carga de `CHOLD` al cerrarse el interruptor ocurre en cientos de kHz a MHz, donde la impedancia es mayor que la de continua. [teoría estándar; no se verificó en ningún datasheet]
3. **El opa tiene que recuperarse del golpe de carga.** Su ancho de banda en lazo cerrado es `GBW / ganancia de ruido`. La etapa final usa 100 kΩ / 10 kΩ, o sea ganancia de ruido 11. Con los `GBW` del documento de cambios de op-amps del proyecto [CÁLCULO, un solo polo]. El del MCP604 (2,8 MHz) está comprobado en su datasheet (DS21314G, p.3); los de TS924A y TLV9064 no se comprobaron:

| Opa | `GBW` | Ancho de banda con ganancia 11 | `τ` | 9 `τ` |
|---|---|---|---|---|
| TS924A (ST) | 4 MHz | 364 kHz | 0,44 µs | 3,9 µs |
| MCP604 (Microchip) | 2,8 MHz | 255 kHz | 0,62 µs | 5,6 µs |
| TLV9064 (TI) | 10 MHz | 909 kHz | 0,18 µs | 1,6 µs |

El golpe de carga es pequeño y no un escalón completo, así que 9 `τ` es una cota alta. Aun así es del orden del `TACQ` de §3.1: **no hay que recortar `T_acq`** y hay que medirlo (prueba 4.8).

**Lo que dice el datasheet del MCP604** (`docs/MCP601-2.PDF`, MCP601/1R/2/3/4, DS21314G) [VERIFICADO]:

- **No da un número de impedancia de salida.** Solo dice que «las salidas del opa son fuentes de voltaje de baja impedancia» (§3.1, p.11). Para tener un valor hay que simular con su modelo SPICE (§5.1) o medir.
- Con ganancia +1: `GBWP` 2,8 MHz típ., margen de fase 50°, slew rate 2,3 V/µs y tiempo de asentamiento al 0,01 % de 4,5 µs para un escalón de 3,8 V, medido con `RL` = 100 kΩ y `CL` = 50 pF (p.3).
- Corriente de cortocircuito ±22 mA a 5,5 V y ±12 mA a 2,7 V (p.2). A 3,7 V no está especificada.
- **Cargas capacitivas** (§4.4, p.13): con ganancia +1 y más de 40 pF recomienda una resistencia en serie `RISO` en la salida. El pin del ADC ya presenta ≈ 33 pF (`CHOLD` 28 pF + `CPIN` 5 pF), más la capacitancia de la pista y de la punta del osciloscopio. Esa `RISO` se suma a `RS`. Con ganancia de ruido 11 la estabilidad es más fácil [teoría estándar], pero conviene comprobarla en simulación y en la placa.

**Qué hay que hacer** [PROPUESTA]:

- [ ] Anotar el modelo del opa de la etapa final de I y Q. Según `CAMBIOS_OPAMPS_Y_AMPLITUDES_2026-10-16.md` (en el proyecto de Claude, no está en `docs/`), hoy PCB2 usa TS924A y la BOM puede pasar a MCP604 o TLV9064.
- [x] Datasheet del MCP604 en `docs/MCP601-2.PDF` (lo puso la persona el 2026-10-30). Los de TS924A y TLV9064, si se usan, faltan. **Aviso:** `.gitignore` tiene `docs/*.pdf` (PDFs pesados de fabricante), así que los PDF no se suben a GitHub salvo con `git add -f`.
- [ ] Conseguir un número de `Zout`: el datasheet del MCP604 **no lo da**. Opciones [PROPUESTA]: simular la etapa final en LTspice con el modelo SPICE del MCP604 (impedancia de salida contra frecuencia, a ≈ 1 MHz) o medir en la placa cuánto cambia el voltaje de salida al conectar una carga conocida. Anotar el valor como `RS`.
- [ ] Sumar a `RS` cualquier resistencia en serie en el pin del PIC (en esta entrega no hay RC anti-aliasing).
- [ ] Recalcular §3.1 con ese `RS` y fijar `ADACQ`.
- [ ] Comprobar con la prueba 4.8, **también con la salida real de PCB2** y no solo con el generador.

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

Sin promedio (desde el 2026-11-03), el jitter de Q entra completo en cada muestra: antes el promedio de 5 reducía su parte aleatoria. La del reloj de conversión no cuenta.

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

> **Advertencia.** Si el requisito se entiende como «60 dB con la señal real de PCB2» (±185 mV, el 10 % del rango), un ADC de 12 bits ideal da 54 dB y **no alcanza**, y ya no hay promedio que lo suba (con el de 5 que había antes llegaba a 61 dB solo si el ruido era blanco y de al menos 0,5 LSB). Si se entiende como una característica del **ADC**, medida con una senoidal casi a escala completa, un 12 bits ideal pasa con 14 dB de margen. En este documento se adopta la segunda lectura, que es la del enunciado («el SINAD del ADC»). **Hay que confirmarla con el profesor** (§13).

**Qué consume el presupuesto de 1,42 LSB rms:** el ruido de la referencia y de la fuente (§7), el jitter del disparo de Q (§4), la no linealidad del ADC (`AD02` ±1 LSb típ., ±2 máx.; `AD03` ±1 LSb; DS p.632, medidas a `VDD = 3 V` y `TAD = 1 µs`, no en nuestras condiciones), el acople de los PWM de reconstrucción (15,6 kHz en RA2/RA4, que se pliega a la banda del ADC si llega a RC0/RC1) y el ruido de PCB2. El offset y la ganancia (`AD04`, `AD05`, ±6 LSb máx.) **no** entran en el SINAD: son errores de continua.

**Cómo se mide** [PROPUESTA]:

1. Generador de funciones conectado **directo** al pin RC0 (y luego a RC1), a través de ≤ 10 kΩ, **sin PCB2**: el requisito es del ADC. Senoidal centrada en 1,85 V con 1,7 V de amplitud (sin recorte: de 0,15 V a 3,55 V). Se repite con una de 0,5 V para ver la dependencia.
2. Frecuencias de 5 Hz y de 20 Hz (la banda del diseño). Para que quepan ciclos enteros: con `fs = 500 Hz` en el PC, `M = 4096` y `k = 163` ciclos, `f = 19,90 Hz`; con `k = 41`, `f = 5,005 Hz` (`k` impar para que no se repitan códigos). Si el generador no se puede sincronizar, la ventana de Kaiser del script reduce el error.
3. Captura de 4096 pares por las líneas ASCII (500 líneas/s = 48,1 % de la línea, 8,2 s). Cada par ya es el ADC puro: el PIC manda 1 de cada 2, sin promediar. Si `adcLineasPerdidas` sube o el PC mide menos de 500 líneas/s, no vale esa captura.
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

> El PDF de TI analiza ADC **delta-sigma**, cuyo filtro digital limita el ancho de banda de ruido. El ADC del PIC es un **SAR sin ese filtro**: el ruido de banda ancha de `VDD` se muestrea y se pliega a 0–500 Hz. Se podría filtrar con capacitores en el pin; en esta entrega no hay filtro anti-aliasing ni promedio digital.

- Si `VREF = VDD`, un rizado de 10 mV en la fuente con `Vin/Vref = 0,5` aporta 5 mV, unos 5,5 LSB, **salvo que la señal también varíe con `VDD`**. `Vref2 ≈ 1,85 V` sale de un divisor del riel de 3,7 V (preinforme §1.2), así que el nivel de reposo es ratiométrico y esa parte se cancela [CÁLCULO, supuesto: ambos del mismo riel]. Solo la parte de la señal (±185 mV) no se cancela.
- TI (Cap. 6, págs. 65–79) trata el ruido de la fuente con el PSRR. Para mejorarlo propone optimizar el *layout* y planificar las frecuencias; para depurar, agregar un LDO, comprobar las especificaciones de la fuente, aumentar los capacitores de desacoplo del ADC y **reemplazar las fuentes por una de banco, una por una** (p.79). El LDO y el desacoplo son lo aplicable aquí; el análisis de TI es de ADC delta-sigma.
- *(Macro)* El rizado de los trimmers sustituidos por PWM se multiplica por 10 en la etapa de resta (preinforme ec. 5); en Micro 3 el PWM no entra a PCB2, así que no aplica (cambio de enfoque, 2026-11-04).

### 7.1 Origen de `Vref`: dentro de la PIC o referencia externa

**No confundir las dos «referencias» del proyecto:**

- `Vref` y `Vref2` de PCB2 (≈ 1,85 V) son buffers del divisor del riel: son la **tierra virtual** de las señales y fijan el nivel de reposo de I y Q (preinforme §1.2). **No** son la referencia del ADC.
- `VREF+` del ADC es el voltaje de fondo de escala: el código es `Vin / VREF+ × 4096`. Se elige con `ADREF` (`ADPREF`, DS p.537).

| Opción | `ADPREF` | La genera | A favor | En contra |
|---|---|---|---|---|
| **`VDD` (la actual)** | `00` | la PIC (su alimentación) | no usa pines ni piezas; cubre 0 a 3,7 V; **ratiométrica** con `Vref2`, que sale del mismo riel | el ruido de `VDD` entra directo en la medida |
| FVR de 1,024 / 2,048 / 4,096 V | `11` | la PIC (referencia interna) | independiente de `VDD` (DS p.491) | no puede superar `VDD`: 4,096 V no sirve; 2,048 V recorta la señal; **no es ratiométrica** |
| Pin `VREF+` (referencia externa) | `10` | un CI de referencia en la placa | estabilidad y ruido bajo si el CI es bueno | usa RA1, que es ICSPCLK: choca con la programación y depuración del SNAP (regla del proyecto: no usar RA0, RA1 ni RA3); pieza y pista extra |

[CÁLCULO] **Por qué importa lo de ratiométrica.** Si `VDD` tiene 10 mV de rizado, `Vref2` (la mitad de `VDD`) se mueve 5 mV. Con `VDD` como referencia el código de reposo no cambia (siempre 2048); con el FVR de 2,048 V el código se movería ≈ 10 LSB. Supuesto: `Vref2` sale de un divisor del mismo riel que alimenta la PIC.

**Decisión actual:** `VDD` [PROPUESTA ya adoptada en el código; falta confirmarla con el profesor]. Condición: `VDD` limpio (LDO y desacoplo, §14).

**Cómo decidir con datos** [PROPUESTA]:

1. Medir el ruido de `VDD` en el pin de la PIC (osciloscopio con acople AC y límite de ancho de banda) y compararlo con el criterio de ≤ 26 µV rms de §7.
2. Comparar la σ del histograma (§8) con la entrada en 0 V y con la entrada en `Vref2`: la diferencia es lo que aporta la referencia y la fuente.
3. Cambiar a FVR o a una referencia externa **solo si** la prueba 4.15 (SINAD) falla por culpa de la referencia y el choque con RA1 tiene solución.

## 8. Medición del ruido: histograma y conteo (R8)

TI (págs. 11–13): es la medición de **ruido en DC** («input-short»): la entrada fija, el histograma de códigos es gaussiano, el ruido rms es una desviación estándar y el pico a pico es 6 o 6,6 σ. Con la entrada en 0 V **no entra** el ruido de la referencia (la razón `Vin/Vref` es cero, p.47); con la entrada en la mitad de la escala entra la mitad (Ec. 27 con %FS = 50 %).

1. Dos puntos: **entrada en 0 V** (pin a GND, para el ruido propio del ADC) y **entrada en `Vref2 ≈ 1,85 V`** (código ≈ 2048, el punto de trabajo, que incluye la referencia).
2. Capturar `M ≥ 4096` muestras (cifra propia; TI no fija un número). Sin promedio, cada muestra ya es el ADC solo (no hace falta recompilar).
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

## 9. Ver I y Q en el Serial Plotter

- Las líneas `2048,2051\r\n` (hasta 11 bytes) que manda `adc_ascii.c` son números separados por coma, que el Serial Plotter de Arduino IDE entiende: se ven directo, sin programa intermedio.
- A 500 líneas/s corre rápido. Para mirar con calma una señal lenta se puede subir `ADC_PARES_POR_LINEA` (50 = 20 líneas/s), pero así **no sirve** para el SINAD ni para el histograma (decima por 50 y pliega). Para medir, dejarlo en 2.
- **Para qué:** ver que I y Q siguen a un generador conectado a RC0 y RC1, y que `adcLineasPerdidas` queda en 0.
- **Resultado:** la persona confirmó el 2026-11-04 que el modo ASCII funciona. Falta anotar en `docs/estado.md` qué señal, qué frecuencia y los contadores.

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

## 11. Envío de los datos al PC (texto ASCII)

Desde el 2026-11-04 (decisión de la persona) los datos van como texto, una línea por par (`docs/especificacion_protocolo.md`):

```
2048,2051\r\n
```

- Una línea cada `ADC_PARES_POR_LINEA` = 2 pares: **500 líneas/s**, fs = 500 Hz en el PC. El ADC sigue muestreando a 1 kHz con el Timer0, así que el instante de cada muestra no cambia.
- Ocupación de la línea: 11 bytes × 500 / 11 429 B/s = **48,1 %**. Una línea por par (1000/s) cabría apenas: 0,96 ms de cada 1 ms (96 %), sin margen.
- El envío empieza solo al encender el PIC; no hay comandos del PC.
- Sin suma de verificación ni número de secuencia: una línea dañada se descarta en el PC si pierde la forma `número,número`; una perdida no se ve (por eso se revisan `adcLineasPerdidas` y las líneas por segundo).

## 12. Decisiones y cambios que cita el código

El código cita `docs/instrucciones_claude_code_adc.md`, que **no estaba en la carpeta**. Lo que sigue se reconstruyó de los comentarios y debe revisarse.

| ID | Qué dice el código | Dónde |
|---|---|---|
| D-1 | `ADC_N_PROMEDIO = 5`. **Retirada el 2026-11-03** por la persona: ya no hay promedio | (antes `adc_iq.h`) |
| D-2 | formato de los datos hacia el PC. **Cambiado el 2026-11-04**: ahora líneas de texto `I,Q` (§11) | `adc_ascii.c` |
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
9. Modelo del opa de las salidas I y Q de PCB2 (TS924A o MCP604) y su `Zout`: el datasheet del MCP604 ya está en `docs/MCP601-2.PDF` y no da el número (§3.2).
10. Si `Vref` se queda en `VDD` o se pasa a FVR o a una referencia externa (§7.1).
11. `RSS` a 3,7 V (gráfica de la Fig. 32-4 del DS) y por qué se eligió Timer0 y no Timer4 o Timer6 (§2.1, §3.1).
12. Si el `TMR0_overflow` que dispara el ADC es el pulso de cada coincidencia (1000 µs) o la salida que conmuta (2000 µs); lo decide la prueba 4.5 (§2.1). El comentario de `adc_iq.h` supone lo primero (cada flanco de `TMR0_out` es un disparo).
13. Filtro anti-aliasing: no hay en esta entrega; definir si se agrega y con qué corte (R11).

## 14. Recomendaciones

**Hardware**
- [ ] `VDD`/`VREF` desde un LDO limpio con desacoplo cercano (100 nF + 1 a 10 µF), lejos de reguladores conmutados.
- [ ] Ninguna salida de PCB2 debe superar `VDD + 0,3 V`; resistencia en serie si hace falta.
- [ ] Salida de PCB2 de baja impedancia (op-amp), ≤ 10 kΩ. Anotar su `Zout` (§3.2).
- [ ] Tierra analógica y digital unidas en un solo punto cerca del ADC; PWM, UART y reloj lejos de `V_I,OUT`, `V_Q,OUT` y la referencia.
- [ ] *(Macro)* Rizado de I0/Q0 ≲ 90 µV pp.
- [ ] Revisar la red de 60 Hz: el filtro de 20 Hz de PCB2 la atenúa solo ≈ 20 dB [CÁLCULO, Butterworth ideal con `fn = 19,4 Hz`]; en esta entrega no hay otro filtro antes del ADC.

**Firmware**
- [ ] Disparo por Timer0 (ya está). Nada de retardos por software.
- [ ] Confirmar `ADACQ` con la prueba 4.16 y calcularlo con la fórmula de §3.1 (con 20 % de margen); bajarlo si la adquisición real es de 320 µs.
- [ ] Mantener cortas las ISR de la UART (pruebas 1.10 y 1.11) para el jitter de Q.
- [ ] Detectar saturación (códigos cerca de 0 o 4095). Hoy no hay máquina de estados (ERROR/ALARM del enunciado): lo avisa la interfaz del PC.
- [ ] Revisar en el depurador que `adcLineasPerdidas` y `adcParesPerdidos` queden en 0 a 500 líneas/s.

**Validación:** histograma (§8), SINAD (§6), periodo de muestreo y duración de la ISR con RC2 (pruebas 4.5, 4.13 y 4.16) y jitter de Q (4.17).

## 15. Referencias

- DS40001985C: cap. 32 (ADC2), p.501–540; AD01–AD23, p.632–633; FVR, cap. 30, p.491; Timer0, cap. 18, p.260–261 y p.267; requisitos de adquisición y Ecuación 32-1, p.506–507; fuentes de disparo `ADACT`, p.538; `ADREF`, p.537.
- Documento de cambios de op-amps (`CAMBIOS_OPAMPS_Y_AMPLITUDES_2026-10-16.md`, en el proyecto de Claude): modelos y `GBW` de TS924A, MCP604 y TLV9064. Datasheet del MCP604: `docs/MCP601-2.PDF` (DS21314G). Los de TS924A y TLV9064 no están en `docs/`.
- Errata 80000799, punto 1.1.2 (ADCC con doble muestra): **no aplica**, porque el código usa el modo básico.
- TI, *Fundamentals of Precision ADC Noise Analysis* (SLYY192A, Rev. A, marzo 2025, 83 páginas): Cap. 1 (págs. 4–22), Cap. 4 (46–57), Cap. 5 (58–64), Cap. 6 (65–79). Citas usadas, ya comprobadas en el PDF: Ec. 2 (p.7), Fig. 7a (p.10), Tablas 2 y 3 (págs. 13–15), Ec. 26 (p.47), Ec. 27 (p.49), Tabla 21 (p.56), Ec. 28 (p.59).
- Preinforme de la Práctica 2: filtros de 20 Hz, `Vref2`, ecuaciones (5) y (8).
- `docs/Especificacion_Tecnica_Practica3_Microcontroladores.md`: muestreo por Timer, Nyquist y filtro antialiasing.
