# [MACRO] Criterios de diseño del filtro activo del PWM (I0 / Q0)

> **Para Macro, no para Micro 3** (cambio de enfoque del profesor, 2026-11-04). Este filtro (fc ≈ 30 Hz, pensado para niveles de auto-cero I0/Q0 que casi no cambian) es del auto-cero de Macro (`docs/macro_autocero_pwm.md`). En Micro 3 el filtro tiene que dejar pasar la señal muestreada: ver `docs/especificacion_pwm.md` §6 (fc ≈ 100 Hz). El procedimiento de WEBENCH de este documento sirve igual cambiando la frecuencia de corte.

Proyecto Micro 3 · Fase 2a · Fecha: 2026-11-04 · Estado: **PROPUESTA, sin aprobar**

> **Para qué sirve:** decidir a qué frecuencia debe cortar el filtro y qué más se le exige, antes de diseñarlo en WEBENCH. Reemplaza al filtro pasivo de 2 etapas que proponía `docs/especificacion_pwm.md` §2 (decisión D-5, que estaba aplazada).
>
> **Etiquetas:** **[VERIFICADO]** = sale de un documento de `docs/` o de un datasheet. **[CÁLCULO]** = cuenta propia con un script (modelo ideal de filtro, sin tolerancias ni op-amp real). **[PROPUESTA]** = decisión por aprobar.

---

## 1. Qué hace el filtro y qué se le pide

El PWM (PWM6 → I0, PWM7 → Q0) es una onda cuadrada de 0 a 3,7 V a **15,625 kHz** [VERIFICADO: `especificacion_pwm.md` §3]. Lo que interesa es solo su **promedio** (`V = 3,7 V · duty`). El filtro pasabajos quita la onda cuadrada y deja ese nivel DC. Después de él va la etapa de resta con ganancia ×10 [VERIFICADO: preinforme, ec. 5], así que **todo lo que el filtro deje pasar se multiplica por 10**.

Se le piden tres cosas, y de ellas salen todos los números:

| # | Requisito | Qué fija |
|---|---|---|
| R1 | **Rizado** de 15,625 kHz en I0/Q0 lo bastante pequeño | frecuencia de corte **máxima** |
| R2 | **Rapidez**: que el nivel se asiente a tiempo tras cada cambio de duty | frecuencia de corte **mínima** |
| R3 | **DC fiel**: ganancia 1, sin recortar entre 0 y 3,7 V | alimentación, tipo de op-amp y topología |

El filtro **no** tiene que dejar pasar ninguna señal útil: I0/Q0 son niveles de ajuste que cambian solo al calibrar. Por eso conviene la frecuencia de corte más baja que el tiempo de calibración permita.

## 2. De dónde sale la frecuencia

### 2.1 Límite superior: el rizado (R1)

Criterio: el rizado en I0 multiplicado por 10 debe ser ≤ 1 cuenta del ADC. 1 cuenta = 3,7 V / 4096 = 0,903 mV, así que el rizado en I0 debe ser **≤ 0,09 mV pico a pico** [PROPUESTA; es el cambio CP-2 del plan, que corrige el «0,9 mV» de la prueba 3.5 porque olvidaba el ×10].

Como el ADC toma una muestra por par sin promediar (decisión del 2026-11-03), cada muestra ve el rizado instantáneo: por eso el criterio relevante es el de 1 cuenta, no uno más laxo. El peor caso es duty ≈ 50 %: la onda cuadrada tiene entonces su armónico fundamental de 2,36 V de pico (4,72 V pp). Atenuación que hace falta a 15,625 kHz: 0,09 mV / 4,72 V = 1,9·10⁻⁵ = **−94 dB** (−100 dB si se quiere margen de 0,5 cuenta).

Un filtro de 2.º orden atenúa `(fc / 15 625)²` a 15,625 kHz, así que:

> **rizado en I0 [mV pp] ≈ 2,0·10⁻⁵ · fc²** (fc en Hz) [CÁLCULO, simulando el PWM completo, todos los armónicos]

