# Criterios de diseño y valores escogidos

Proyecto Micro 3 · Enlace UART PC ↔ PIC (Fase 1) · Fecha: 2026-10-16 · Actualizado: 2026-11-05 (coherencia con el código: PWM, Timer0, sin anti-aliasing)

> **Para qué sirve:** explicar **por qué** se escogió cada valor (reloj, baud, formato, pines, niveles, buffers, formato de envío), qué alternativas se descartaron y qué falta confirmar. El detalle técnico y las páginas del datasheet están en `especificacion_diseno_uart.md` y `notas_pic16f18426.md`. Las pruebas están en `protocolos_prueba.md`.
>
> **Etiquetas:** **[VERIFICADO]** = sale de un documento de `docs/` o de un cálculo comprobado. **[PROPUESTA]** = decisión de diseño por confirmar con el profesor.

---

## 1. Resumen de lo escogido

| # | Parámetro | Valor escogido | Estado |
|---|---|---|---|
| 1 | Microcontrolador | PIC16F18426 (14 pines, 28 KB flash, 2 KB RAM) | decidido |
| 2 | Estilo de código | C sobre registros, con el código de MCC como base (revisado en `docs/revision_codigo_mcc.md`) | la persona (2026-10-19) |
| 3 | Reloj | HFINTOSC interno a **16 MHz**, sin cristal | [PROPUESTA] |
| 4 | Baud rate | **115200** (`SP1BRG = 34`, error −0,79 %) | [PROPUESTA] |
| 5 | Formato de byte | **8N1** (8 datos, sin paridad, 1 parada) | [PROPUESTA] |
| 6 | Formato de envío | **Texto ASCII**: una línea `xxxx,xxxx\r\n` (I,Q) cada 2 pares = 500 líneas/s. El PIC no recibe comandos | decisión de la persona (2026-11-04) |
| 7 | Buffers | Circulares (cabeza y cola): RX de 32 bytes, TX de 64 bytes. Motivos en §6.1 | [PROPUESTA] |
| 8 | Interrupciones | RX siempre activa. TX solo mientras haya datos. Una sola ISR | [VERIFICADO] |
| 9 | Pines UART | TX = RC4 (pin 6), RX = RC5 (pin 5) | [PROPUESTA] |
| 10 | Nivel de entrada de RX | TTL (`INLVLC5 = 0`) | decidido, **sin aplicar**: **el código actual lo deja en Schmitt** (`INLVLC = 0x3F` en `pin_manager.c`; VIH = 2,96 V) |
| 11 | Alimentación | 3,7 V (fuente Keysight E36233A, según el preinforme) | confirmado |
| 12 | Adaptador USB-serie | Módulo CP2102 (TTL) con el nivel lógico en 3,3 V | por confirmar el módulo |
| 13 | Conexión PIC → adaptador | Divisor 1 kΩ + 2,2 kΩ en el TX del PIC | [PROPUESTA] |
| 14 | Visualización I vs Q | En el PC (LabVIEW o Python), por UART | decidido |
| 15 | Referencia del ADC (Fase 2) | VDD = 3,7 V (`ADREF = 0x00`) | en el código; falta confirmar con el profesor |
| 16 | Muestreo del ADC | 1 kHz, disparado por Timer0 (hardware); I y Q convertidos en ráfaga dentro de 1 ms: 1000 pares/s, 1 kHz por canal | profesor; código T-09 sin probar. Motivos en §11 |
| 17 | Promedio del ADC | **ninguno**: una muestra por par (el PC recibe 1 de cada 2: 500 pares/s) | decisión de la persona (2026-11-03); reemplaza D-1 |
| 18 | Canales del ADC | I = RC0 y Q = RC1, un solo ADC multiplexado | decidido |
| 19 | **SINAD del ADC** | **> 60 dB** (ENOB > 9,67 bits) | requisito de la persona (2026-10-30) |
| 20 | Timer0 | 8 bits, Fosc/4, 1:16, `TMR0H` = 249 → 1 ms; dispara el ADC (`ADACT` = 0x02); `TMR0_out` en RC2 (cuadrada de 500 Hz) | código T-09 |
| 21 | PWM | Timer2 a Fosc/4, `T2PR` = 255 → 15,625 kHz, 10 bits; PWM6 en RA2 (I) y PWM7 en RA4 (Q); `duty = (código + 2) >> 2` con cada par (1000/s) | cambio de enfoque del profesor (2026-11-04); código T-06 sin probar |
| 22 | Filtro anti-aliasing | **ninguno** en esta entrega | indicación de la persona (2026-11-05) |

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

