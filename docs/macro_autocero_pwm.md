# [MACRO] Especificación del PWM de auto-cero (I0 / Q0), controlado por serial

> **Este documento es para Macro, no para Micro 3** (cambio de enfoque del profesor, 2026-11-04). En Micro 3 el PWM ya no fija niveles de auto-cero: reproduce las señales muestreadas (`docs/especificacion_pwm.md`). Lo de aquí (auto-cero, I0/Q0, comandos `P:I,…`, lazo de bisección, pruebas 3.10 a 3.16 y 5.x) queda guardado para Macro. Los datos del chip (§3) y los pines (§4) siguen valiendo y son los mismos que usa Micro 3.

Proyecto Micro 3 · Fase 2a · Fecha: 2026-10-21 · Estado: **PROPUESTA, sin aprobar** (no hay código)

> **Para qué sirve:** fijar pines, recursos del chip, eventos, tareas, protocolo y pruebas del PWM **antes de escribir código**. Meta final: variar el PWM desde el PC por la UART para ajustar a mano el offset de I y Q; el lazo automático (ADC) viene después y reutiliza esta misma base.
>
> **Etiquetas:** **[VERIFICADO]** = sale del datasheet (DS40001985C), del preinforme o de un documento de `docs/`. **[CÁLCULO]** = cuenta propia de este documento, por comprobar en la placa. **[PROPUESTA]** = decisión por aprobar.
>
> **Fuentes:** `MASTER_PLAN.md`, `docs/especificacion_diseno_uart.md` §3, §5–§8, `docs/protocolos_prueba.md` Módulo 3 y 5, `docs/notas_pic16f18426.md` §9, preinforme de la Práctica 2 (ecuación 5) y el `.asc` de LTspice.

---

## 1. Qué hace el PWM en el circuito

Hoy I0 y Q0 son dos trimmers de 10 kΩ entre 0 V y VCC (3,7 V), seguidos de un buffer [VERIFICADO: preinforme §1.4 y `.asc`]. El PWM los sustituye:

```
PIC (PWM6) ──► filtro RC pasa-bajos ──► buffer ──► I0 ──┐
                                                         ├─► etapa resta, ganancia 10 ──► VI_OUT ──► ADC
                              I_LPF (filtro de 20 Hz) ───┘
```

- Ecuación de la placa [VERIFICADO: preinforme, ec. 5]: `VI_OUT = Vref2 + 10·(I_LPF − I0)` y `VQ_OUT = Vref2 + 10·(Q_LPF − Q0)`, con Vref2 ≈ 1,85 V.
- Tensión del PWM filtrado: `V = VDD · duty`, con VDD = 3,7 V [VERIFICADO: spec. del enunciado §3.1].
- **Signo (importante para el lazo futuro):** subir I0 (más duty) **baja** VI_OUT. Si el ADC lee más de 2048, hay que **subir** el duty de ese canal.
- Valores de referencia del preinforme (tubo sano, ya calibrados a mano): I0 ≈ 1,93 V (duty ≈ 52 %, cuenta ≈ 534) y Q0 ≈ 1,77 V (duty ≈ 48 %, cuenta ≈ 490) [CÁLCULO sobre datos del preinforme]. Dicen dónde cae el rango útil; cada ensayo da valores distintos.

## 2. Números que condicionan el diseño [CÁLCULO]

