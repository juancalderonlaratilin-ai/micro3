# [MACRO] Guía: del valor digital al nivel analógico (PWM + filtro activo)

> **Para Macro, no para Micro 3** (cambio de enfoque del profesor, 2026-11-04). Este filtro (fc ≈ 30 Hz, pensado para niveles de auto-cero I0/Q0 que casi no cambian) es del auto-cero de Macro (`docs/macro_autocero_pwm.md`). En Micro 3 el filtro tiene que dejar pasar la señal muestreada: ver `docs/especificacion_pwm.md` §6 (fc ≈ 100 Hz). El procedimiento de WEBENCH de este documento sirve igual cambiando la frecuencia de corte.

Proyecto Micro 3 · Fase 2a · Fecha: 2026-11-04 · Estado: **PROPUESTA, sin aprobar**

> **Para qué sirve:** explicar cómo un número digital (el duty del PWM) vuelve a ser un voltaje analógico, con qué valores de componentes, y cómo se diseña y se comprueba. Complementa a `docs/criterios_filtro_pwm.md` (los criterios y las cuentas) y a `docs/especificacion_pwm.md` (pines, registros, eventos y tareas del PWM).
>
> **Etiquetas:** **[VERIFICADO]** = sale de un documento de `docs/` o de un datasheet. **[CÁLCULO]** = cuenta propia con un script, con un modelo simple (ver §2). **[PROPUESTA]** = decisión por aprobar.

---

## 0. Qué entendí y qué hay que confirmar

Hay dos formas de usar «la señal digitalizada que maneja el PWM y vuelve a ser analógica»:

| Uso | Qué es | Filtro |
|---|---|---|
| **A. Offsets I0 / Q0** (lo que pide el enunciado de la práctica) | Un valor digital que casi no cambia (resultado del auto-cero o del ajuste por serial) sale como un **nivel DC** que sustituye a los trimmers [VERIFICADO: `Especificacion_Tecnica_Practica3...md` §3.1] | pasabajos de **30 Hz** (§1 a §6) |
| **B. Reproducir la señal I/Q completa** | Cada muestra del ADC (1 kHz) se convierte otra vez en voltaje para verla en un osciloscopio, una señal que **sí varía** (hasta ≈ 20 Hz) | pasabajos de **≈ 100 Hz** (§7) |

Esta guía desarrolla **A** por completo, porque es lo que está en el enunciado y en el plan (T-06 a T-08). **B** queda como variante con sus valores (§7). **Confirma cuál de las dos quieres:** B no es lo mismo que A, necesita otro filtro y otros pines.

## 1. La cadena completa

```
valor digital d (0 a 1023)
   │  PWMxDCH = d >> 2 ; PWMxDCL = (d & 3) << 6
   ▼
pin del PIC (RA2 = PWM6 → I0 ; RA4 = PWM7 → Q0)
   onda cuadrada 0 / 3,7 V a 15,625 kHz, ancho = d / 1024
   ▼
 Rpre 2,2 kΩ ──┬── R1 23,7 kΩ ──┬── R2 48,7 kΩ ──┬──► (+)  TLV9062
                │                │                │              │
              Cpre 100 nF        C2 220 nF        C1 100 nF     (−) unido a la salida
                │                └──────────────────────────────┐
               GND                                  GND (C1)    │
                                                                ▼
                                                  I0 (o Q0) → buffer → etapa de resta ×10
```

- **Rpre + Cpre**: RC pasivo de primer orden (fc ≈ 720 Hz). Suaviza los flancos del PWM.
- **R1, R2, C1, C2, op-amp**: filtro **Sallen-Key** de ganancia 1 y 2.º orden (Butterworth, fc = 30 Hz). C2 va de la unión R1/R2 a la **salida**; C1 va de la entrada (+) a **tierra**.
- Un canal por cada PWM: dos copias idénticas (una para I0, otra para Q0). Un TLV9062 (doble) alcanza para las dos.

Por qué funciona: el promedio de la onda cuadrada es `3,7 V · d / 1024`; el filtro deja pasar ese promedio (DC) y quita la onda de 15,625 kHz [VERIFICADO: `especificacion_pwm.md` §1].

## 2. ¿El filtro cumple lo que queríamos?

Revisión del diseño que dio WEBENCH, con los números de `criterios_filtro_pwm.md` y una simulación del filtro con **op-amp real** (ganancia finita y resistencia de salida), no ideal.