**Cómo se llega a 16 MHz:** `RSTOSC = 110` (HFINT1: arranca a 1 MHz) y `OSCILLATOR_Initialize()` de `mcc.c` sube a 16 MHz con `OSCFRQ = 0x05` y `OSCCON1 = 0x60` (NOSC = HFINTOSC, NDIV = 1); no espera `OSCCON3[ORDY]`. El primer MCC de clase quedaba a 1 MHz (`OSCCON1 = 0x62`, `OSCFRQ = 0x02`); el regenerado el 2026-10-19 ya trae los 16 MHz.

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

| Baud | µs por byte | Bytes/s | Uso de la línea con 5500 B/s |
|---|---|---|---|
| 9600 | 1041,7 | 960 | **573 %: no cabe** |
| 19200 | 520,8 | 1920 | **286 %: no cabe** |
| 57600 | 173,6 | 5760 | **95 %: sin margen** |
| **115200** | **86,8** | **11 520** | **48 %** |

- 5500 B/s = 500 líneas/s × 11 bytes (`4095,4095\r\n`). 115200 es el único baud estándar de la tabla en que cabe con margen. A 115200, 1000 líneas/s (11 000 B/s = 96 %) cabrían apenas: por eso se manda 1 de cada 2 pares (`docs/especificacion_protocolo.md` §2).
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
| Bits de datos | 8 | Es lo estándar; el texto ASCII usaría 7, pero todas las terminales y LabVIEW esperan 8 |
| Paridad | ninguna | Detecta solo errores de 1 bit y cuesta 10 % de la línea, que va al 48 %. Una línea dañada casi siempre pierde la forma `número,número` y el PC la descarta |
| Bits de parada | 1 | Suficiente. Con 2 se da más tiempo a un receptor lento, aquí no hace falta |

---

## 5. Formato de envío (texto ASCII)

| Decisión | Valor | Por qué |
|---|---|---|
| Representación | Texto ASCII | Decisión de la persona (2026-11-04): se lee directo en una terminal, en el Serial Plotter y en LabVIEW con *VISA Read* + *Scan From String*. El receptor de la trama binaria en LabVIEW resultó muy largo. Es lo que sugiere el enunciado |
| Línea | `xxxx,xxxx\r\n`: primero I, después Q, códigos de 0 a 4095 en decimal, sin nombres | Números separados por coma los entiende el Serial Plotter y se separan fácil en LabVIEW (`%d,%d`); sin nombres la línea es más corta (11 bytes en vez de 15). `\n` permite leer línea por línea |
| Longitud | hasta 11 bytes (11 con 4 cifras en los dos canales) | El fin de línea marca dónde termina; no hace falta largo fijo |
| Líneas por segundo | 500 (1 de cada 2 pares, `ADC_PARES_POR_LINEA` = 2) | Una línea tarda 0,96 ms: una por cada par de 1 ms ocuparía el 96 %, sin margen. Con 500/s la línea va al 48,1 % (cumple el < 50 % de la prueba 6.6) |
| Verificación | Solo la forma de la línea | No hay suma de verificación ni número de secuencia; con 8N1 sin paridad, un byte dañado casi siempre rompe la forma |
| Sentido | Solo PIC → PC | El PIC empieza a mandar al encender; no hay comandos |

