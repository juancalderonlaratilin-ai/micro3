# Criterios de diseño y valores escogidos

Proyecto Micro 3 · Enlace UART PC ↔ PIC (Fase 1) · Fecha: 2026-10-16

> **Para qué sirve:** explicar **por qué** se escogió cada valor (reloj, baud, formato, pines, niveles, buffers, protocolo), qué alternativas se descartaron y qué falta confirmar. El detalle técnico y las páginas del datasheet están en `especificacion_diseno_uart.md` y `notas_pic16f18426.md`. Las pruebas están en `protocolos_prueba.md`.
>
> **Etiquetas:** **[VERIFICADO]** = sale de un documento de `docs/` o de un cálculo comprobado. **[PROPUESTA]** = decisión de diseño por confirmar con el profesor.

---

## 1. Resumen de lo escogido

| # | Parámetro | Valor escogido | Estado |
|---|---|---|---|
| 1 | Microcontrolador | PIC16F18426 (14 pines, 28 KB flash, 2 KB RAM) | decidido |
| 2 | Estilo de código | C "bare-metal" sobre registros. Sin MCC | regla de la práctica |
| 3 | Reloj | HFINTOSC interno a **16 MHz**, sin cristal | [PROPUESTA] |
| 4 | Baud rate | **115200** (`SP1BRG = 34`, error −0,79 %) | [PROPUESTA] |
| 5 | Formato de byte | **8N1** (8 datos, sin paridad, 1 parada) | [PROPUESTA] |
| 6 | Protocolo | **Binario**, tramas fijas de 9 bytes con CRC-8 | [PROPUESTA] |
| 7 | Buffers | Circulares (cabeza y cola): RX de 32 bytes, TX de 64 bytes. Motivos en §6.1 | [PROPUESTA] |
| 8 | Interrupciones | RX siempre activa. TX solo mientras haya datos. Una sola ISR | [VERIFICADO] |
| 9 | Pines UART | TX = RC4 (pin 6), RX = RC5 (pin 5) | [PROPUESTA] |
| 10 | Nivel de entrada de RX | TTL (`INLVLC5 = 0`) | [VERIFICADO] |
| 11 | Alimentación | 3,7 V (fuente Keysight E36233A, según el preinforme) | confirmado |
| 12 | Adaptador USB-serie | Módulo CP2102 (TTL) con el nivel lógico en 3,3 V | por confirmar el módulo |
| 13 | Conexión PIC → adaptador | Divisor 1 kΩ + 2,2 kΩ en el TX del PIC | [PROPUESTA] |
| 14 | Visualización I vs Q | En el PC (LabVIEW o Python), por UART | decidido |
| 15 | Referencia del ADC (Fase 2) | VDD = 3,7 V | por confirmar en el capítulo del ADC |

---

## 2. Reloj

**Criterios usados**
1. **Error del baud** (el que más pesa). El baud sale de dividir el reloj por un entero, así que no todos los relojes dan todos los bauds.
2. **Carga de la CPU.** En la Fase 2 solo se muestrean 2 señales de ~20 Hz: cualquier reloj de 4 MHz o más sobra.
3. **Consumo.** Menos MHz, menos corriente (la placa puede ir con batería).
4. **Exactitud del oscilador.** El interno tiene ±2 % nominal. Un cristal sería más exacto, pero ocuparía RA4 y RA5, que ya están asignados.
5. **Compatibilidad con PWM y ADC** de la Fase 2 (ambos salen del mismo reloj).

**Alternativas**

| Opción | Por qué se descarta o se acepta |
|---|---|
| 1 MHz (arranque por defecto) | Da +8,5 % de error a 115200: no funciona |
| 4 MHz | −3,6 % a 115200: demasiado |
| 8 MHz | +2,1 % a 115200, a 57600 sale −0,79 %: usable, pero con menos margen en PWM |
| **16 MHz** | **−0,79 % a 115200. Elegido** |
| 32 MHz | +0,64 %. Mejor error, pero más consumo. Queda como reserva si hay errores de framing |
| Cristal externo | Ocupa 2 pines. No se necesita |

