# Protocolos de prueba y medición por módulo

Proyecto Micro 3 · Fecha: 2026-10-16 · Revisado: 2026-11-05 (pin RC2, LED retirado, sin anti-aliasing) · Borrador v2

> **Estado:** los criterios de aceptación marcados **[PROPUESTA]** son decisiones de diseño por confirmar con el profesor. Los valores esperados están calculados a partir de la especificación (`especificacion_diseno_uart.md`). Hay que reemplazarlos por lo que se mida y completar la columna "Resultado" en cada prueba.

**Orden de trabajo:** cada módulo se prueba solo, antes de integrarlo con el siguiente. No se pasa al siguiente módulo hasta que el anterior cumpla todos sus criterios.

| Módulo | Fase | Estado |
|---|---|---|
| 0. Puesta en marcha (alimentación, programación, reloj) | 1 | 0.3 pasó (2026-10-19); el resto por hacer |
| 1. UART (enlace físico, buffer circular, ISR) | 1 | eco: 1.5, 1.6, 1.7 y 1.18 pasaron (2026-10-19); el resto por hacer |
| 2. Envío en texto ASCII (líneas `I,Q`) | 2 | el modo ASCII funciona (la persona, 2026-11-04); faltan 2.1 a 2.6 con 500 líneas/s |
| 3. PWM por canal + filtro pasa bajos (reconstruir I y Q) | 2 | código implementado y compilado (T-06), sin probar; filtro sin montar |
| 4. ADC (canales I y Q) | 2 | código implementado y compilado (T-09), sin probar en el chip; sin filtro anti-aliasing |
| 5. Pulsador y lazo de auto-cero | — | **pasa a Macro** (2026-11-04) |
| 6. Integración con el PC (gráfica I vs Q) | 2 | por hacer |

---

## Reglas generales

1. **Una prueba = un cambio.** Si se cambia código y hardware a la vez y falla, no se sabe cuál fue.
2. **Instrumentos mínimos:** multímetro, osciloscopio (de preferencia con decodificador serie), terminal serie (Realterm, Hercules o Tera Term). El LED de estado se retiró (T-05, 2026-10-21). Un analizador lógico ayuda, pero no es obligatorio.
3. **Pin de prueba RC2:** lo elige `ADC_PIN_PRUEBA` en `adc_iq.h`. Con **2** (lo que tiene el código) saca la salida del Timer0: cuadrada de 500 Hz, cada flanco es un disparo del ADC (pruebas 4.5 y 0.4). Con **1** sube mientras corre `adc_iq_isr()` (dos pulsos por ms: pruebas 4.13, 4.16 y 4.17); hay que recompilar. Con 0 no se toca. Solo marca la ISR del ADC, no las de la UART. En la placa nueva RC2 es `EN_AZ_PIC`: medir con JP_AZEN en 1–2.
4. **Registrar** cada resultado con fecha, versión del código (hash de git: `git rev-parse --short HEAD`) y la tabla de datos de la prueba.
5. **Tierra común** entre el osciloscopio, el conversor USB-serie y la placa. Medir con la punta del osciloscopio en 10× y el resorte de tierra corto.
6. Si una prueba falla, se anota en "Resultado" qué se midió y qué se cambió, antes de repetirla.

---

## Módulo 0. Puesta en marcha