**Lo que se pierde frente a una trama binaria:** fs = 500 Hz en el PC en vez de 1000, ninguna detección de líneas perdidas y ningún comando del PC al PIC (el control del PWM desde el PC, T-07, pasa a Macro). Detalle: `docs/especificacion_protocolo.md`.

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
| Línea `4095,4095\r\n` (11 bytes) | 0,96 ms |
| Capacidad útil de un buffer de N bytes | N − 1 (se considera lleno cuando `head + 1 == tail`, sin contador extra) |

| Buffer | Tamaño | Capacidad útil | Líneas de 11 B que caben | Tiempo de línea que cubre |
|---|---|---|---|---|
| MCC de clase (RX y TX) | 8 | 7 | 0 | 0,6 ms |
| **RX** | **32** | 31 | — (hoy no se usa) | 2,7 ms |
| **TX** | **64** | 63 | 5 | 5,5 ms |

**Por qué 32 en RX**
- Hoy el PIC no recibe comandos: el buffer de RX solo se llena si el PC manda algo, y nadie lo lee. Se deja el tamaño por si se agregan comandos de texto (T-07, ahora en Macro).
- Si se usa, lo que hay que cubrir es el tiempo que el programa principal pueda tardar en volver a leer. Con 31 bytes útiles son 2,7 ms de margen.
- Aquí el buffer es el segundo escalón: el hardware solo tiene una FIFO de 2 bytes, y la ISR debe vaciarla en menos de unos 174 µs.

**Por qué 64 en TX**
- El PIC es el que más habla: 11 bytes cada 2 ms (5500 B/s, el 48,1 % de la línea). Una línea tarda 0,96 ms en salir, así que al llegar la siguiente el buffer ya está vacío.
- 63 bytes útiles son 5 líneas completas: aguanta que `main` se atrase varias líneas sin que `tx_string` devuelva `false`.
- Si una ráfaga lo llenara, entran 5,5 B/ms y salen 11,4, así que se vacía a ≈ 5,9 B/ms: ≈ 11 ms [CÁLCULO].

**Por qué potencia de 2**
- El índice se envuelve con `idx = (idx + 1) & (N - 1)`: una operación. Con un tamaño cualquiera habría que usar `%`, que en un PIC16 sin divisor por hardware lo resuelve una rutina de software más lenta, y esto corre dentro de la ISR.
- Los índices de 8 bits se leen y escriben en una sola instrucción, así que no se corrompen si la ISR interrumpe a `main` a mitad de lectura.

**Por qué no más grandes**
- Cuestan RAM: 32 + 64 = 96 bytes de 2048 (4,7 %). Sobra margen, pero la Fase 2 también necesita datos de I y Q, las variables del PWM de reconstrucción.
- Un buffer más grande solo esconde un problema de tiempo: si `main` tarda tanto que se llena, el defecto es la tarea larga, no el buffer.

**Qué pasa cuando se llena**

| Buffer | Comportamiento |
|---|---|
| RX lleno | La ISR descarta el byte nuevo y marca la bandera de overflow. No se pisa un dato sin leer |
| TX lleno | `tx_DAT` y `tx_string` devuelven `false`. `adc_ascii_par` descarta la línea y la cuenta en `adcLineasPerdidas` |

**Cuándo revisar el tamaño:** si la línea crece, si se mandan más líneas por segundo, o si se agregan comandos y una prueba mide que `main` tarda más de 2,7 ms sin atender el RX. Regla: TX ≥ (líneas por ráfaga × bytes por línea) + 1; RX ≥ (tiempo máximo sin leer ÷ 86,8 µs) + 1.

**Cómo comprobarlo:** prueba 2.4 de `docs/protocolos_prueba.md`: 10 minutos a 500 líneas/s con `adcLineasPerdidas` en 0.

**Diferencia con el MCC de clase:** sus buffers son de 8 bytes y `EUSART1_Read` y `EUSART1_Write` tienen bucles `while` que esperan, algo prohibido por la regla del proyecto.

---

## 7. Pines y niveles lógicos