| Magnitud | Valor | Consecuencia |
|---|---|---|
| Resolución del PWM | 10 bits = 1024 pasos | `T2PR = 255`, ver §3 |
| Paso en I0 o Q0 | 3,7 V / 1024 = **3,61 mV** | coincide con la prueba 3.7 |
| Paso en VI_OUT o VQ_OUT (×10) | **36,1 mV = 40 cuentas del ADC** (1 cuenta = 0,903 mV) | **ver advertencia** |
| Error mínimo alcanzable | medio paso: **±18 mV = ±20 cuentas** | la prueba 5.3 pide ≤ 3 cuentas |
| Desplazamiento que produce la discontinuidad (preinforme §2.6, ×10) | ≈ 60 mV en I y ≈ 110 mV en Q (66 y 122 cuentas) | el residuo de ±20 cuentas es el 15–30 % de la señal |
| Rizado a 15,625 kHz, filtro RC de 1 etapa 10 kΩ + 1 µF | ≈ 4,8 mV pp en I0, ≈ 48 mV pp en la salida | **no sirve** |
| Rizado, filtro RC de 2 etapas 10 kΩ + 220 nF (RC = 2,2 ms por etapa) | ≈ 0,10 mV pp en I0, ≈ 1,0 mV pp en la salida (1,1 cuentas) | **sirve** |
| Tiempo de establecimiento, 2 etapas de 2,2 ms | 99 % en ≈ 27 ms; a medio paso de PWM en ≈ 45 ms | esperar **≥ 50 ms** entre un cambio de duty y una lectura |

> **Advertencia 1 (resolución).** Con el PWM de 10 bits sustituyendo el trimmer 1 a 1, el auto-cero **no puede dejar la salida a menos de ±20 cuentas del centro**. El criterio actual de la prueba 5.3 («≤ 3 cuentas») es imposible tal como está. Opciones en §9, decisión D-1.
>
> **Advertencia 2 (rizado).** El criterio de la prueba 3.5 («≤ 0,9 mV») está medido en la salida del filtro, pero ese rizado se multiplica ×10 en la etapa de resta. Para que la salida final tenga ≤ 1 cuenta, el rizado **en I0** debe ser ≤ **0,09 mV pp**. Propongo corregir la prueba 3.5 a ese valor (D-2).

## 3. Recursos del chip [VERIFICADO salvo lo marcado]

| Recurso | Valor | Fuente |
|---|---|---|
| Módulos | **PWM6** (canal I0) y **PWM7** (canal Q0), 10 bits | DS cap. 24 |
| Base de tiempo | **Timer2** para los dos: tras el reset `CCPTMRS1` tiene `P6TSEL = 01` y `P7TSEL = 01` (Timer2), así que **no hay que escribirlo** | DS §23.2.2, p.346 |
| Periodo | `(T2PR + 1) · 4 · Tosc · prescaler`. Con Fosc = 16 MHz, `T2PR = 255`, prescaler 1:1: 256 · 4 · 62,5 ns = **64 µs = 15,625 kHz** | DS Ec. 24-1 |
| Reloj de Timer2 | debe ser **Fosc/4**: `T2CLKCON.CS = 0001` (el valor de reset es el pin externo, no cuenta) | DS §24.3; `notas` §9.3 |
| Duty de 10 bits | `PWMxDCH` = 8 bits altos, `PWMxDCL[7:6]` = 2 bits bajos. Para un valor `d` de 0 a 1023: `DCH = d >> 2`, `DCL = (d & 3) << 6` | DS §24.4 |
| Doble buffer del duty | los registros se pueden escribir en cualquier momento, pero se copian al hardware cuando `T2TMR = T2PR`. Si el cambio cae entre escribir DCH y DCL, **un solo periodo** sale con un valor mezclado (error ≤ 4 cuentas = 14 mV en I0 durante 64 µs); el filtro RC lo borra. No hace falta apagar interrupciones | DS §24.1; [CÁLCULO] |
| Habilitar | `PWMxCON.EN = 1` (bit 7). `POL = 0` (salida normal) | DS §24.12.2 |
| Pin | bit `TRIS` en 0 **y** `RxyPPS`: `RA2PPS = 0x0D` (PWM6OUT), `RA4PPS = 0x0E` (PWM7OUT) | DS Tabla 16-3 |
| Cuentas máxima y mínima | duty 0 = siempre bajo (0 V); 1023 = 99,9 % (3,696 V) | DS Ec. 24-3 |
| Interrupciones | **ninguna**: el PWM corre solo en hardware | DS cap. 24 |
| `PPS1WAY` | ya está en OFF (C-01), el PPS se puede cambiar | `estado.md` |
| RA4 | `FEXTOSC = 100` (sin oscilador externo), así que OSC2 no se usa y RA4 queda libre | `notas` §4 |