| ID | Qué se verifica | Procedimiento | Valor esperado | Criterio | Resultado |
|---|---|---|---|---|---|
| 0.1 | Alimentación | Con la fuente en 3,7 V y límite de corriente bajo, medir VDD en los pines 1 y 14 del PIC, sin el PIC programado | 3,7 V | VDD entre 3,6 y 3,8 V | |
| 0.2 | Programación | Conectar el SNAP y programar un `main` mínimo | Programa sin errores | MPLAB X termina con "Programming complete" | |
| 0.3 | Revisión de silicio | MPLAB X → *Window > Dashboard* | Device ID = 0x30D2 (PIC16F18426) y anotar la revisión | ID correcto y revisión anotada. Medido 2026-10-19: ID 0x30D2, revisión 0x2002 (ver `docs/resultados_pruebas_uart.md`) | pasó |
| 0.4 | Reloj del sistema | Con el firmware actual (`ADC_PIN_PRUEBA` = 2): periodo de la cuadrada de RC2. (`CLKOUTEN` en RA4 ya no sirve: RA4 es el PWM de Q) | 2,000 ms (Fosc/4 = 4 MHz) | ± 2 % (1,96 a 2,04 ms) | |
| 0.5 | LED de estado | *(retirada el 2026-10-21 con T-05: no hay LED)* | — | — | — |
| 0.6 | Consumo | Medir la corriente de la fuente con el firmware de la Fase 1 | Anotar | Sin cortocircuito. Valor registrado | |

---

## Módulo 1. UART (Fase 1)

Configuración: 8N1, 115200 baud, `SP1BRG = 34` (baud real 114 286, error −0,79 %).

> Las pruebas de eco (1B y 1D) se hicieron el 2026-10-19 con un `main.c` de eco que ya no está en el firmware (desde el 2026-11-04 el PIC solo manda líneas de texto). Quedan como registro; para repetirlas haría falta volver a escribir ese eco.

### 1A. Capa física (osciloscopio sobre RC4 = TX)

| ID | Qué se verifica | Procedimiento | Valor esperado | Criterio | Resultado |
|---|---|---|---|---|---|
| 1.1 | Niveles lógicos de TX | Enviar el byte `0xFF` repetido y medir el nivel alto y bajo en RC4 | Alto ≈ 3,7 V, bajo ≈ 0 V | VOH ≥ 3,3 V y VOL ≤ 0,4 V | |
| 1.2 | Ancho de bit | Enviar `0x55` repetido (onda cuadrada) y medir el período | Bit = 8,75 µs (período 17,5 µs) | Dentro de ±2 % de 8,68 µs nominal: 8,51 a 8,85 µs | |
| 1.3 | Formato del byte | Decodificar un byte con el osciloscopio (modo serie) | 1 bit de inicio, 8 de datos (LSB primero), 1 de parada. Duración 10 bits = 87,5 µs | Decodificación correcta de 10 bytes seguidos | |
| 1.4 | Nivel mínimo de RX | Con el conversor conectado, medir el nivel alto que llega a RC5 | ≈ 3,3 V | ≥ 1,73 V (umbral TTL de RC5 con VDD = 3,7 V) | |

### 1B. Eco y buffers (terminal serie en el PC)

| ID | Qué se verifica | Procedimiento | Valor esperado | Criterio | Resultado |
|---|---|---|---|---|---|
| 1.5 | Eco de 1 byte | Enviar `0x41` desde la terminal en modo hexadecimal | Regresa `0x41` | 20 de 20 correctos | pasó (2026-10-19): 20/20 |
| 1.6 | Los 256 valores | Enviar los bytes `0x00` a `0xFF` | Regresan idénticos | 256 de 256, sin pérdidas ni cambios | pasó (2026-10-19): 256/256 |
| 1.7 | Ráfaga continua | Enviar 1000 bytes seguidos (archivo de la terminal o script de Python) | Regresan 1000 bytes. Duración mínima 86,8 ms (a 115200) | 1000 de 1000, sin overrun ni framing | pasó (2026-10-19): 1000/1000 en 141 ms |
| 1.8 | Ráfaga larga | Enviar 100 000 bytes pseudoaleatorios y comparar | Idéntico | 0 errores. Si hay errores, medir con `OSCTUNE` (ver especificación §2.2) | |
| 1.9 | Desbordamiento del buffer | Enviar más bytes de los que caben en el FIFO de RX sin que el programa principal los lea | Se activa el contador de errores. El PIC no se bloquea | `tx_DAT` devuelve `false` si el FIFO de TX está lleno. Sin reinicios | |