**Cómo se llega a 16 MHz:** `RSTOSC = 110` (arranca a 1 MHz) y el código sube a 16 MHz con `OSCFRQ` y `OSCCON1`, esperando `OSCCON3[ORDY]`. El código de MCC generado en clase queda a 1 MHz (`OSCCON1 = 0x62`, `OSCFRQ = 0x02`), que no sirve para 115200.

---

## 3. Baud rate

**Fórmula (BRG16 = 1, BRGH = 1):** `SP1BRG = Fosc / (4 × Baud) − 1`. A 16 MHz y 115200: 33,72 → **34**, baud real 114 286, error −0,79 %. [VERIFICADO: DS Tabla 28-2]

**Error de baud por reloj**

| Reloj | 9600 | 19200 | 57600 | 115200 |
|---|---|---|---|---|
| 1 MHz | +0,16 % | +0,16 % | +8,51 % | +8,51 % |
| 4 MHz | +0,16 % | +0,16 % | +2,12 % | −3,55 % |
| 8 MHz | +0,16 % | +0,16 % | −0,79 % | +2,12 % |
| **16 MHz** | −0,08 % | +0,16 % | +0,64 % | **−0,79 %** |
| 32 MHz | +0,04 % | −0,08 % | −0,08 % | +0,64 % |

**Cuánto error aguanta la UART.** El receptor se sincroniza con el flanco del bit de inicio y muestrea cada bit a su mitad. El bit de parada está a 9,5 bits del flanco, y el desfase acumulado debe ser menor que medio bit: 0,5 / 9,5 = **±5,3 %** como límite teórico, sumando los dos lados. En la práctica se reserva la mitad (±2 a 3 %). Con −0,79 % del baud más ±2 % del oscilador interno, el peor caso es ≈ −2,8 %: funciona, pero con poco margen. Si fallara, se ajusta `OSCTUNE` (método preferido del datasheet) o se sube a 32 MHz.

**Criterios para escoger 115200**

| Baud | µs por byte | Bytes/s | Uso de la línea con 1800 B/s |
|---|---|---|---|
| 9600 | 1041,7 | 960 | **188 %: no cabe** |
| 19200 | 520,8 | 1920 | 94 %: sin margen |
| 57600 | 173,6 | 5760 | 31 % |
| **115200** | **86,8** | **11 520** | **16 %** |

- 1800 B/s = 200 muestras/s × 9 bytes por trama.
- 115200 es un baud **estándar**: lo aceptan todas las terminales y adaptadores. Un valor raro (como los 125 000 que da el código MCC de clase) solo lo aceptan algunos.
- A 115200 la ISR tiene 86,8 µs (≈347 ciclos) por byte. Sobra para una ISR corta.
- **Plazo de RX:** el receptor tiene solo 2 bytes de FIFO. Si llega completo un tercer byte sin que se haya leído `RC1REG` hay overrun. A 115200 son unos 174 µs.

---

## 4. Formato de byte: 8N1

```
reposo ‾‾‾|START|D0|D1|D2|D3|D4|D5|D6|D7|STOP|‾‾‾ reposo
           (0)   LSB primero                (1)
```

| Elemento | Escogido | Por qué |
|---|---|---|
| Bits de datos | 8 | El protocolo es binario y necesita el byte completo |
| Paridad | ninguna | Detecta solo errores de 1 bit y cuesta 10 % de la línea. Con el CRC-8 de la trama se detecta mucho mejor |
| Bits de parada | 1 | Suficiente. Con 2 se da más tiempo a un receptor lento, aquí no hace falta |

---

## 5. Protocolo

| Decisión | Valor | Por qué |
|---|---|---|
| Representación | Binario, no ASCII | Lo pidió el profesor: el ASCII es pesado para el procesador (convertir números a texto) |
| Longitud | Tramas fijas de 9 bytes | Más simple de procesar que longitud variable |
| Formato | `A5 5A`, `TYPE`, `SEQ`, `PAYLOAD` (4 bytes), `CRC8` | Dos bytes de cabecera para resincronizar. `SEQ` permite detectar pérdidas |
| Datos I y Q | 2 bytes por canal, 12 bits cada uno | I + Q = 4 bytes = el campo `PAYLOAD` |
| Verificación | CRC-8 (polinomio `0x07`) sobre los bytes 2 a 7 | Detecta errores y corrupción de bytes |
| Duración de una trama | 9 × 86,8 µs = 781 µs | Con 200 muestras/s: 15,6 % de la línea |
| Etapas | 1a: eco crudo. 1b: ping/pong con CRC | Se prueba primero el canal físico y luego la lógica |

