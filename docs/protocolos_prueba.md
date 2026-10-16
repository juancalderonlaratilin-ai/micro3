# Protocolos de prueba y medición por módulo

Proyecto Micro 3 · Fecha: 2026-10-16 · Borrador v1

> **Estado:** los criterios de aceptación marcados **[PROPUESTA]** son decisiones de diseño por confirmar con el profesor. Los valores esperados están calculados a partir de la especificación (`especificacion_diseno_uart.md`). Hay que reemplazarlos por lo que se mida y completar la columna "Resultado" en cada prueba.

**Orden de trabajo:** cada módulo se prueba solo, antes de integrarlo con el siguiente. No se pasa al siguiente módulo hasta que el anterior cumpla todos sus criterios.

| Módulo | Fase | Estado |
|---|---|---|
| 0. Puesta en marcha (alimentación, programación, reloj) | 1 | por hacer |
| 1. UART (enlace físico, buffer circular, ISR) | 1 | por hacer |
| 2. Protocolo de tramas (CRC, errores, resincronización) | 1 | por hacer |
| 3. PWM + filtro RC (auto-cero) | 2 | por hacer |
| 4. ADC (canales I y Q) | 2 | por hacer |
| 5. Pulsador y lazo de auto-cero | 2 | por hacer |
| 6. Integración con el PC (gráfica I vs Q) | 2 | por hacer |

---

## Reglas generales

1. **Una prueba = un cambio.** Si se cambia código y hardware a la vez y falla, no se sabe cuál fue.
2. **Instrumentos mínimos:** multímetro, osciloscopio (de preferencia con decodificador serie), terminal serie (Realterm, Hercules o Tera Term) y el LED de estado. Un analizador lógico ayuda, pero no es obligatorio.
3. **Pin de prueba RC2** (reserva): el firmware lo pone en 1 al entrar a la ISR y en 0 al salir. El ancho del pulso en el osciloscopio es la duración de la ISR. Se quita del código final.
4. **Registrar** cada resultado con fecha, versión del código (hash de git: `git rev-parse --short HEAD`) y la tabla de datos de la prueba.
5. **Tierra común** entre el osciloscopio, el conversor USB-serie y la placa. Medir con la punta del osciloscopio en 10× y el resorte de tierra corto.
6. Si una prueba falla, se anota en "Resultado" qué se midió y qué se cambió, antes de repetirla.

---

## Módulo 0. Puesta en marcha

| ID | Qué se verifica | Procedimiento | Valor esperado | Criterio | Resultado |
|---|---|---|---|---|---|
| 0.1 | Alimentación | Con la fuente en 3,7 V y límite de corriente bajo, medir VDD en los pines 1 y 14 del PIC, sin el PIC programado | 3,7 V | VDD entre 3,6 y 3,8 V | |
| 0.2 | Programación | Conectar el SNAP y programar un `main` mínimo | Programa sin errores | MPLAB X termina con "Programming complete" | |
| 0.3 | Revisión de silicio | MPLAB X → *Window > Dashboard* | Anotar la revisión (A4 esperada, ID 0x2004) | Anotada en este documento | |
| 0.4 | Reloj del sistema | Código de prueba que invierte RC2 en un bucle con Timer, o con `CLKOUTEN` activado se mide Fosc/4 en RA4 (RA4 queda libre en la Fase 1) | Fosc/4 = 4 MHz | 4 MHz ± 2 % (3,92 a 4,08 MHz) | |
| 0.5 | LED de estado | Heartbeat a 1 Hz en RA5 | Parpadeo 1 Hz | Medido con cronómetro: 1 s ± 5 % | |
| 0.6 | Consumo | Medir la corriente de la fuente con el firmware de la Fase 1 | Anotar | Sin cortocircuito. Valor registrado | |

---

## Módulo 1. UART (Fase 1)

Configuración: 8N1, 115200 baud, `SP1BRG = 34` (baud real 114 286, error −0,79 %).

### 1A. Capa física (osciloscopio sobre RC4 = TX)