### 1C. Tiempos y ejecución no bloqueante (pin RC2 con osciloscopio)

| ID | Qué se verifica | Procedimiento | Valor esperado | Criterio | Resultado |
|---|---|---|---|---|---|
| 1.10 | Duración de la ISR de RX | Pulso en RC2 durante la ISR al recibir un byte (necesita código de prueba: hoy RC2 solo marca la ISR del ADC) | Anotar (objetivo en la especificación: < 100 ciclos = 25 µs) | ≤ 25 µs **[PROPUESTA]** | |
| 1.11 | Duración de la ISR de TX | Igual, durante la transmisión de un byte | Anotar | ≤ 25 µs **[PROPUESTA]** | |
| 1.12 | El programa principal no se bloquea | *(retirada con el LED de RA5; hoy lo cubren 2.4 y 4.14: `adcParesPerdidos` = 0 con la UART enviando)* | — | — | — |
| 1.13 | `TX1IE` solo con datos | Con el FIFO de TX vacío, medir con el depurador (punto de interrupción en la ISR) que no entra a la ISR de TX | La ISR de TX no se ejecuta | 0 entradas en 10 s | |
| 1.14 | Firmas pedidas en clase | Llamar `tx_DAT(buf, 5)` y `tx_string("hola")` | Se reciben los 5 bytes y los 4 caracteres | Correctos y sin bloquear | |

### 1D. Errores y recuperación

| ID | Qué se verifica | Procedimiento | Valor esperado | Criterio | Resultado |
|---|---|---|---|---|---|
| 1.15 | Framing | Cambiar el baud de la terminal a 57600 y enviar bytes | El PIC cuenta errores de framing y sigue vivo | Contador de framing > 0. Al volver a 115200 el enlace funciona sin reiniciar | |
| 1.16 | Overrun | Provocar un overrun (ráfaga con la ISR detenida en el depurador) y reanudar | Se recupera con la secuencia `CREN` 0→1 | El eco vuelve a funcionar | |
| 1.17 | Desconexión | Desconectar y reconectar el cable USB-serie | El PIC sigue funcionando | El enlace se recupera sin reiniciar el PIC | |
| 1.18 | Reinicio | Pulsar MCLR / reiniciar con el programador | Arranca y vuelve a responder | Eco correcto tras el reinicio | |

---

## Módulo 2. Envío en texto ASCII (Fase 2)

El PIC manda `xxxx,xxxx\r\n` cada 2 pares del ADC: 500 líneas por segundo, sin comando del PC (`docs/especificacion_protocolo.md`). Herramientas: una terminal serie, `herramientas/receptor_micro3.py` o `herramientas/interfaz_micro3.py`.

| ID | Qué se verifica | Procedimiento | Valor esperado | Criterio | Resultado |
|---|---|---|---|---|---|
| 2.1 | Forma de la línea | Abrir una terminal a 115200 8N1 y mirar 20 líneas | número (I) + `,` + número (Q) + fin de línea; números de 0 a 4095 | 20 de 20 con la forma correcta | |
| 2.2 | Líneas por segundo | `receptor_micro3.py COM5 --muestras 10000` y leer el resumen | 500 muestras/s (1000 / `ADC_PARES_POR_LINEA`) | 500 ± 1 % | |
| 2.3 | Tiempo de una línea | Osciloscopio en RC4 (TX), con decodificador serie si hay | Ráfaga de 11 bytes de ≈ 0,96 ms cada 2 ms | 0,94 a 1,00 ms; periodo 2 ms ± 1 % | |
| 2.4 | Sin pérdidas | 10 min con `interfaz_micro3.py` y el depurador de MPLAB X | 300 000 líneas | Líneas malas ≤ 1 (la primera, cortada) y `adcLineasPerdidas` = 0 | |
| 2.5 | Arranque y reconexión | Encender el PIC con el PC ya escuchando; luego desconectar y reconectar el USB | Llega `Micro3 listo` y después las líneas; al reconectar siguen llegando | Sin reiniciar el PIC | |
| 2.6 | Valor correcto | Entrada fija de 1,000 V medida con el multímetro en RC0 (y en RC1) | Código ≈ 1,000 × 4096 / 3,7 = 1107 | ± 10 códigos del calculado con la VDD medida | |