| Requisito | Meta | Filtro de WEBENCH solo, ya a 30 Hz en kΩ | Con el RC previo (§3) |
|---|---|---|---|
| DC fiel, ganancia 1 | 1,000 | 1,000 | 1,000 |
| Rizado de 15,625 kHz en I0 | **≤ 0,09 mV pp** (1 cuenta del ADC a la salida) | ideal: 0,017 mV ✓. **Con op-amp real: 2,8 mV (Ro = 100 Ω) a 28 mV (Ro = 1 kΩ) ✗** = 31 a 306 cuentas | **0,002 a 0,015 mV ✓** = 0,03 a 0,17 cuentas |
| Asentamiento a ±½ paso de PWM | ≤ 80 ms | 54 ms ✓ | 54 ms ✓ |
| Corriente en los flancos del PWM | ≲ 2 mA | 0,14 mA ✓ | 1,7 mA ✓ |
| Alimentación del op-amp | 3,7 V | TLV9062/9064 ✓ (el OPA2186 de la primera propuesta pide 4,5 V mínimo ✗) | igual |

**Conclusión: el Sallen-Key de WEBENCH no basta por sí solo; el RC previo es obligatorio.** El filtro ideal atenúa 109 dB a 15,625 kHz, pero en un Sallen-Key real el lazo del op-amp pierde ganancia con la frecuencia y su salida deja de ser «tierra de señal»: los flancos rápidos del PWM se cuelan por C2 hasta la salida, y ese rizado crece con la frecuencia. El RC previo redondea los flancos antes de que lleguen al op-amp.

Limitaciones de esta conclusión [CÁLCULO, por validar]: el modelo usa una resistencia de salida constante y un solo polo en el op-amp. No conozco la resistencia de salida real del TLV9062; por eso el rango 100 Ω a 1 kΩ. En todos los casos el resultado sin RC previo falla y con RC previo cumple, así que la conclusión no depende del valor exacto. Se confirma en LTspice con el macromodelo del fabricante (§6).

## 3. Valores (por canal)

| Pieza | Valor | Serie y tipo | Para qué |
|---|---|---|---|
| Rpre | **2,2 kΩ** | E96, 1 % | RC previo: limita la corriente en los flancos y suaviza |
| Cpre | **100 nF** | C0G o X7R | RC previo (fc ≈ 720 Hz) |
| R1 | **23,7 kΩ** | E96, 1 % | entrada del Sallen-Key. Rpre + R1 = 25,9 kΩ ≈ 26,1 kΩ del diseño base |
| R2 | **48,7 kΩ** | E96, 1 % | entre las dos uniones |
| C1 | **100 nF** | E24, 5 % (C0G si se consigue) | de la entrada (+) a tierra |
| C2 | **220 nF** | E12, 5 % | de la unión R1/R2 a la salida |
| Op-amp | **TLV9062** (1/2 por canal) o **TLV9064** | rail-to-rail, 1,8 a 5,5 V, GBW 10 MHz | ver `CAMBIOS_OPAMPS...md` §1.1 |
| Desacople | 100 nF en la alimentación del op-amp | | |

Por qué R1 es 23,7 kΩ y no 26,1 kΩ: a baja frecuencia Rpre queda en serie con R1, así que el diseño base de 26,1 kΩ se reparte entre las dos para no mover la frecuencia de corte.

Resultado esperado [CÁLCULO, op-amp ideal para la respuesta en frecuencia]: **f(−3 dB) = 30,0 Hz**, respuesta plana sin pico, ganancia DC 0,99999. Asentamiento ≈ `1,64 / 30` = 54 ms. Con resistencias del 1 % y capacitores del 5 %, la frecuencia se mueve ≈ ±6 % (≈ 29 a 31 Hz).

Se puede cambiar Rpre/Cpre sin desarmar lo demás (siempre ajustando R1 para que Rpre + R1 ≈ 26,1 kΩ): 4,7 kΩ + 100 nF o 2,2 kΩ + 220 nF dan menos rizado todavía (0,002 mV pp), 1 kΩ + 100 nF da 0,011 mV pp (todavía cumple) pero con más corriente en los flancos (3,7 mA).

## 4. Cómo se pasa de número a voltaje (y de vuelta)

**Del duty al voltaje:** `V = 3,7 V · d / 1024`. **Del voltaje al duty:** `d = redondear(V · 1024 / 3,7)`.