**Presupuesto:** se usan TX, RX, I, Q, PWM de I (RA2) y PWM de Q (RA4); RC2 saca la salida del Timer0 (pin de prueba). El LED y el pulsador de auto-cero eran del diseño anterior: el pulsador pasa a Macro (2026-11-04). RA0, RA1 y RA3 se dejan para el programador SNAP. Detalle en la §3 de la especificación.

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
2. Marcado exacto del chip del módulo CP2102 y si tiene jumper de nivel.
3. Que el chip del PIC sea el `PIC16F18426` y no el `PIC16LF18426` (máximo 3,6 V).
4. Que el PIC se alimente de los 3,7 V de la placa.
5. ~~Verificación de las tramas~~ (ya no aplica: desde el 2026-11-04 se manda texto ASCII, §5).
6. Revisión de silicio (MPLAB X → Dashboard).
7. Valor `VBOR` del brown-out (el MCC de clase usó `BORV = LO`, 2,45 V).
8. Con qué ADC y `Vref` calculó el profesor los 352 µV (corresponden a 12 bits y 5 V; con 3,7 V son 261 µV).
9. Cómo se mide el SINAD > 60 dB: sobre el ADC con una senoidal casi a escala completa (lo que se adoptó) o sobre la señal real de PCB2 (±185 mV, donde un 12 bits ideal da 54 dB), y a qué frecuencias.
10. Unidad de `ADACQ` (10, 40 o 320 µs; prueba 4.16) y duración de las ISR de la UART, que fija el jitter del disparo de Q.
11. Modelo e impedancia de salida (`Zout`) del opa de I y Q de PCB2 (datasheet del MCP604 en `docs/MCP601-2.PDF`, sin número de `Zout`) y si `Vref` se queda en `VDD` (`docs/especificacion_adc.md` §3.2 y §7.1).

---

## 10. Cambios y correcciones

| Fecha | Cambio | Motivo |
|---|---|---|
| 2026-10-16 | PWM de auto-cero: de PWM3/PWM4 a **PWM6/PWM7** | Este chip no tiene PWM3 ni PWM4 |
| 2026-10-16 | `LVP` queda **habilitado** | RA3 es MCLR. Con `LVP = 0` hace falta alto voltaje en MCLR para programar |
| 2026-10-16 | Flash 28 KB y RAM 2 KB (antes 14 KB y 1 KB) | Valores del datasheet |
| 2026-10-16 | `RX1PPS` es `RX1DTPPS` en el `.h` de XC8 | Visto en el código generado por MCC |
| 2026-10-16 | TX del PIC pasa por un divisor | El datasheet del CP2102-GMR no describe sus entradas como tolerantes a 5 V, y el máximo es VIO + 0,5 V. Antes se suponía que sí toleraban 5 V |
| 2026-10-19 | Documentados los motivos del tamaño de los buffers (§6.1): RX 32, TX 64, potencia de 2, comportamiento al llenarse | La revisión de `eusart1.c` de MCC mostró que 8 bytes son muy pocos y que su RX no controla el desborde |
| 2026-10-21 | Se escoge una trama binaria con suma de verificación para el enlace | Decisión de la persona. **Reemplazada el 2026-11-04** por el texto ASCII |
| 2026-10-21 | El PWM de auto-cero usa los **10 bits directos** (sin dithering ni divisor); el residuo de hasta ±20 cuentas se resta después por software. Detalle en `docs/macro_autocero_pwm.md` (pasa a Macro el 2026-11-04; los 10 bits se conservan en el PWM de reconstrucción) | Decisión de la persona; paso de 3,61 mV en I0 y 36 mV en la salida |
| 2026-10-21 | El cálculo del filtro RC del PWM (valores de R y C) se **aplaza** | Decisión de la persona; T-06 no lo necesita para empezar |
| 2026-10-30 | **SINAD del ADC > 60 dB** como requisito de diseño | Decisión de la persona. Condiciones de medición en `docs/especificacion_adc.md` §6 [PROPUESTA] |
| 2026-10-30 | Se documenta el ADC de T-09: muestreo de 1 kHz por Timer0 y promedio de 5 | El código ya existía (zip del 2026-10-30); se documentó lo que hace. No hubo cambio de diseño |
| 2026-11-03 | **Se quita el promedio del ADC** (antes 5 pares, D-1): cada par lleva una sola muestra de I y de Q | Decisión de la persona. Se pierde la mejora de ruido del promedio (hasta 7 dB) |
| 2026-11-04 | **Solo texto ASCII**: el PIC manda `xxxx,xxxx\r\n` cada 2 pares (500 líneas/s, 48,1 % de la línea) y no recibe comandos. Se borran `crc8.*` y `protocol.*`, se retira T-04 y `adc_prueba.*` pasa a `adc_ascii.*` (§5) | Decisión de la persona: el receptor de la trama binaria en LabVIEW era muy largo; el texto se lee con *VISA Read* + *Scan From String*. Costo: fs = 500 Hz en el PC, sin detección de líneas perdidas, sin comandos (T-07 por replantear) |
| 2026-11-04 | La línea pierde los nombres: de `I:2048,Q:2051\r\n` (15 bytes) a `2048,2051\r\n` (11 bytes; primero I, después Q). Con 500 líneas/s la línea pasa del 65,6 % al **48,1 %** | Decisión de la persona: ver solo los valores. Cumple el < 50 % de la prueba 6.6. El PC depende del orden de los números |
| 2026-11-05 | **Sin filtro anti-aliasing** en esta entrega: la prueba 4.7 y el criterio de Nyquist del enunciado (§3.2) quedan pendientes | Indicación de la persona. El ADC muestrea a 1 kHz (Nyquist 500 Hz) y el PC recibe 500 Hz (Nyquist 250 Hz) |
| 2026-11-04 | **Cambio de enfoque del profesor:** en Micro 3, un PWM por canal reproduce las muestras del ADC (`duty = (código + 2) >> 2`, PWM6 en RA2 y PWM7 en RA4, Timer2 con `T2PR` = 255) y un filtro pasa bajos de ≈ 100 Hz devuelve la señal analógica (`docs/especificacion_pwm.md`). El auto-cero con PWM (I0/Q0, pulsador, lazo) pasa a Macro (`docs/macro_autocero_pwm.md`) | Decisión del profesor. Se conservan los pines, el Timer2 y los 10 bits ya decididos |