---

## Módulo 3. PWM por canal y filtro pasa bajos: reconstruir I y Q (Fase 2)

Cambio de enfoque del profesor (2026-11-04): el PWM ya no es el auto-cero (eso es de Macro, `docs/macro_autocero_pwm.md`). Ahora cada par del ADC se copia a un PWM por canal y un filtro pasa bajos devuelve la señal analógica. PWM6 en RA2 (I) y PWM7 en RA4 (Q); Timer2 con `T2PR = 255` a Fosc/4: periodo 64 µs (15,625 kHz), 10 bits. `duty = (código + 2) >> 2`; salida filtrada ideal `V = VDD · duty / 1024` = la tensión de entrada. Detalle: `docs/especificacion_pwm.md`.

Montaje: generador (o fuente fija) en RC0 y RC1 como en el Módulo 4; filtro de `docs/especificacion_pwm.md` §6 (A: Sallen-Key de ≈ 100 Hz; B: RC pasivo de 2 etapas) en RA2 y RA4; osciloscopio de 2 canales.

| ID | Qué se verifica | Procedimiento | Valor esperado | Criterio | Resultado |
|---|---|---|---|---|---|
| 3.1 | Frecuencia | Osciloscopio en RA2 (pata 11) y RA4 (pata 3), antes del filtro | 15,625 kHz (64 µs) | ±2 % (oscilador interno) | |
| 3.2 | El duty sigue a la entrada | Entrada fija de 0,5 V, 1,85 V y 3,0 V en RC0; medir el ancho del pulso en RA2 | ancho = 64 µs · V / 3,7 V (8,6 µs, 32 µs y 51,9 µs) | error ≤ 2 pasos (125 ns) | |
| 3.3 | Continua tras el filtro | Con las mismas entradas, multímetro en la entrada (RC0) y en la salida del filtro | salida = entrada | diferencia ≤ 10 mV (≈ 3 pasos) **[PROPUESTA]** | |
| 3.4 | Linealidad | 11 tensiones de entrada de 0,2 a 3,5 V; graficar salida contra entrada | recta de pendiente 1 | error máximo ≤ 1 % de VDD (37 mV) **[PROPUESTA]** | |
| 3.5 | Rizado del PWM | Osciloscopio en AC en la salida del filtro, entrada en 1,85 V (duty 50 %, peor caso) | ≈ 0,2 mV pp (filtro A), ≈ 0,5 mV pp (B) | ≤ 1 código del ADC (0,9 mV) **[PROPUESTA]** | |
| 3.6 | Seguimiento de una senoidal | Generador: 1,7 V de amplitud centrada en 1,85 V, a 5 Hz y a 20 Hz; canal 1 en RC0, canal 2 en la salida del filtro | misma forma y amplitud (filtro A: −0,01 dB a 20 Hz) | amplitud ±2 %; sin escalones visibles | |
| 3.7 | Retardo | Mismo montaje; medir el desfase entre entrada y salida a 20 Hz | ≈ 2,3 ms del filtro + ≈ 0,6 ms de muestreo y retención | anotar | |
| 3.8 | Independencia y envío | Mover solo la entrada de I; ver que la salida de Q no cambie; a la vez, revisar que siguen llegando 500 líneas/s al PC | Q quieta; líneas sin pérdidas | variación de Q ≤ 1 paso (3,6 mV); `adcLineasPerdidas` = 0 | |