| ID | Qué se verifica | Procedimiento | Valor esperado | Criterio | Resultado |
|---|---|---|---|---|---|
| 1.1 | Niveles lógicos de TX | Enviar el byte `0xFF` repetido y medir el nivel alto y bajo en RC4 | Alto ≈ 3,7 V, bajo ≈ 0 V | VOH ≥ 3,3 V y VOL ≤ 0,4 V | |
| 1.2 | Ancho de bit | Enviar `0x55` repetido (onda cuadrada) y medir el período | Bit = 8,75 µs (período 17,5 µs) | Dentro de ±2 % de 8,68 µs nominal: 8,51 a 8,85 µs | |
| 1.3 | Formato de la trama | Decodificar un byte con el osciloscopio (modo serie) | 1 bit de inicio, 8 de datos (LSB primero), 1 de parada. Duración 10 bits = 87,5 µs | Decodificación correcta de 10 bytes seguidos | |
| 1.4 | Nivel mínimo de RX | Con el conversor conectado, medir el nivel alto que llega a RC5 | ≈ 3,3 V | ≥ 1,73 V (umbral TTL de RC5 con VDD = 3,7 V) | |

### 1B. Eco y buffers (terminal serie en el PC)

| ID | Qué se verifica | Procedimiento | Valor esperado | Criterio | Resultado |
|---|---|---|---|---|---|
| 1.5 | Eco de 1 byte | Enviar `0x41` desde la terminal en modo hexadecimal | Regresa `0x41` | 20 de 20 correctos | |
| 1.6 | Los 256 valores | Enviar los bytes `0x00` a `0xFF` | Regresan idénticos | 256 de 256, sin pérdidas ni cambios | |
| 1.7 | Ráfaga continua | Enviar 1000 bytes seguidos (archivo de la terminal o script de Python) | Regresan 1000 bytes. Duración mínima 86,8 ms (a 115200) | 1000 de 1000, sin overrun ni framing | |
| 1.8 | Ráfaga larga | Enviar 100 000 bytes pseudoaleatorios y comparar | Idéntico | 0 errores. Si hay errores, medir con `OSCTUNE` (ver especificación §2.2) | |
| 1.9 | Desbordamiento del buffer | Enviar más bytes de los que caben en el FIFO de RX sin que el programa principal los lea | Se activa el contador de errores. El PIC no se bloquea | `tx_DAT` devuelve `false` si el FIFO de TX está lleno. Sin reinicios | |

### 1C. Tiempos y ejecución no bloqueante (pin RC2 con osciloscopio)

| ID | Qué se verifica | Procedimiento | Valor esperado | Criterio | Resultado |
|---|---|---|---|---|---|
| 1.10 | Duración de la ISR de RX | Pulso en RC2 durante la ISR al recibir un byte | Anotar (objetivo en la especificación: < 100 ciclos = 25 µs) | ≤ 25 µs **[PROPUESTA]** | |
| 1.11 | Duración de la ISR de TX | Igual, durante la transmisión de un byte | Anotar | ≤ 25 µs **[PROPUESTA]** | |
| 1.12 | El programa principal no se bloquea | Mientras se transmiten 1000 bytes, el LED de RA5 sigue parpadeando a 1 Hz | Parpadeo sin interrupciones | Sin pausas visibles ni alteración del período | |
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

## Módulo 2. Protocolo de tramas (Fase 1b)

Trama de 9 bytes: `A5 5A | TYPE | SEQ | PAYLOAD(4) | CRC8`. Un PING válido debe responderse con un PONG en 781 µs de transmisión.