| fc (−3 dB) | Rizado en I0 (mV pp) | En la salida ×10 | Atenuación a 15,625 kHz | Tiempo de establecimiento (±½ paso de PWM) |
|---|---|---|---|---|
| 20 Hz | 0,008 | 0,09 cuentas | −115 dB | 82 ms |
| **30 Hz** | **0,018** | **0,2 cuentas** | **−109 dB** | **55 ms** |
| 40 Hz | 0,033 | 0,4 cuentas | −104 dB | 41 ms |
| 50 Hz | 0,051 | 0,6 cuentas | −100 dB | 33 ms |
| 60 Hz | 0,073 | 0,8 cuentas | −97 dB | 27 ms |
| 80 Hz | 0,130 | 1,4 cuentas | −92 dB | 20 ms |
| 100 Hz | 0,203 | 2,2 cuentas | −88 dB | 16 ms |

(Respuesta Butterworth. Con Bessel o amortiguamiento crítico el rizado es el mismo a igual frecuencia natural y el asentamiento mejora un 10–15 %.)

Con la meta de 1 cuenta, **fc ≤ 67 Hz**; con margen de media cuenta, **fc ≤ 47 Hz**.

### 2.2 Límite inferior: el tiempo de asentamiento (R2)

Criterio: tras un cambio de duty, I0 debe quedar a **±½ paso de PWM** (1,8 mV) de su valor final, que es lo mínimo para que el ADC vea el cambio de un paso. Con respuesta Butterworth:

> **t_asentamiento ≈ 1,64 / fc** (segundos) [CÁLCULO]

El lazo de auto-cero espera ese tiempo tras cada paso. Una bisección de 10 pasos por canal dura entonces `10 · t_asentamiento` (los canales I y Q son independientes y pueden ajustarse a la vez). Ojo: el filtro de 20 Hz de I_LPF **no** está en este camino, porque I0 entra directo a la etapa de resta; solo cuenta el filtro del PWM.

El tiempo total de calibración aceptable lo tiene que fijar la persona o el profesor (la prueba 3.6 lo deja pendiente). Con **t_cal ≤ 1 s** el límite es `fc ≥ 16 Hz`.

### 2.3 Valor propuesto

> **fc = 30 Hz, Butterworth de 2.º orden** [PROPUESTA]. Rango admisible: 20 a 50 Hz.

Motivos: queda 5 veces por debajo del límite de rizado (margen para el op-amp real, la tolerancia de los componentes y el desacople de la fuente), asienta en ≈ 55 ms (10 pasos ≈ 0,6 s por canal) y con tolerancias de R 1 % y C 5 % la fc se mueve ≈ ±6 % (el asentamiento pasa de 55 a 58 ms en el peor caso), dentro del rango. Es más conservador que el filtro pasivo de 2 etapas de la especificación del PWM (0,10 mV pp y 45 ms), que era el punto de partida.

## 3. Hoja de datos de entrada para WEBENCH

Los campos que pide un diseño de WEBENCH Filter Designer (según un informe de ejemplo de TI: tipo, respuesta, orden, topología, ganancia, frecuencia de paso, frecuencia y atenuación de rechazo, alimentación, op-amp, tolerancias y capacitor semilla) y el valor que propongo para cada uno. Los nombres exactos pueden variar según la versión de la herramienta.

| Campo de WEBENCH | Valor | Notas |
|---|---|---|
| Tipo de filtro | Pasabajos | |
| Respuesta | **Butterworth** | Q = 0,707; ≈ 4 % de sobreimpulso, aceptable. Si se prefiere cero sobreimpulso: Bessel, pero entonces se especifica la frecuencia natural, no la de −3 dB |
| Orden | **2** (una sola etapa) | decisión de la persona |
| Topología | **Sallen-Key** de ganancia unidad | pocas piezas y ganancia exacta de 1. Alternativa: MFB (inversora), ver decisión F-2 |
| Ganancia | **1 V/V (0 dB)** | sin resistencias de ganancia (ver F-1 si se quiere atenuar) |
| Frecuencia de paso | **30 Hz** (−3 dB) | comprobar en la gráfica que −3 dB cae en 30 Hz |
| Frecuencia de rechazo | **15,625 kHz** | es el PWM; si cambia `T2PR` o el prescaler, cambia esto |
| Atenuación de rechazo | **≥ 60 dB** | con orden 1 solo se logran 54 dB a 15,625 kHz con fc = 30 Hz, así que 60 dB obliga a orden 2 y la herramienta no sube de ahí. El 2.º orden real da ≈ 109 dB. **Comprobar que el resultado sea de orden 2** |
| Alimentación | **Simple, 3,7 V / 0 V** | misma fuente que el PIC y PCB2 |
| Op-amp | rail-to-rail **entrada y salida**, 3,7 V, GBW ≥ 1 MHz | recomendado **TLV9064** (TI, SOIC-14, 1,8–5,5 V, 10 MHz [VERIFICADO: `CAMBIOS_OPAMPS_Y_AMPLITUDES_2026-10-16.md` §1.1]). Ver §4 |
| Tolerancia de resistencias | **1 % (E96)** | |
| Tolerancia de capacitores | **5 % (E24)**, C0G/NP0 si se consigue; X7R acepta ±10 % | |
| Capacitor semilla | **100 nF** | deja R en el orden de 30–40 kΩ |