Opcional: SINAD de la salida reconstruida, capturando la salida del filtro con un segundo ADC u osciloscopio con FFT. Simulación: ≈ 54 dB a 20 Hz y ≈ 64 dB a 5 Hz con fc = 100 Hz (`docs/especificacion_pwm.md` §6.3). No es el requisito de > 60 dB, que es del ADC (4.15).

Las pruebas del PWM de auto-cero (rizado en I0/Q0, establecimiento para el lazo, comandos por serial, sentido del offset) pasan a Macro: `docs/macro_autocero_pwm.md` §10.

---

## Módulo 4. ADC: canales I y Q (Fase 2)

ADC de 12 bits con referencia VDD = 3,7 V: 1 LSB = 3,7 V / 4096 = 0,903 mV. La señal en reposo debe quedar cerca de Vref2 (1,85 V), que corresponde al código 2048.

Muestreo a 1 kHz disparado por Timer0, I y Q convertidos en menos de 1 ms, sin promedio: una muestra de cada canal por milisegundo (se quitó `ADC_N_PROMEDIO` el 2026-11-03). Especificación: `docs/especificacion_adc.md`. **Requisito de la persona (2026-10-30): SINAD del ADC > 60 dB** (prueba 4.15).

**Sin filtro anti-aliasing** en esta entrega (indicación de la persona, 2026-11-05).

Los datos llegan al PC como líneas `I,Q` a 500 por segundo (1 de cada 2 pares; el ADC sigue muestreando a 1 kHz y el PWM usa todos los pares). Pin de prueba RC2 (`ADC_PIN_PRUEBA` en `adc_iq.h`): con 1 sube durante `adc_iq_isr()` (**dos pulsos por periodo**, fin de I y fin de Q); con 2 saca la salida del Timer0 (cuadrada de 500 Hz, cada flanco es un disparo).