Los nombres exactos de los registros en C (`PWM6DCH`, `PWM6DCL`, `PWM6CON`, `RA2PPS`, `T2CON`, `T2PR`, `T2CLKCON`) se **comprueban en `pic16f18426.h`** de la instalación de XC8 2.40 antes de usarlos, como pide la regla del proyecto. `CCPTMRS1` y `T2CLKCON` ya están confirmados en el datasheet; el `.h` fija el nombre en C.

**Timers ocupados:** Timer2 queda **reservado para el PWM** (no usarlo para nada más). Timer4 y Timer6 siguen libres para tiempos de espera.

## 4. Pines

| Pin físico | Puerto | Función | Dirección | Notas |
|---|---|---|---|---|
| 11 | **RA2** | PWM6 → I0 | salida | `TRISA2 = 0`, `ANSELA2 = 0`, `RA2PPS = 0x0D`. RA2 también es INT/ADCVREF−: no se usan |
| 3 | **RA4** | PWM7 → Q0 | salida | `TRISA4 = 0`, `ANSELA4 = 0`, `RA4PPS = 0x0E` |
| 6 / 5 | RC4 / RC5 | UART TX / RX | – | sin cambios |
| 10 / 9 | RC0 / RC1 | entradas I y Q (ADC) | – | fase siguiente |
| 7 | RC3 | pulsador de auto-cero | – | fase siguiente |
| 8 | RC2 | libre (pin de prueba) | – | puede medir la ISR |

`ANSELA` hoy vale `0x37` y `TRISA` vale `0x3F` (valores de MCC): hay que bajar los bits 2 y 4 en cada registro. Nota: según el DS §14.4.6 (p.191), `ANSEL` no afecta a las salidas digitales, pero la regla del proyecto es bajarlo igual. **No se cambia ningún pin ya decidido.**

**Hardware que falta definir [PENDIENTE]:** (a) valores reales del filtro RC (propuesta en §2: 2 etapas, 10 kΩ + 220 nF); (b) en qué nodo de PCB2 entra el PWM filtrado: el punto que hoy es el cursor del trimmer hacia el buffer de I0 y de Q0 (los trimmers deberán desconectarse o quitarse para no pelear con el PWM); (c) tierra analógica común entre el PIC y PCB2.

## 5. Variables [PROPUESTA]

| Variable | Tipo | Quién la escribe | Descripción |
|---|---|---|---|
| `pwmDuty[2]` | `uint16_t`, 0 a 1023 | solo el programa principal | índice 0 = I0 (PWM6), 1 = Q0 (PWM7). Copia del duty aplicado, para responder `GET_PWM` sin leer el hardware |
| Valor inicial | 512 en ambos | `pwm_init()` | ≈ 1,85 V = Vref2. Evita saturar la salida al arrancar |

Durante el reset los pines son entradas (alta impedancia), así que el buffer ve el condensador del filtro sin control: la salida de PCB2 puede quedar en cualquier punto hasta que `pwm_init()` corra. Es normal.

## 6. Eventos

| ID | Evento | Fuente | Respuesta |
|---|---|---|---|
| EP1 | Llegó una línea `P:I,534` o `P:Q,534` [PROPUESTA, §8] | lector de comandos de texto (por escribir) | valida canal y rango, llama a `pwm_set`, encola `OK:I,534\r\n` con el valor aplicado |
| EP2 | Llegó `P?` | lector de comandos | encola `P:I,534,Q,512\r\n` con los valores actuales |
| EP3 | Canal inválido o duty > 1023 | lector de comandos | **no toca el PWM**; encola `E:canal\r\n` o `E:rango\r\n` |
| EP4 | Timer2 coincide con `T2PR` | hardware | copia el duty a la salida. **Sin ISR**, no consume CPU |
| EP5 (fase siguiente) | Pasó el tiempo de establecimiento tras un cambio | Timer4 (ms) | el lazo de auto-cero ya puede leer el ADC. **No se implementa en esta fase** |