**Alternativas que quedan abiertas:** XOR en lugar de CRC-8 (más simple, detecta menos), y la lista final de tipos de trama.

---

## 6. Buffers e interrupciones

**Buffer circular (lo que pide el profesor):** un arreglo con índices `head` (donde se escribe) y `tail` (de donde se lee). Es el mismo esquema que el del código MCC generado en clase.

| Decisión | Valor | Por qué |
|---|---|---|
| Tamaño | Potencia de 2 (RX 32, TX 64) | El índice se enmascara sin dividir |
| Buffer lleno | `head + 1 == tail` | Sin contador extra |
| Quién escribe | RX: la ISR escribe en `head` y `main` lee de `tail`. TX: `main` escribe en `head` y la ISR lee de `tail` | Cada índice lo modifica un solo lado |
| `tx_DAT` y `tx_string` | Devuelven `false` si no cabe | No bloquean |
| ISR | Una sola (el PIC16 tiene un único vector, `0x0004`). Se comprueba bandera **y** habilitación de cada fuente | Sin prioridades |
| `TX1IE` | Solo se activa cuando hay datos y se apaga al vaciar | `TX1IF` vale 1 siempre que `TX1REG` esté vacío, aunque `TX1IE` sea 0: dejarlo activo siempre hace que la ISR se repita sin parar |

### 6.1 Tamaño de los buffers: RX 32 bytes, TX 64 bytes [PROPUESTA]

**Decisión:** FIFO de recepción de 32 bytes y de transmisión de 64 bytes, ambas circulares con índices `head` y `tail` de 8 bits. El profesor no fijó tamaños en lo que tenemos; el MCC de clase usó 8 por defecto. Estos valores son nuestros y se pueden revisar.

**Datos de partida** (a 115200 baud, 8N1 = 10 bits por byte):

| Dato | Valor |
|---|---|
| Tiempo por byte | 86,8 µs |
| Trama (9 bytes) | 781 µs |
| Capacidad útil de un buffer de N bytes | N − 1 (se considera lleno cuando `head + 1 == tail`, sin contador extra) |

| Buffer | Tamaño | Capacidad útil | Tramas de 9 B que caben | Tiempo de línea que cubre |
|---|---|---|---|---|
| MCC de clase (RX y TX) | 8 | 7 | 0 | 0,6 ms |
| **RX** | **32** | 31 | 3 | 2,7 ms |
| **TX** | **64** | 63 | 7 | 5,5 ms |

**Por qué 32 en RX**
- El PC solo manda órdenes cortas (una trama de 9 bytes, o pocas seguidas). Casi siempre el buffer está vacío.
- Lo que hay que cubrir es el tiempo que el programa principal pueda tardar en volver a `parser_task()`. Con 31 bytes útiles son 2,7 ms de margen. Con los 8 bytes del MCC (7 útiles) serían 0,6 ms, menos que una sola trama (0,78 ms): cualquier tarea un poco larga en `main` perdería bytes.
- Aquí el buffer es el segundo escalón: el hardware solo tiene una FIFO de 2 bytes, y la ISR debe vaciarla en menos de unos 174 µs (ver la especificación).

**Por qué 64 en TX**
- El PIC es el que más habla. En la Fase 2 serán 9 bytes por muestra, a unas 200 muestras por segundo (1800 B/s, el 16 % de la línea; es una propuesta de velocidad de muestreo, no un dato confirmado). Entre muestras pasan 5 ms y una trama tarda 0,78 ms en salir.
- Una respuesta (por ejemplo PONG) puede coincidir con tramas de datos. 63 bytes útiles son 7 tramas completas: aguanta ráfagas de varias tramas encoladas de golpe sin devolver `false`.
- Vaciar el buffer completo toma 5,5 ms, casi un periodo de muestreo: aunque se llene, los datos no quedan atrasados más de eso.