| ID | Qué se verifica | Procedimiento | Valor esperado | Criterio | Resultado |
|---|---|---|---|---|---|
| 4.1 | Código en reposo | Entrada I y Q conectadas a Vref2 (1,85 V medidos con el multímetro) | Código ≈ 2048 | 2048 ± 10 códigos **[PROPUESTA]** | |
| 4.2 | Conversión contra voltaje | Aplicar 0,5 V, 1,0 V, 1,85 V, 2,5 V, 3,2 V desde una fuente, medir cada uno con el multímetro y comparar con el código | Código = V / 3,7 × 4096 | Error ≤ 5 LSB en cada punto **[PROPUESTA]**. Nota: el datasheet da offset ±0,5 LSb típ. (±6 máx.) y ganancia ±0,2 típ. (±6 máx.) con `VDD` = 3 V y `TAD` = 1 µs (DS p.632); en el peor caso la suma supera los 5 LSB | |
| 4.3 | Ruido (histograma) | Con la entrada en 0 V (pin a GND) y luego en Vref2, tomar ≥ 4096 muestras, contar los códigos y calcular σ (`docs/especificacion_adc.md` §8) | Anotar σ, códigos distintos y pico a pico | σ ≤ 2 LSB **[PROPUESTA]**. Ver CP-6: con σ = 2 LSB el SINAD a escala completa sería ≈ 57 dB | |
| 4.4 | Canales independientes | Variar solo I y verificar que Q no cambie, y al revés | Sin cambio en el otro canal | Variación ≤ 3 LSB | |
| 4.5 | Frecuencia de muestreo | Con `ADC_PIN_PRUEBA` = 2: tiempo entre flancos consecutivos de RC2 (salida del Timer0). Para comprobar que cada flanco dispara una conversión, repetir con `ADC_PIN_PRUEBA` = 1 y medir el periodo entre pulsos de I | 1000 µs entre flancos (cuadrada de 2 ms). Si los pulsos de I salen cada 2000 µs, ver `especificacion_adc.md` §2.1: `TMR0H` = 124 | Error de período ≤ 1 % | |
| 4.6 | Señal de 20 Hz | Inyectar una senoidal de 5 Hz y otra de 20 Hz con un generador, y capturar la forma por UART | Amplitud y frecuencia correctas | Frecuencia ± 2 %, amplitud ± 5 % **[PROPUESTA]** | |
| 4.7 | Aliasing | Inyectar una señal por encima de fs/2 | **Pendiente: en esta entrega no hay filtro anti-aliasing.** Sin filtro, la señal aparece plegada (en \|f − k·1000\| Hz en el ADC y en \|f − k·500\| Hz en el PC) | Se define cuando haya filtro | |
| 4.8 | Tiempo de adquisición | Cambiar la fuente de alta a baja impedancia y comparar; repetir con la salida real del opa de PCB2 (`docs/especificacion_adc.md` §3.2) | Mismo código | Diferencia ≤ 2 LSB. Si no, aumentar el tiempo de adquisición con la fórmula de `docs/especificacion_adc.md` §3.1 | |
| 4.10 | *(retirada el 2026-11-04: era la compatibilidad con el eco, que ya no está en el firmware)* | — | — | — | |
| 4.13 | Duración de `adc_iq_isr()` *(inferida del código)* | Ancho del pulso de RC2 con el osciloscopio (`ADC_PIN_PRUEBA` = 1, recompilar) | Anotar | ≤ 25 µs **[PROPUESTA]**, igual que 1.10 y 1.11 | |
| 4.14 | Sin pérdidas *(inferida del código)* | 10 min de envío (500 líneas/s, 48,1 % de la línea); leer `adcParesPerdidos` y `adcLineasPerdidas` en el depurador | Ambos contadores en 0 | Y 500 líneas/s en el PC (prueba 2.4) | |
| **4.15** | **SINAD del ADC (requisito > 60 dB)** | Generador **directo** en RC0 (luego RC1), sin PCB2, con ≤ 10 kΩ. Senoidal de 1,7 V de amplitud centrada en 1,85 V a 5 Hz y a 20 Hz (para ciclos enteros con `fs` = 500 Hz en el PC y 4096 muestras: `k` = 41 da 5,005 Hz y `k` = 163 da 19,90 Hz; con otra frecuencia, la ventana de Kaiser reduce el error). Repetir con 0,5 V de amplitud. Capturar 4096 pares (8,2 s a 500 líneas/s) con `herramientas/interfaz_micro3.py` (`docs/interfaz_mediciones.md` §3) | 73 dB si solo hay cuantización | **SINAD ≥ 60 dB** (ENOB ≥ 9,67 bits) a 5 Hz y 20 Hz en I y en Q | |
| 4.16 | Tiempo real de adquisición y ráfaga de I y Q | Con `ADC_PIN_PRUEBA` = 1. Osciloscopio en RC2, disparado en el flanco de subida del primer pulso: tiempo desde el flanco de bajada del primer pulso hasta el flanco de subida del segundo | ≈ 38,5 µs si `ADACQ` cuenta ciclos de Fosc (10 µs, lo que guarda MCC); ≈ 68,5 µs si cuenta `TCY` (40 µs, Fig. 32-2); ≈ 349 µs si cuenta `TAD` (320 µs, DS p.529). Cada medida puede tener 2 a 4 µs de más por la latencia de la ISR | I + Q terminan en < 1 ms (≤ 800 µs, 20 % de margen). Anotar cuál de los tres casos es (CP-7) | |
| 4.17 | Jitter del instante de muestreo de Q | Con `ADC_PIN_PRUEBA` = 1 y la UART transmitiendo y recibiendo, osciloscopio en persistencia sobre RC2: variación de la separación entre el pulso de I y el de Q | Anotar el ancho de la variación `T` | `T` ≤ 8,7 µs (−70 dB a 20 Hz y escala completa) **[PROPUESTA]**; hasta 27 µs consume los 60 dB enteros | |
| 4.18 | Serial Plotter | Generador en RC0/RC1; abrir el Serial Plotter del Arduino IDE a 115200 | I y Q siguen al generador; `adcLineasPerdidas` = 0 | Solo cualitativa (a 500 líneas/s corre rápido; con `ADC_PARES_POR_LINEA` = 50 se ve mejor una señal lenta) | |