| Quiero | d | Registros (`DCH`, `DCL`) |
|---|---|---|
| 0 V | 0 | 0, 0 |
| 1,77 V (Q0 del preinforme) | 490 | 122, 128 |
| 1,85 V (centro) | 512 | 128, 0 |
| 1,93 V (I0 del preinforme) | 534 | 133, 128 |
| 3,696 V (máximo) | 1023 | 255, 192 |

`DCH = d >> 2` (8 bits altos) y `DCL = (d & 3) << 6` (2 bits bajos, en las posiciones 7 y 6) [VERIFICADO: datasheet §24.4]. Los nombres de los registros en C se comprueban en `pic16f18426.h` antes de usarlos.

Rango útil [PROPUESTA]: duty entre ≈ 3 % y 97 % (`d` de 31 a 993, de 0,11 a 3,59 V), para no pegarse a los rieles del op-amp. Cubre de sobra los valores que aparecen en el preinforme (1,6 a 2,3 V).

**Cómo corregir I0/Q0 con la lectura del ADC** [CÁLCULO]: la placa cumple `VI_OUT = Vref2 + 10·(I_LPF − I0)` [VERIFICADO: preinforme, ec. 5], de modo que:

- 1 paso de PWM = 3,61 mV en I0 = 36,1 mV en VI_OUT = **40 cuentas del ADC**.
- Sea `e = lectura del ADC − 2048` (error respecto al centro). Corrección: **`d_nuevo = d + redondear(e / 40)`**. Si el ADC lee más de 2048, **sube** el duty (subir I0 baja VI_OUT).
- Eso vale mientras la salida no esté saturada. Si la lectura está pegada a 0 o a 4095, el error real es mayor: dar pasos grandes o usar bisección.
- El mejor resultado posible es ±½ paso = **±20 cuentas** (decisión de 10 bits directos).
- Después de cada cambio de duty, **esperar ≥ 60 ms** (el asentamiento es de 54 ms) antes de leer el ADC. Sin esperas bloqueantes: con un temporizador, como el resto del firmware.

## 5. Cómo diseñarlo en WEBENCH (paso a paso)

1. Tipo **pasabajos**, respuesta **Butterworth**, orden **2**, topología **Sallen-Key**, ganancia **1 V/V**.
2. Frecuencia de paso **30 Hz**. La frecuencia de rechazo la limita la herramienta a 10 veces la de paso: dejar **300 Hz** y pedir **35 dB** (fuerza el 2.º orden).
3. Alimentación **simple 3,7 V**. **Op-amp: TLV9062 o TLV9064** (no el OPA2186, que pide 4,5 V mínimo).
4. Tolerancias **1 % (E96)** en resistencias y **5 % (E24)** en capacitores. Capacitor semilla **100 nF**.
5. Revisar en la hoja de resultados: orden 2, una sola etapa, **f₀ = 30 Hz**, Q ≈ 0,707. Si WEBENCH devuelve algo distinto, usar los valores de §3.
6. **Agregar a mano el RC previo** (WEBENCH no lo modela): Rpre 2,2 kΩ y Cpre 100 nF delante de R1, y **restar 2,2 kΩ a R1** (23,7 kΩ).
7. La atenuación a 15,625 kHz **no se puede pedir** en WEBENCH (§2 de `criterios_filtro_pwm.md`): se comprueba en LTspice.

## 6. Cómo comprobarlo

**En LTspice** (con el macromodelo del TLV9062 o TLV9064 de TI, que se descarga del sitio del fabricante; no pude descargarlo desde aquí):

1. Fuente de pulsos 0/3,7 V, 15,625 kHz, tiempo de subida y bajada de 10 ns, duty 50 % (repetir con 25 % y 10 %) hacia Rpre.
2. Armar la cadena de §1. Simular 300 ms y medir el rizado pico a pico de la salida en los últimos 20 ms. **Aceptación: ≤ 0,045 mV pp** (media cuenta).
3. Hacer la misma medición **sin** Rpre/Cpre para ver el efecto: debería salir del orden de milivoltios, como en la §2.
4. Escalón de duty 25 % → 75 %: asentamiento a ±1,8 mV en ≤ 80 ms, sobreimpulso ≤ 5 %.
5. Monte Carlo (R 1 %, C 5 %): rizado ≤ 0,09 mV pp en todas.

**En la placa**, con las pruebas del Módulo 3 de `docs/protocolos_prueba.md`: 3.3 y 3.4 (DC y linealidad), 3.5 (rizado, medido **en I0/Q0 antes de la resta**, criterio 0,09 mV pp), 3.6 (asentamiento ≤ 80 ms) y 3.7 (un paso de duty se distingue en el multímetro). Para el rizado usar el osciloscopio con acoplamiento AC, 20 MHz de ancho de banda limitado y la punta de tierra corta.