El PWM no agrega ninguna fuente a la ISR, así que el plazo de 174 µs de la recepción UART no cambia.

## 7. Tareas

| ID | Función | Dónde corre | Descripción |
|---|---|---|---|
| P1 | `pwm_init()` | arranque, antes de habilitar interrupciones | Timer2 (`CS` = Fosc/4, `T2PR` = 255, prescaler 1:1, `ON`), duty inicial de los dos canales, PPS, `TRIS`, `ANSEL`, `PWMxCON.EN` |
| P2 | `bool pwm_set(uint8_t canal, uint16_t duty)` | programa principal | devuelve `false` si canal > 1 o duty > 1023 y no toca nada; si no, escribe DCH y DCL y guarda `pwmDuty` |
| P3 | `uint16_t pwm_get(uint8_t canal)` | programa principal | devuelve `pwmDuty[canal]` (0xFFFF si el canal no existe) |
| P4 | `cmd_task()` [PROPUESTA] | programa principal | junta los bytes de RX hasta `\n` (sin esperar), separa canal y número, llama a P2/P3 y encola la respuesta con `tx_string` |

Reglas: sin `while` que espere, sin `__delay_ms`. Ninguna función de PWM se llama desde la ISR (así no se duplica código ni hay accesos compartidos). Archivos nuevos: `pwm.c` y `pwm.h`. Comentarios en español, línea por línea, con el formato fijo de `MASTER_PLAN.md`.

## 8. Comandos por serial [PROPUESTA, por replantear con T-07]

Desde el 2026-11-04 el enlace es **solo texto ASCII y solo PIC → PC** (`docs/especificacion_protocolo.md`). Para variar el PWM desde el PC hace falta que el PIC lea líneas de texto. Propuesta:

| Del PC al PIC | Respuesta del PIC | Qué hace |
|---|---|---|
| `P:I,534\n` | `OK:I,534\r\n` | pone el duty de I0 en 534 (0 a 1023) |
| `P:Q,512\n` | `OK:Q,512\r\n` | igual para Q0 |
| `P?\n` | `P:I,534,Q,512\r\n` | devuelve los dos valores actuales |
| canal que no es `I` ni `Q` | `E:canal\r\n` | no cambia nada |
| duty > 1023 o sin número | `E:rango\r\n` | no cambia nada |

- Las respuestas se mezclan con las líneas `I,Q` del ADC: el PC las distingue porque empiezan con otra letra. Caben: una respuesta es de ≤ 16 bytes y la línea va al 48,1 %.
- Ejemplo: poner I0 a 1,93 V → duty = 1,93 / 3,7 · 1024 ≈ 534 → `P:I,534\n`.
- Leer los números se hace con la operación inversa de `a_decimal` (multiplicar por 10 y sumar cada cifra), sin `scanf`.

## 9. Decisiones abiertas