**Por qué potencia de 2**
- El índice se envuelve con `idx = (idx + 1) & (N - 1)`: una operación. Con un tamaño cualquiera habría que usar `%`, que en un PIC16 sin divisor por hardware lo resuelve una rutina de software más lenta, y esto corre dentro de la ISR.
- Los índices de 8 bits se leen y escriben en una sola instrucción, así que no se corrompen si la ISR interrumpe a `main` a mitad de lectura.

**Por qué no más grandes**
- Cuestan RAM: 32 + 64 = 96 bytes de 2048 (4,7 %). Sobra margen, pero la Fase 2 también necesita datos de I y Q, variables del auto-cero y el parser.
- Un buffer más grande solo esconde un problema de tiempo: si `main` tarda tanto que se llena, el defecto es la tarea larga, no el buffer.

**Qué pasa cuando se llena**

| Buffer | Comportamiento |
|---|---|
| RX lleno | La ISR descarta el byte nuevo y marca la bandera de overflow (error 3 del protocolo). No se pisa un dato sin leer |
| TX lleno | `tx_DAT` y `tx_string` devuelven `false`. Quien llama decide: reintentar o descartar la muestra y contarla |

**Cuándo revisar el tamaño:** si la trama pasa de 9 bytes, si sube la frecuencia de muestreo, o si una prueba mide que `main` tarda más de 2,7 ms sin atender el RX. Regla: TX ≥ (tramas por ráfaga × bytes por trama) + 1; RX ≥ (tiempo máximo sin leer ÷ 86,8 µs) + 1.

**Cómo comprobarlo:** prueba de ráfaga en `docs/protocolos_prueba.md` (módulo 1): mandar tramas seguidas desde el PC y contar bytes perdidos y errores de overflow.

**Diferencia con el MCC de clase:** sus buffers son de 8 bytes y `EUSART1_Read` y `EUSART1_Write` tienen bucles `while` que esperan, algo prohibido por la regla del proyecto.

---

## 7. Pines y niveles lógicos

**Presupuesto:** se usan 8 pines (TX, RX, LED, I, Q, PWM de I0, PWM de Q0, pulsador) y sobra 1 (RC2, pin de prueba). RA0, RA1 y RA3 se dejan para el programador SNAP. Detalle en la §3 de la especificación.

**Niveles con VDD = 3,7 V**

| Señal | Cálculo | Conclusión |
|---|---|---|
| RX del PIC (entrada Schmitt) | VIH = 0,8 × VDD = 2,96 V. El CP2102 a 3,3 V entrega VOH mínimo de 2,9 V | No cumple en el peor caso |
| RX del PIC (entrada **TTL**) | VIH = 0,25 × VDD + 0,8 = 1,73 V | **Cumple con margen. Se usa TTL** |
| TX del PIC → adaptador | El CP2102 acepta como máximo VIO + 0,5 V = 3,8 V. El PIC entrega ≈3,7 V | Entra por 0,1 V. **Se pone divisor** |
| Divisor 1 kΩ + 2,2 kΩ | 3,7 × 2,2 / 3,2 = **2,54 V**. VIH mínimo del adaptador: 1,9 V | Cumple. La constante de tiempo es ~14 a 34 ns, muy menor que un bit (8,75 µs) |

Fuente de los niveles del CP2102: datasheet del CP2102-GMR (Silicon Labs), sección 6.1 (valores máximos absolutos) y 6.2 (parámetros eléctricos). El módulo comprado puede llevar otra variante del chip. Hay que verificar el marcado.

---

## 8. Circuito

```
 PC ──USB──► [Módulo USB-TTL CP2102]                 [PIC16F18426]
                  TXD ──────────────────────────────► RC5 (pin 5, RX)
                  RXD ◄──┬──────[ R1 1 kΩ ]───────────  RC4 (pin 6, TX)
                         │
                       [R2 2,2 kΩ]
                         │
                  GND ───┴────────────────────────────  VSS (pin 14)

   Fuente +3,7 V ──┬── VDD (pin 1) ── 100 nF a GND (junto a los pines 1 y 14)
                   └── 10 kΩ ── RA3/MCLR (pin 4) ── al SNAP
   Módulo: el jumper de nivel (si lo tiene) en 3,3 V. 3V3, 5V, RTS y CTS sin conectar
```

**Lista de materiales**