**Comprobación de lo que devuelva WEBENCH** (Sallen-Key unidad, R1 = R2 = R, Butterworth, fc = 30 Hz) [CÁLCULO]: con C2 = 100 nF (a tierra), **R ≈ 37,4 kΩ y C1 ≈ 200 nF** (el capacitor de realimentación). Si WEBENCH entrega algo muy distinto (por ejemplo R de 1 MΩ o C de 1 nF), revisar el campo de frecuencia o la respuesta.

Dos cuidados con la salida de la herramienta:
- **Sin bias a Vcc/2.** El filtro debe pasar DC desde 0 V hasta 3,7 V, referido a tierra. Si el diseño de «alimentación simple» trae una polarización a la mitad de la alimentación, se reemplaza por tierra.
- **Sin recorte cerca de los rieles.** Un op-amp rail-to-rail no llega exactamente a 0 ni a 3,7 V; limitar el duty a ≈ 3 %–97 % (0,11–3,59 V) cubre sobrado el rango útil (I0 ≈ 1,9 V, Q0 ≈ 1,8 V en el preinforme; 1,6 a 2,3 V en la simulación).

## 3.1 Primer resultado de WEBENCH (2026-11-04) y valores corregidos en kΩ

Primer diseño devuelto: Sallen-Key de ganancia 1 con R1 = 82 Ω, R2 = 150 Ω, C1 = 1 µF (a tierra), C2 = 2,2 µF (realimentación) y OPA2186. Revisado con las ecuaciones del Sallen-Key (`f₀ = 1/(2π√(R1·R2·C1·C2))`, `Q = √(R1·R2·C1·C2) / (C1·(R1+R2))`, con C1 el capacitor a tierra) [CÁLCULO]:

| Punto | Resultado | ¿Cumple? |
|---|---|---|
| Forma de la respuesta | Q = 0,709: Butterworth de 2.º orden, ganancia 1 | sí |
| Frecuencia de corte | **f₀ = 967 Hz**, no 30 Hz | **no** |
| Rizado a 15,625 kHz | −48 dB: **17,5 mV pp en I0**, ≈ 175 mV en la salida (≈ 194 cuentas del ADC) frente al máximo de 0,09 mV | **no** |
| Corriente en los flancos del PWM | 3,7 V / 82 Ω ≈ **45 mA**, más de lo que un pin del PIC debe entregar (el límite de corriente de salida del pin se confirma en el datasheet, sección de características eléctricas) | **no** |
| Op-amp OPA2186 | alimentación **4,5 V a 24 V** [según la página de producto de TI]; PCB2 trabaja a 3,7 V | **no** |
| Op-amp OPA2186, ancho de banda | 0,75 MHz, por debajo del GBW ≥ 1 MHz de §4 | no |

Conclusión: la **estructura** es la correcta, pero la escala (frecuencia, resistencias) y el op-amp no. Escalando impedancias (R × k, C ÷ k) la forma de la respuesta no cambia, solo la frecuencia y la corriente.

**Valores propuestos** (Sallen-Key de ganancia 1, op-amp TLV9062 o TLV9064, a 3,7 V) [CÁLCULO]:

| Pieza | Valor | Serie |
|---|---|---|
| R1 (entrada, desde el PWM o el RC previo) | **26,1 kΩ** | E96, 1 % |
| R2 (hacia la entrada +) | **48,7 kΩ** | E96, 1 % |
| C1 (de la entrada + a tierra) | **100 nF** | E24, 5 % |
| C2 (de la unión R1/R2 a la salida) | **220 nF** (E12) | 5 % |