| ID | Qué se verifica | Procedimiento | Valor esperado | Criterio | Resultado |
|---|---|---|---|---|---|
| 2.1 | CRC-8 contra el PC | Calcular el CRC de 3 tramas a mano o en Python (polinomio `0x07`, sobre los bytes 2 a 7) y compararlo con el que genera el PIC | Iguales | 3 de 3 | |
| 2.2 | PING válido | Enviar un PING con payload conocido | PONG con el mismo `SEQ` y payload, CRC correcto | 20 de 20 | |
| 2.3 | CRC alterado | Cambiar un bit del CRC | El PIC responde ERROR (código 1) | 10 de 10, y el PIC sigue funcionando | |
| 2.4 | Tipo desconocido | Enviar `TYPE = 0x55` con CRC correcto | ERROR (código 2) | 10 de 10 | |
| 2.5 | Basura antes de una trama | Enviar bytes aleatorios y después un PING | Resincroniza y responde al PING | 20 de 20 | |
| 2.6 | Trama partida | Enviar la trama en dos partes con una pausa de 50 ms | Responde al PING completo | 10 de 10 | |
| 2.7 | Tramas seguidas | 1000 PING consecutivos con `SEQ` creciente | 1000 PONG con `SEQ` correlativo | Sin pérdidas ni repetidos | |
| 2.8 | Latencia | Medir con el osciloscopio el tiempo entre el último bit del PING y el primer bit del PONG | Anotar | ≤ 2 ms **[PROPUESTA]** | |

---

## Módulo 3. PWM y filtro RC (Fase 2)

PWM6 en RA2 (I0) y PWM7 en RA4 (Q0). Timer2 con `T2PR = 255` a Fosc/4: período 64 µs (15,625 kHz), 10 bits de resolución. Salida DC ideal: `V = VDD × duty`.

| ID | Qué se verifica | Procedimiento | Valor esperado | Criterio | Resultado |
|---|---|---|---|---|---|
| 3.1 | Frecuencia | Osciloscopio en la salida del PIC (antes del filtro) | 15,625 kHz (64 µs) | ±2 % (oscilador interno) | |
| 3.2 | Duty vs. registro | Programar 0 %, 25 %, 50 %, 75 % y 100 %; medir el ancho del pulso | Ancho = duty × 64 µs | Error ≤ 1 paso de 10 bits (62,5 ns) | |
| 3.3 | Voltaje DC tras el filtro | Con los mismos duty, medir con el multímetro la salida del filtro RC (antes del buffer) | `V = 3,7 × duty` | ±1 % de VDD (±37 mV) **[PROPUESTA]** | |
| 3.4 | Linealidad | 11 puntos de 0 % a 100 % (cada 10 %). Graficar V medido contra V ideal | Recta | Error máximo ≤ 1 % de VDD **[PROPUESTA]** | |
| 3.5 | Rizado | Osciloscopio en AC en la salida del filtro, con duty de 50 % (peor caso) | Anotar mV pico a pico | ≤ 1 LSB del ADC (0,9 mV) **[PROPUESTA]**. Si no cumple, subir RC o filtrar más | |
| 3.6 | Tiempo de establecimiento | Escalón de duty de 25 % a 75 % y medir hasta llegar al 99 % del valor final | ≈ 4,6 × RC | Menor que el tiempo máximo del lazo de auto-cero (definir con el profesor) | |
| 3.7 | Resolución | Cambiar el duty en 1 paso y medir el cambio de voltaje con el multímetro | 3,6 mV por paso (3,7 V / 1024) | Se distingue el cambio. Anotar el voltaje real | |
| 3.8 | Independencia entre canales | Cambiar PWM6 y verificar que PWM7 no se mueva | Sin cambio | Variación de PWM7 ≤ 1 mV | |
| 3.9 | Efecto del buffer | Medir el voltaje antes y después del buffer | Iguales | Diferencia ≤ 5 mV | |

Datos que faltan: valores reales de R y C del filtro en la placa (para calcular el rizado y el tiempo de establecimiento).

---

## Módulo 4. ADC: canales I y Q (Fase 2)

ADC de 12 bits con referencia VDD = 3,7 V: 1 LSB = 3,7 V / 4096 = 0,903 mV. La señal en reposo debe quedar cerca de Vref2 (1,85 V), que corresponde al código 2048.