| Cantidad | Parte | Uso |
|---|---|---|
| 1 | Módulo USB-TTL CP2102 | Enlace con el PC |
| 1 | Resistencia 1 kΩ | R1, en serie con el TX del PIC |
| 1 | Resistencia 2,2 kΩ | R2, a GND (divisor) |
| 1 | Resistencia 10 kΩ | Pull-up de MCLR |
| 1 | Capacitor 100 nF | Desacople de VDD |
| – | Cables | Conexiones |

**Reglas**
1. TXD del adaptador va al RX del PIC, y el TX del PIC al RXD del adaptador. **Nunca TX con TX.**
2. **GND común** entre el adaptador, el PIC y la fuente.
3. **No conectar** los pines 5V o 3V3 del módulo al PIC: el PIC lo alimenta la fuente de 3,7 V.
4. Antes de conectar: con el multímetro, el TXD del adaptador en reposo debe marcar ≈3,3 V (línea en alto). Si marca 5 V, el módulo está en 5 V y hay que bajar el jumper a 3,3 V antes de conectarlo al PIC.
5. En el PC: puerto COM del adaptador a 115200, 8 bits, sin paridad, 1 bit de parada, sin control de flujo.
6. **Prueba previa (loopback):** unir TXD con RXD del adaptador y enviar caracteres desde la terminal. Si regresan, el adaptador y su driver funcionan.

---

## 9. Pendientes por confirmar

1. Qué baud pidió el profesor en clase (115200 contra 125 000 del código de MCC).
2. Si el entregable final puede usar MCC.
3. Marcado exacto del chip del módulo CP2102 y si tiene jumper de nivel.
4. Que el chip del PIC sea el `PIC16F18426` y no el `PIC16LF18426` (máximo 3,6 V).
5. Que el PIC se alimente de los 3,7 V de la placa.
6. ~~CRC-8 o XOR~~ (resuelto el 2026-10-21: CRC-8, ver §10). Falta la lista final de tipos de trama.
7. Revisión de silicio (MPLAB X → Dashboard).
8. Valor `VBOR` del brown-out (el MCC de clase usó `BORV = LO`, 2,45 V).

---

## 10. Cambios y correcciones

| Fecha | Cambio | Motivo |
|---|---|---|
| 2026-10-16 | PWM de auto-cero: de PWM3/PWM4 a **PWM6/PWM7** | Este chip no tiene PWM3 ni PWM4 |
| 2026-10-16 | `LVP` queda **habilitado** | RA3 es MCLR. Con `LVP = 0` hace falta alto voltaje en MCLR para programar |
| 2026-10-16 | Flash 28 KB y RAM 2 KB (antes 14 KB y 1 KB) | Valores del datasheet |
| 2026-10-16 | `RX1PPS` es `RX1DTPPS` en el `.h` de XC8 | Visto en el código generado por MCC |
| 2026-10-16 | TX del PIC pasa por un divisor | El datasheet del CP2102-GMR no describe sus entradas como tolerantes a 5 V, y el máximo es VIO + 0,5 V. Antes se suponía que sí toleraban 5 V |
| 2026-10-19 | Documentados los motivos del tamaño de los buffers (§6.1): RX 32, TX 64, potencia de 2, comportamiento al llenarse | La revisión de `eusart1.c` de MCC mostró que 8 bytes no cubren una trama y que su RX no controla el desborde |
| 2026-10-21 | Verificación de tramas con **CRC-8** (polinomio `0x07`, inicial `0x00`, sin reflejo, sobre los bytes 2 a 7). Se descarta el XOR. Formato en `docs/especificacion_protocolo.md` | Decisión de la persona. Cierra el pendiente 6 de §9 en lo que toca al CRC |
| 2026-10-21 | El PWM de auto-cero usa los **10 bits directos** (sin dithering ni divisor); el residuo de hasta ±20 cuentas se resta después por software. Detalle y motivos en `docs/especificacion_pwm.md` §2 y §9 (D-1) | Decisión de la persona; paso de 3,61 mV en I0 y 36 mV en la salida |
| 2026-10-21 | El cálculo del filtro RC del PWM (valores de R y C) se **aplaza** | Decisión de la persona; T-06 no lo necesita para empezar |