| Resultado | Nominal | Monte Carlo (R 1 %, C 5 %, 60 corridas) |
|---|---|---|
| f₀ | 30,1 Hz | 28,9 a 31,3 Hz |
| Q | 0,707 | 0,677 a 0,740 |
| Atenuación a 15,625 kHz | −108,6 dB | – |
| Rizado en I0 (duty 50 %) | 0,017 mV pp (0,17 mV, 0,2 cuentas en la salida) | máx. 0,018 mV pp |
| Asentamiento a ±½ paso de PWM | 54 ms | máx. 57 ms |
| Sobreimpulso | 4,3 % | – |
| Corriente pico en el flanco | 3,7 V / 26,1 kΩ = 0,14 mA | – |

Alternativa con los capacitores que dio WEBENCH (1 µF y 2,2 µF): R1 = 2,67 kΩ y R2 = 4,87 kΩ dan f₀ = 29,8 Hz y Q = 0,709, con 1,4 mA pico en los flancos. Funciona igual, pero necesita capacitores de 1 y 2,2 µF (más grandes y, en cerámica, más sensibles al voltaje de polarización).

Reglas para volver a pasarlo por WEBENCH: poner **TLV9062/TLV9064** (rail-to-rail, 1,8 a 5,5 V; TLV9064 ya está evaluado en `CAMBIOS_OPAMPS_Y_AMPLITUDES_2026-10-16.md`), **capacitor semilla 100 nF** y revisar que la f₀ de la hoja de resultados sea 30 Hz. Si la herramienta no deja fijar fp = 30 Hz, usar los valores de arriba directamente.

## 4. Criterios del op-amp y de las piezas

| Criterio | Valor | Por qué |
|---|---|---|
| Entrada y salida **rail-to-rail** | obligatorio | En Sallen-Key la entrada (+) recorre 0–3,7 V. El MCP604 solo admite entradas hasta VDD − 1,2 V = 2,5 V [VERIFICADO: `CAMBIOS_OPAMPS...` §1.1] y **no sirve** para esta topología con rango completo |
| GBW | ≥ 1 MHz | a 15,625 kHz el lazo debe conservar ganancia para que el rizado no se cuele. TLV9064: 10 MHz |
| Offset y corriente de polarización | no críticos | el lazo de auto-cero los absorbe (el ADC cierra el lazo). Solo debe ser estable en el tiempo |
| Ruido | no crítico | R = 37 kΩ da ≈ 0,17 µV rms en 30 Hz; ×10 = 1,7 µV, muy por debajo de 0,9 mV por cuenta [CÁLCULO] |
| Slew rate | no crítico | solo pasa un rizado de décimas de mV |
| Cantidad | 1 op-amp por canal (2 en total) | un TLV9064 (cuádruple) alcanza para I0 y Q0 y deja 2 libres |
| Desacople | 100 nF cerca de la alimentación | como el resto de la placa |

## 5. Riesgo conocido y mitigación

El modelo ideal da −109 dB a 15,625 kHz, pero **un op-amp real no mantiene esa atenuación**: a esa frecuencia su ganancia de lazo es finita (TLV9064: 10 MHz / 15,6 kHz ≈ 640, unos 56 dB) y parte de la onda cuadrada pasa por los capacitores del filtro hasta la salida. El margen real contra el requisito de −94 dB es solo de ≈ 15 dB, y es lo primero que habría que comprobar [CÁLCULO + riesgo por verificar en simulación].

**Mitigación barata [PROPUESTA]:** un RC pasivo de 1.er orden **delante** del filtro activo: **2,2 kΩ + 100 nF** a tierra por canal (fc ≈ 720 Hz). Atenúa 27 dB a 15,625 kHz, no cambia la ganancia DC (el op-amp no toma corriente de entrada) y no afecta al asentamiento (constante de 0,22 ms). Con él, al filtro activo solo le piden ≈ −68 a −74 dB en lugar de −94 a −100 dB, y el margen pasa de 15 dB a ≈ 35–41 dB. Cuesta 1 R y 1 C por canal, y la carga sobre el pin del PIC es pequeña (≤ 3,7 V / 2,2 kΩ ≈ 1,7 mA en los flancos). No entra en WEBENCH: se agrega aparte.