## 7. Variante B: reproducir I y Q en analógico (si es lo que se quiere)

Si cada muestra del ADC (1 kHz) debe volver a ser voltaje (`d = lectura >> 2`, actualizando `PWMxDC` en cada muestra), el filtro de 30 Hz no sirve: a 20 Hz atenúa 0,78 dB (8,6 % de error en amplitud) y la banda con error ≤ 0,5 dB llega solo a 17,7 Hz. Se necesita una frecuencia de corte más alta, sin perder el rechazo del PWM.

| Criterio | Valor |
|---|---|
| Banda de la señal | hasta ≈ 20 Hz (los filtros de PCB2 son de 19,4 Hz, [VERIFICADO: preinforme §1.4]) |
| Error de amplitud en 20 Hz | ≤ 0,1 dB → `fc ≥ 51 Hz` |
| Rizado del PWM en la salida | ≤ 1 paso de PWM = 3,6 mV pp (aquí no hay etapa ×10) |
| Imágenes del muestreo a 1 kHz (alrededor de 980 Hz) | ≥ 60 dB por debajo de la señal |

Propuesta [CÁLCULO]: **Butterworth de 2.º orden, fc ≈ 100 Hz**, mismos capacitores (C1 = 100 nF, C2 = 220 nF) y el mismo RC previo en versión más ligera:

| Pieza | Valor |
|---|---|
| Rpre / Cpre | 1 kΩ / 100 nF |
| R1 | 6,81 kΩ (E96) (Rpre + R1 = 7,81 kΩ) |
| R2 | 14,7 kΩ (E96) |
| C1 / C2 | 100 nF / 220 nF |

Resultado: f(−3 dB) ≈ 99 Hz; error de ganancia a 20 Hz de −0,009 dB; imagen de 980 Hz atenuada ≈ 73 dB (la mantenida por el muestreo, −34 dB, más el filtro, −40 dB); rizado del PWM con op-amp real ≈ 0,06 mV pp (0,02 pasos); retardo de grupo ≈ 2,3 ms. Sin el RC previo el rizado sería de 28 mV pp, así que **también aquí es obligatorio**.

Lo que cambia además del filtro:
- **Resolución:** el ADC es de 12 bits y el PWM de 10; se pierden 2 bits (paso de 3,6 mV, ≈ 62 dB de relación señal/ruido ideal a escala completa).
- **Pines:** PWM6 y PWM7 ya están asignados a I0 y Q0. Reproducir I y Q necesita **dos salidas PWM más** (los módulos CCP en modo PWM también sirven, sobre el mismo Timer2) y el presupuesto de pines solo deja libre RC2, más RA5 (LED) y RC3 (pulsador) si se renuncia a ellos [VERIFICADO: `especificacion_diseno_uart.md` §3.1].
- **Sincronía:** actualizar el duty justo después de cada conversión del ADC; el doble buffer del PWM lo aplica en el siguiente periodo de 64 µs.

## 8. Decisiones abiertas

| ID | Decisión | Mi recomendación |
|---|---|---|
| G-1 | ¿Uso A (offsets) o B (reproducir I/Q)? | **A**, que es lo del enunciado. B solo si se pide expresamente, por pines y resolución |
| G-2 | RC previo | obligatorio (§2); 2,2 kΩ + 100 nF |
| G-3 | Op-amp | TLV9062 (doble) o TLV9064; descartados MCP604 y OPA2186 |
| G-4 | Valor exacto de fc | 30 Hz (A) |
| G-5 | Dónde entra el filtro en PCB2 y qué pasa con los buffers de I0/Q0 | pendiente: revisar el esquemático (`hardware/`); los trimmers se desconectan |

## 9. Lo que no verifiqué

- La resistencia de salida real del TLV9062 (el rizado se simuló con 100 Ω a 1 kΩ) y su macromodelo: la validación definitiva es en LTspice y en la placa.
- Que el modelo de PWM (flancos ideales hasta ≈ 3 MHz de contenido espectral) refleje los flancos reales del pin del PIC; los flancos reales más lentos ayudan, y el RC previo los redondea de todos modos.
- Las corrientes de polarización del TLV9062 con resistencias de decenas de kΩ (no son críticas: el lazo de auto-cero las compensa).
- Que WEBENCH ofrezca el TLV9062/64 en la versión actual.