| ID | Decisión | Opciones | Mi recomendación |
|---|---|---|---|
| **D-1** | Cómo tratar la resolución (advertencia 1) | **A.** Aceptar ±20 cuentas y **restar el residuo en el PC/PIC** tras el auto-cero (el PWM deja la salida en el centro grueso; lo fino lo corrige software). **B.** Atenuar el PWM por hardware (divisor hacia Vref2): reduce el rango pero mejora el paso. Cambio de placa. **C.** Dithering: alternar duty entre dos valores vecinos en cada periodo de Timer2 para ganar 2 bits (12 bits efectivos, ±5 cuentas). Exige una ISR de Timer2 cada 64 µs o un postscaler | **A para empezar** (la persona está de acuerdo con los 10 bits, 2026-10-21; la resta es una operación de 16 bits por muestra, o se hace en el PC) (cero cambios de hardware ni de ISR) y medir el residuo real. Si no alcanza, evaluar C. Cambiar el criterio 5.3 a «≤ 20 cuentas con 10 bits» |
| **D-2** | Criterio de rizado de la prueba 3.5 | dejar 0,9 mV, o corregir a 0,09 mV pp en I0 | corregir |
| **D-3** | Cómo variar el duty por serial | **A.** Comandos de texto `P:I,534` (§8). **B.** Sin serial: duty fijo en el código y se recompila para cada valor | **B para T-06** (el bring-up del PWM no necesita serial); **A** cuando se replantee T-07 |
| **D-4** | Valor inicial del duty | 512 en ambos, o los valores del último ensayo | 512 |
| **D-5** | Valores R y C del filtro | era: 2 etapas pasivas 10 kΩ + 220 nF | **Reemplazada (2026-11-04):** la persona diseñará un filtro **activo de 2.º orden** en WEBENCH. Criterios (fc propuesta 30 Hz, entradas de WEBENCH, riesgo del rizado y pruebas) en `docs/criterios_filtro_pwm.md`. Los números de §2 de este archivo que hablan del filtro pasivo (rizado de 0,10 mV y 45 ms) quedan como referencia comparativa. T-06 no la necesita para empezar |

## 10. Pruebas

Se usan las del **Módulo 3** de `docs/protocolos_prueba.md` (3.1 a 3.9) con la corrección de la 3.5, más estas nuevas [PROPUESTA]:

| ID | Qué se verifica | Procedimiento | Criterio |
|---|---|---|---|
| 3.10 | Cambio de duty por serial | `P:I,256`, `P:I,512`, `P:I,768`; medir en el pin | ancho de pulso = duty · 62,5 ns, ±1 paso |
| 3.11 | Leer el valor | `P?` tras cada cambio | igual al enviado |
| 3.12 | Canal inválido | `P:X,512` | `E:canal` y el PWM no cambia |
| 3.13 | Duty fuera de rango | `P:I,1024` | `E:rango` y el PWM no cambia |
| 3.14 | No bloquea | mandar 100 comandos seguidos mientras llegan las líneas del ADC | ninguna línea perdida (`adcLineasPerdidas` = 0); el bucle principal nunca espera |
| 3.15 | Sentido del offset | subir el duty de I0 y medir VI_OUT con multímetro | VI_OUT **baja** ≈ 36 mV por paso |
| 3.16 | Rango de captura | barrer I0 de 0 a 1023 con el tubo sano | existe un duty donde VI_OUT ≈ 1,85 V (y lo mismo en Q) |

## 11. Orden de trabajo propuesto (para que Gemini lo refine en `MASTER_PLAN.md`)

| ID | Tarea | Criterio de aceptación |
|---|---|---|
| T-06 | `pwm.c/.h`: `pwm_init`, `pwm_set`, `pwm_get`. Duty fijo en `main`, **sin serial** | Pruebas 3.1 a 3.4 y 3.7 con osciloscopio y multímetro; el envío ASCII sigue pasando 2.1 a 2.4 |
| T-07 | Comandos de texto `P:I,…` / `P:Q,…` / `P?` (§8) | Pruebas 3.10 a 3.14 |
| T-08 | Con el filtro montado y PCB2 conectada: ajuste manual del offset desde el PC | Pruebas 3.5, 3.6, 3.8, 3.9, 3.15 y 3.16 |

Después viene el ADC y, encima de esto, el lazo de auto-cero (bisección con el signo de §1 y espera de ≥ 50 ms por paso, ≈ 10 pasos por canal ≈ 0,5 s).

## 12. Lo que no sé o no verifiqué

- Los valores reales de R y C del filtro en tu placa y el nodo exacto donde entra el PWM.
- Los nombres de los registros en `pic16f18426.h` (solo verificados en el datasheet).
- El rizado y los tiempos de la §2 son cálculos con filtros ideales, sin cargas ni tolerancias.