## 6. Cómo comprobar el diseño

**En LTspice, antes de pedir piezas** (ya usan LTspice para PCB1+PCB2):
1. Fuente de pulsos 0/3,7 V, 15,625 kHz, duty 50 % (y también 25 % y 10 %) hacia la entrada del filtro, con el modelo real del op-amp.
2. Medir el rizado pico a pico de la salida después de 300 ms. **Aceptación: ≤ 0,045 mV pp** (media cuenta) con componentes nominales.
3. Repetir con 50 corridas Monte Carlo (R 1 %, C 5 %): ≤ 0,09 mV pp en todas.
4. Escalón de 25 % a 75 % de duty: **asentamiento a ±1,8 mV en ≤ 80 ms** (peor caso con tolerancias; nominal ≈ 55 ms) y sobreimpulso ≤ 5 %.
5. Duty 3 % y 97 %: la salida sigue a `3,7 V · duty` sin recortar.

**En la placa**, con las pruebas del Módulo 3 de `docs/protocolos_prueba.md`: 3.3 y 3.4 (DC y linealidad), 3.5 (rizado, con el criterio corregido de 0,09 mV pp medido **en I0/Q0**, antes de la resta), 3.6 (asentamiento ≤ 80 ms) y 3.7 (un paso de duty se distingue en el multímetro).

## 7. Decisiones abiertas

| ID | Decisión | Opciones | Mi recomendación |
|---|---|---|---|
| F-1 | Ganancia del filtro | **A.** Ganancia 1 (resolución de 10 bits directos, ya aprobada: paso de 3,6 mV en I0 y 36 mV en la salida). **B.** Atenuar en el filtro: ganancia −0,25 referida a Vref2 deja el paso en 0,9 mV en I0 y 9 mV en la salida (±5 cuentas de residuo en lugar de ±20), con rango de I0 de 1,39 a 2,31 V. Exige MFB inversora con bias en Vref2 (1,85 V) e invertir el duty (`POL = 1` o por software) | **A**, para no reabrir la decisión de 10 bits. B se puede reconsiderar si el residuo de ±20 cuentas resulta grande en las pruebas; hay que decidirlo **antes** de montar, porque cambia la topología |
| F-2 | Sallen-Key o MFB | SK de ganancia 1 (ganancia exacta, pero entrada hasta el riel y más sensible al rizado que pasa por los capacitores); MFB (el modo común queda en el bias) | SK con el RC previo de §5 |
| F-3 | RC pasivo previo de 2,2 kΩ + 100 nF | con él o sin él | con él, mientras la simulación con el op-amp real no demuestre margen suficiente sin él |
| F-4 | fc = 30 Hz | 20 a 50 Hz | 30 Hz |
| F-5 | Tiempo máximo de calibración | ≤ 1 s propuesto | confirmar con el profesor (prueba 3.6) |
| F-6 | Op-amp | TLV9062/TLV9064 (TI, R-R, 1,8 a 5,5 V); TS924A (ST, R-R, 4 MHz); **MCP604 y OPA2186 descartados** (el primero por su rango de entrada, el segundo por pedir 4,5 V de alimentación mínima) | TLV9062 (doble, una por canal) o TLV9064 |
| F-7 | Dónde entra la salida del filtro en PCB2 y qué pasa con los buffers de I0/Q0 | el filtro puede reemplazar al buffer o ir antes de él; los trimmers se desconectan | revisar el esquemático (`hardware/`); pendiente |

## 8. Lo que no verifiqué

- Que WEBENCH acepte exactamente esos campos y que ofrezca el TLV9064 en la versión actual de la herramienta; los nombres salen de un informe de ejemplo de TI, no de la interfaz.
- El rizado y el asentamiento son cálculos con un filtro ideal. El rizado real depende del op-amp, de los capacitores y de la distribución de la placa; se valida en LTspice y en la placa.
- Las corrientes de polarización del TLV9064 con resistencias de decenas de kΩ (no revisadas; no son críticas porque el lazo de auto-cero las compensa).
- El nodo exacto donde se conecta el PWM en PCB2 (F-7).