| ID | Qué se verifica | Procedimiento | Valor esperado | Criterio | Resultado |
|---|---|---|---|---|---|
| 4.1 | Código en reposo | Entrada I y Q conectadas a Vref2 (1,85 V medidos con el multímetro) | Código ≈ 2048 | 2048 ± 10 códigos **[PROPUESTA]** | |
| 4.2 | Conversión contra voltaje | Aplicar 0,5 V, 1,0 V, 1,85 V, 2,5 V, 3,2 V desde una fuente, medir cada uno con el multímetro y comparar con el código | Código = V / 3,7 × 4096 | Error ≤ 5 LSB en cada punto **[PROPUESTA]** | |
| 4.3 | Ruido | Entrada fija; tomar 1000 muestras y calcular la desviación estándar | Anotar | σ ≤ 2 LSB **[PROPUESTA]** | |
| 4.4 | Canales independientes | Variar solo I y verificar que Q no cambie, y al revés | Sin cambio en el otro canal | Variación ≤ 3 LSB | |
| 4.5 | Frecuencia de muestreo | Pulso en RC2 en cada muestra (disparo por Timer) | Período constante | Error de período ≤ 1 % | |
| 4.6 | Señal de 20 Hz | Inyectar una senoidal de 5 Hz y otra de 20 Hz con un generador, y capturar la forma por UART | Amplitud y frecuencia correctas | Frecuencia ± 2 %, amplitud ± 5 % **[PROPUESTA]** | |
| 4.7 | Aliasing | Inyectar una señal por encima de fs/2 | Se atenúa por el filtro de 20 Hz | La señal medida está por debajo de 3 LSB | |
| 4.8 | Tiempo de adquisición | Cambiar la fuente de alta a baja impedancia y comparar | Mismo código | Diferencia ≤ 2 LSB. Si no, aumentar el tiempo de adquisición (errata ADCC 1.1.2) | |

---

## Módulo 5. Pulsador y lazo de auto-cero (Fase 2)

| ID | Qué se verifica | Procedimiento | Valor esperado | Criterio | Resultado |
|---|---|---|---|---|---|
| 5.1 | Antirrebote | Presionar el botón 50 veces | Un solo evento por pulsación | 50 de 50 | |
| 5.2 | Disparo por comando | Enviar el comando de auto-cero por UART | Inicia la calibración | Responde con una trama de aceptación | |
| 5.3 | Convergencia | Introducir un desbalance (mover un trimmer o inyectar una tensión) y ejecutar el auto-cero | I y Q llegan cerca de 2048 | diferencia con 2048 de 3 LSB o menos en ambos canales **[PROPUESTA]** | |
| 5.4 | Tiempo de calibración | Cronometrar desde el inicio hasta el fin | Anotar | ≤ 5 s **[PROPUESTA]** | |
| 5.5 | Repetibilidad | Repetir la calibración 10 veces con el mismo desbalance | Mismo duty final | Variación ≤ 2 pasos | |
| 5.6 | Saturación | Desbalance fuera del rango alcanzable | El PIC entra en ERROR y avisa al PC | No se queda en un bucle infinito | |
| 5.7 | Estabilidad posterior | Dejar 10 min en reposo | Código estable | Deriva ≤ 5 LSB **[PROPUESTA]** | |

---

## Módulo 6. Integración con el PC (Fase 2)

| ID | Qué se verifica | Procedimiento | Valor esperado | Criterio | Resultado |
|---|---|---|---|---|---|
| 6.1 | Recepción en LabVIEW o Python | Conectar y leer tramas de datos con I y Q | Valores iguales a los de la terminal | Coinciden en 100 muestras | |
| 6.2 | Plano de impedancia | Con la sonda sobre un tubo sano (en reposo) | Punto o nube pequeña cerca del origen | Nube de radio ≤ 5 LSB **[PROPUESTA]** | |
| 6.3 | Defecto | Pasar la sonda por un defecto conocido | Trayectoria clara fuera de la nube | Se distingue del ruido | |
| 6.4 | Indicador | Sano / dañado en el panel | Verde en reposo, rojo con defecto | 10 de 10 | |
| 6.5 | Tasa de datos | 200 muestras/s durante 10 min | 120 000 tramas sin pérdidas | 0 tramas perdidas (contadores de `SEQ`) | |
| 6.6 | Carga del enlace | Calcular la ocupación | 200 × 781 µs = 15,6 % | Menor del 50 % | |

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