Las pruebas **4.9, 4.11 y 4.12** también las cita el código, pero su definición (en `docs/instrucciones_claude_code_adc.md`, que no está en la carpeta) no se encontró: sin definir. Las 4.10, 4.13 y 4.14 se reconstruyeron de los comentarios del código y deben revisarse.

---

## Módulo 5. Pulsador y lazo de auto-cero (pasa a Macro)

> **Para Macro, no para Micro 3** (cambio de enfoque del profesor, 2026-11-04). Se conserva como referencia.

| ID | Qué se verifica | Procedimiento | Valor esperado | Criterio | Resultado |
|---|---|---|---|---|---|
| 5.1 | Antirrebote | Presionar el botón 50 veces | Un solo evento por pulsación | 50 de 50 | |
| 5.2 | Disparo por comando | Enviar el comando de auto-cero por UART (formato de texto por definir, T-07) | Inicia la calibración | Responde con una línea de aceptación | |
| 5.3 | Convergencia | Introducir un desbalance (mover un trimmer o inyectar una tensión) y ejecutar el auto-cero | I y Q llegan cerca de 2048 | diferencia con 2048 de 3 LSB o menos en ambos canales **[PROPUESTA]** | |
| 5.4 | Tiempo de calibración | Cronometrar desde el inicio hasta el fin | Anotar | ≤ 5 s **[PROPUESTA]** | |
| 5.5 | Repetibilidad | Repetir la calibración 10 veces con el mismo desbalance | Mismo duty final | Variación ≤ 2 pasos | |
| 5.6 | Saturación | Desbalance fuera del rango alcanzable | El PIC entra en ERROR y avisa al PC | No se queda en un bucle infinito | |
| 5.7 | Estabilidad posterior | Dejar 10 min en reposo | Código estable | Deriva ≤ 5 LSB **[PROPUESTA]** | |

---

## Módulo 6. Integración con el PC (Fase 2)

| ID | Qué se verifica | Procedimiento | Valor esperado | Criterio | Resultado |
|---|---|---|---|---|---|
| 6.1 | Recepción en LabVIEW o Python | Conectar y leer las líneas `I,Q` (`docs/guia_labview_paso_a_paso.md`) | Valores iguales a los de la terminal | Coinciden en 100 muestras | |
| 6.2 | Plano de impedancia | Con la sonda sobre un tubo sano (en reposo) | Punto o nube pequeña cerca del origen | Nube de radio ≤ 5 LSB **[PROPUESTA]** | |
| 6.3 | Defecto | Pasar la sonda por un defecto conocido | Trayectoria clara fuera de la nube | Se distingue del ruido | |
| 6.4 | Indicador | Sano / dañado en el panel | Verde en reposo, rojo con defecto | 10 de 10 | |
| 6.5 | Tasa de datos | 500 líneas/s durante 10 min | 300 000 líneas | Líneas malas ≤ 1 y `adcLineasPerdidas` = 0 | |
| 6.6 | Carga del enlace | Calcular la ocupación | 500 × 0,96 ms = 48,1 % | Menor del 50 %: **cumple** (calculado) con 500 líneas/s de 11 bytes | |

---

## Hoja de resultados

Para cada ejecución se copia la siguiente tabla en `docs/resultados/AAAA-MM-DD_modulo.md`:

```
Fecha:
Módulo:
Versión del código (git):
Instrumentos usados:
Condiciones (VDD, temperatura, cable):

ID    Valor medido      ¿Cumple?    Observaciones
```