---

## 11. ADC: criterios y valores escogidos (Fase 2)

> Cuentas, tiempos y pruebas: `docs/especificacion_adc.md`. Aquí solo el porqué de cada valor. **[VERIFICADO]** = datasheet, código o documento de `docs/`; **[CÁLCULO]** = cuenta propia comprobada; **[PROPUESTA]** = por aprobar.

| Decisión | Valor | Por qué | Alternativas descartadas |
|---|---|---|---|
| Referencia | `VDD` = 3,7 V | Cubre 0 a 3,7 V (los rieles de PCB2) sin recorte. `Vref2` sale de un divisor del mismo riel, así que el nivel de reposo es ratiométrico (TI Ec. 26). No usa pines: RA1/`ADCVREF+` es ICSPCLK | FVR de 4,096 V (no puede superar `VDD`, DS p.491); FVR de 2,048 V (recorta toda señal sobre 2,048 V; el rango lineal de 1,85 ± 0,185 V llega a 2,035 V, a 13 mV del recorte); `VDD` de 5 V (la placa trabaja a 3,7 V) |
| Muestreo | 1 kHz por canal (I y Q en cada disparo: 1000 pares/s); al PC llegan 500 pares/s | Pedido por el profesor. La señal útil está por debajo de ≈ 20 Hz: sobremuestreo de 25 [CÁLCULO]. **En esta entrega no hay filtro anti-aliasing** (persona, 2026-11-05): lo que llegue al pin por encima de 500 Hz se pliega en el ADC, y por encima de 250 Hz en el PC. La prueba 4.7 queda pendiente | – |
| Disparo | Timer0 por hardware (`ADACT`) | El instante de muestreo de I no depende del software (el jitter que importa es el del reloj de muestreo, no el de conversión). Sin esperas | `__delay_ms` o iniciar por software: prohibido y con jitter |
| I y Q | un ADC multiplexado, en ráfaga | Un solo convertidor. Sesgo entre canales de 0,5 % a 20 Hz si la adquisición es de 10 µs, 4,4 % si es de 320 µs [CÁLCULO] | Alternar canales a 2 kHz (sesgo de 500 µs, 6 %); ADC de muestreo simultáneo (innecesario) |
| Jitter de Q | por vigilar | Q lo arranca la ISR, así que hereda la variación de latencia de las ISR de la UART (hasta 25 µs ⇒ 61 dB a 20 Hz) [CÁLCULO] | Disparar Q por hardware (a decidir si la prueba 4.17 falla) |
| Promedio | ninguno (una muestra por par) | Decisión de la persona (2026-11-03). Cada línea trae la conversión cruda: sirve tal cual para el histograma y el SINAD del ADC. El PC recibe 1 de cada 2 pares (500 líneas/s) | 5 pares (lo de antes, D-1): bajaba el ruido hasta 7 dB si era blanco y ≥ 0,5 LSB, atenuaba 0,14 dB a 20 Hz y ocupaba 15,6 % de la línea |
| ISR del ADC | llamada directa desde `interrupt_manager.c` (C-11) | Mismo criterio que C-06: no se cambia la función en ejecución, y se ahorra el salto indirecto | Puntero `ADCC_ADI_InterruptHandler` de MCC |
| SINAD | > 60 dB | Requisito de la persona. Un 12 bits ideal da 74 dB a escala completa: 14 dB de margen para ruido, jitter, referencia y distorsión | – |
| Disparo del ADC | `ADACT` = `00010` (`TMR0_overflow`) | Periodo exacto por hardware (`TMR0H` = 249 → 1000 µs), sin recargar por software. El Timer2 es del PWM. `ADACT` solo elige una fuente, así que Q lo arranca la ISR | Timer1/3/5 (16 bits: recarga por software), PWM6/7 (15,6 kHz), pin externo, escribir `ADPCH` o leer `ADRESH` (software, con jitter). Timer4 y Timer6 también servirían (`docs/especificacion_adc.md` §2.1) |
| Origen de `Vref` | `VDD`, o sea generada por la PIC (`ADPREF` = 00) | Sin pines ni piezas; ratiométrica con `Vref2` (mismo riel): el rizado de `VDD` no mueve el código de reposo [PROPUESTA; falta confirmar] | FVR (no puede superar `VDD`; 2,048 V recorta; no es ratiométrica) y pin `VREF+` (RA1 es ICSPCLK). Ver `docs/especificacion_adc.md` §7.1 |
| Tiempo de adquisición | `TACQ = 2 µs + CHOLD·(RIC + RSS + RS)·ln(8191) + 0,05 µs/°C·(T − 25)`; `ADACQ` = ⌈1,2·`TACQ` / `T_unidad`⌉ | Fórmula del datasheet (p.506–507); con `RS` = 10 kΩ y 50 °C: 8,55 µs, o sea `ADACQ` = 6 si `T_unidad` = 2 µs [PROPUESTA] | `ADACQ = 160` (10, 40 o 320 µs) hasta medir la unidad (prueba 4.16). Cuentas en `docs/especificacion_adc.md` §3.1 |
| `RS`: impedancia de salida del opa de I y Q | por leer del datasheet del opa; ≤ 10 kΩ | Entra en `TACQ` y fija la recuperación del opa tras el golpe de carga (`docs/especificacion_adc.md` §3.2) | – |

**Resolución:** 12 bits (DS `AD01`, p.632). El LSB es 0,903 mV y el ruido de cuantización ideal 261 µV con 3,7 V. **El SINAD depende de la amplitud:** con la señal real de PCB2 (±185 mV) un 12 bits ideal da 54 dB, por eso en §9 queda pendiente cómo se entiende el requisito.

**Cómo se comprueba:** pruebas 4.3 (histograma), 4.15 (SINAD), 4.16 (adquisición real y ráfaga) y 4.17 (jitter de Q) de `docs/protocolos_prueba.md`.
