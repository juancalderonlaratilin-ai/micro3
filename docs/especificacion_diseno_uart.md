# Especificación de diseño: enlace UART PC ↔ PIC (Fase 1)

Proyecto Micro 3 · Firmware `firmware/Micro_Firmware.X` · Fecha: 2026-10-16

> **Estado:** borrador v2, revisado contra el datasheet, la errata y la guía de XC8. Los valores marcados **[PROPUESTA]** son decisiones de diseño que hay que confirmar. Los marcados **[VERIFICADO]** salen directamente de los documentos de `docs/`. El detalle y las páginas de cada dato están en **`notas_pic16f18426.md`**.

---

## 1. Alcance

**Dentro de la Fase 1:** comunicación serie bidireccional PC → PIC y PIC → PC, no bloqueante, con interrupciones, en binario (sin ASCII).

**Fuera de la Fase 1** (se diseña después, pero los pines ya quedan reservados): lectura de I y Q con el ADC, PWM de auto-cero (I0/Q0), máquina de estados de calibración y la interfaz gráfica, que va **en el PC** (LabVIEW o Python), no en el PIC.

**Objetivo de la Fase 1:** demostrar que el PIC recibe y envía bytes y tramas sin pérdidas y sin bloquear el programa principal.

---

## 2. Parámetros de diseño

| Parámetro | Valor | Estado |
|---|---|---|
| Microcontrolador | PIC16F18426 (8 bits, encapsulado de 14 pines) | confirmado (configuración del proyecto) |
| IDE / compilador | MPLAB X 6 + XC8 2.40 | confirmado |
| Programador | SNAP | confirmado |
| Estilo de código | C "bare-metal" sobre registros (SFR). **Sin MCC.** | regla de la práctica |
| Oscilador | **HFINTOSC interno a 16 MHz**, sin PLL ni cristal (exactitud nominal ±2 %) | [PROPUESTA] |
| Rango de VDD del chip | PIC16F18426 (sin "L"): 2,3 – 5,5 V con Fosc ≤ 16 MHz. **Un PIC16LF18426 (máx. 3,6 V) no sirve con 3,7 V** | [VERIFICADO] |
| Ciclo de instrucción | Fcy = Fosc/4 = 4 MHz → 250 ns | calculado |
| Formato UART | **8N1** (8 bits de datos, sin paridad, 1 bit de parada), asíncrono | [PROPUESTA] |
| Baud rate | **115200** | [PROPUESTA] |
| Alimentación (VCC / VDD) | **3,7 V** DC de la fuente Keysight E36233A (GND a +3,7 V). Se asume que el PIC comparte esta alimentación. Las referencias Vref/Vref2 ≈ 1,85 V son la tierra virtual de las señales analógicas | confirmado (preinforme §1.2) |
| Nivel lógico del conversor USB-serie | 3,3 V (CP2102, FT232R o similar) | ver nota 2.3 |
| Visualización (gráfica de impedancia I vs Q) | **En el PC**, por UART, con LabVIEW o Python. El PIC no maneja pantalla | decidido (2026-10-16) |

### 2.1 Por qué 16 MHz

La carga del procesador es muy baja: en la Fase 2 solo se muestrearán dos señales de unos 20 Hz. 16 MHz sobra, consume menos que 32 MHz (la placa va con batería) y da un error de baud rate pequeño. Si aparecieran errores de framing, se puede subir a 32 MHz sin cambiar el diseño.

### 2.2 Cálculo del baud rate

Modo de alta velocidad con generador de 16 bits (`BRGH = 1`, `BRG16 = 1`):

```
SPBRG = Fosc / (4 × Baud) − 1 = 16 000 000 / (4 × 115 200) − 1 = 33.72  →  34
Baud real = 16 000 000 / (4 × (34 + 1)) = 114 286 baud
Error = (114 286 − 115 200) / 115 200 = −0.79 %
```

Registros: `SP1BRGH = 0`, `SP1BRGL = 34`. La fórmula (`Fosc / [4 × (n + 1)]` con `BRGH = 1` y `BRG16 = 1`) y el valor de 32 MHz (`n = 68`, +0,64 %) coinciden con la Tabla 28-2 del datasheet [VERIFICADO].

**Margen de error:** el −0,79 % se suma a la exactitud nominal del oscilador interno (±2 %), así que el peor caso combinado ronda **−2,8 %**, al límite de lo tolerable. Si el enlace pierde bytes por framing, el datasheet recomienda ajustar **`OSCTUNE`** como método preferido (DS p.410). La alternativa es subir a 32 MHz (`SPBRG = 68`, error +0,64 %). La prueba P2 (ráfaga de 1000 bytes) debe validar este margen.

### 2.3 Notas de hardware

1. **Presupuesto de tiempo a 115200 baud:** 1 byte = 10 bits = 86,8 µs ≈ 347 ciclos de instrucción. Las ISR de UART deben ser cortas (objetivo: RX + TX combinadas < 100 ciclos por byte).
2. **Niveles lógicos con VDD = 3,7 V:**
   - **PIC → conversor:** el TX del PIC sale a ~3,7 V. En el datasheet del CP2102-GMR (Silicon Labs) los pines de la UART están referidos a VIO: máximo absoluto **VIO + 0,5 V** y la hoja no los describe como tolerantes a 5 V. Con VIO = 3,3 V el máximo es 3,8 V: 3,7 V entra, pero con solo 0,1 V de margen. **Decisión: divisor resistivo en el TX del PIC (1 kΩ en serie + 2,2 kΩ a GND)**, que deja ≈2,5 V en el RXD del adaptador (VIH mínimo con VIO = 3,3 V: 1,9 V). El módulo real puede llevar otra variante del CP2102: verificar el marcado del chip.
   - **Conversor → PIC:** un conversor a 3,3 V entrega ~3,3 V. Con VDD = 3,7 V, la entrada Schmitt (por defecto) exige VIH ≥ 0,8 × VDD = **2,96 V**, un margen justo. Con entrada TTL basta VIH ≥ 0,25 × VDD + 0,8 = **1,73 V**. **Decisión: configurar RC5 como TTL (`INLVLC5 = 0`)** [VERIFICADO: DS Tabla 39-4 y registro `INLVLC`, p.217].
   - **No** uses un conversor con salida a 5 V: la entrada del PIC no debe superar VDD + 0,3 V.
3. **Tierra común** entre el conversor USB-serie y la placa.
4. **Por qué 115200 y no 9600** (de cara a la Fase 2): los filtros de I/Q son de 20 Hz, así que bastan ~100–200 muestras por segundo. Con tramas de 9 bytes eso son 900–1800 B/s (9–18 kbps de los 115,2 kbps). A 9600 baud el enlace se saturaría.

---

## 3. Asignación de pines

El pinout del PIC16F18426 de 14 pines y las funciones de cada pin están [VERIFICADOS] (DS p.6 y p.8). **La asignación de funciones a cada pin es [PROPUESTA]**: hay que confirmar que coincide con tu placa o protoboard.

Evitar los pines de programación (RA0 = ICSPDAT, RA1 = ICSPCLK, RA3 = MCLR) para no perder el depurador SNAP. Las funciones digitales (UART, PWM) se asignan con PPS, así que se pueden mover a cualquier RAx/RCx sin cambiar el diseño.

| Pin físico | Puerto | Función | Fase | Notas |
|---|---|---|---|---|
| 6 | **RC4** | **TX (EUSART1)** | **1** | salida vía PPS: `RC4PPS = 0x0F`. Bajar `ANSELC4` (reset = analógico) |
| 5 | **RC5** | **RX (EUSART1)** | **1** | entrada: `RX1DTPPS = 0x15` (en el datasheet se llama `RX1PPS`) (ya es el valor por defecto). Bajar `ANSELC5`. Nivel TTL (`INLVLC5 = 0`) |
| 2 | **RA5** | LED de estado (heartbeat / error) | **1** | salida digital (bajar `ANSELA5`). Opcional, muy útil para depurar |
| 10 | RC0 | Entrada analógica canal I (ANC0) | 2 | reservado |
| 9 | RC1 | Entrada analógica canal Q (ANC1) | 2 | reservado |
| 11 | RA2 | PWM de I0 (auto-cero) | 2 | reservado. PWM6 vía PPS (`RA2PPS = 0x0D`) |
| 3 | RA4 | PWM de Q0 (auto-cero) | 2 | reservado. PWM7 vía PPS (`RA4PPS = 0x0E`) |
| 7 | RC3 | Pulsador de auto-cero (IOC, pull-up débil) | 2 | reservado |
| 8 | RC2 | libre (reserva: pin de prueba para medir la ISR con el osciloscopio) | – | |
| 13 / 12 / 4 | RA0 / RA1 / RA3 | ICSPDAT / ICSPCLK / MCLR | – | programación, no usar. RA3 solo es entrada |
| 1 / 14 | VDD / VSS | alimentación | – | con capacitor de desacople de 100 nF cerca |

**Regla general:** al reset, todos los pines de `ANSELA` y `ANSELC` están en **modo analógico** (valor 1). Cada pin que se use como digital necesita su bit de `ANSEL` en 0 (DS p.205).

### 3.1 Presupuesto de pines [PROPUESTA — decisión: se queda el PIC16F18426 de 14 pines]

| Función | Pines | Fase |
|---|---|---|
| UART TX y RX | 2 | 1 |
| LED de estado | 1 | 1 |
| Canales I y Q (ADC) | 2 | 2 |
| Auto-cero I0 y Q0 (PWM6 y PWM7 + filtro RC + buffer) | 2 | 2 |
| Pulsador de auto-cero | 1 | 2 |
| **Usados** | **8** | |
| Libres para E/S (12 − RA0, RA1, RA3) | 9 | |
| **Reserva** | **1** (RC2) | |

Razones y consecuencias:

- **Los trimmers I0/Q0 sustituidos por PWM** son los únicos elementos analógicos que el PIC controla. Según el preinforme, `V_out = Vref2 + 10·(I_LPF − I0)`, así que el PWM filtrado y con buffer ocupa el lugar del trimmer.
- **La gráfica de impedancia no usa pines del PIC.** Los valores de I y Q viajan por la UART y el PC dibuja I (eje X) contra Q (eje Y). Cada muestra de 12 bits ocupa 2 bytes, de modo que I + Q = 4 bytes y caben en el campo de datos de la trama (§8.2). A 200 muestras/s son ~1800 B/s, el 15,6 % de la capacidad de la línea.
- **Una pantalla local no cabe** en este chip salvo un OLED I2C quitando el LED (0 de reserva). Una pantalla SPI necesitaría 4–5 pines. Si el profe la pide, el reemplazo sin cambios de código es el **PIC16F18446** (20 pines, 18 E/S, 17 canales de ADC; mismo datasheet [VERIFICADO: DS p.14]).
- **Sin cristal externo:** usaría RA4 (OSC2) y RA5 (OSC1), que aquí son el PWM de Q0 y el LED.
- **Debe respetarse RA0/RA1/RA3** para que el SNAP pueda programar y depurar.

---

## 4. Periféricos (Fase 1)

| Periférico | Uso | Configuración clave |
|---|---|---|
| Oscilador HFINTOSC | reloj del sistema | `RSTOSC` = HFINTOSC 1 MHz; en código: `OSCFRQ = 0b101` (16 MHz), `OSCCON1 = 0x60` (NOSC = 110, NDIV = 1:1), esperar `OSCCON3[ORDY]` |
| PPS | enrutar TX y RX a RC4 / RC5 | `RC4PPS = 0x0F` (TX1), `RX1DTPPS = 0x15` (en el datasheet se llama `RX1PPS`) (RC5). `PPSLOCKED = 0` tras el reset: no hace falta desbloquear |
| Entradas/salidas | pines digitales | `ANSELC4 = ANSELC5 = 0`; `TRISC4 = 0`, `TRISC5 = 1`; `INLVLC5 = 0` (TTL) |
| EUSART1 | enlace con el PC | async, 8N1, `BRGH = 1`, `BRG16 = 1`, `SP1BRGH = 0`, `SP1BRGL = 34`, `SYNC = 0`, `SPEN`, `TXEN`, `CREN` |
| Interrupciones | RX y TX por evento | `RC1IE` (recepción) siempre activa. `TX1IE` **solo mientras hay datos por enviar**. `PEIE` y `GIE` activos. Vector único en `0x0004` |
| GPIO | LED de estado en RA5 | salida digital |
| Bits de configuración | ver §11 | `LVP` habilitado, WDT desactivado en desarrollo, BOR activo |

Periféricos reservados para la Fase 2: ADCC (2 canales, referencia VDD = 3,7 V; las salidas I y Q quedan centradas en ~1,85 V), Timer 2 + **PWM6/PWM7** (auto-cero), Timer 1 o Timer 0 (disparo periódico del ADC) e IOC (pulsador).

---

## 5. Eventos

Arquitectura guiada por eventos: las interrupciones solo capturan el evento y mueven bytes. El procesamiento ocurre en el programa principal.

| ID | Evento | Fuente | Respuesta |
|---|---|---|---|
| E1 | Llegó un byte | `RC1IF && RC1IE` | La ISR lee `RC1STA` (para `FERR`/`OERR`) **antes** que `RC1REG`, y guarda el byte en la FIFO de RX. Leer `RC1REG` limpia `RC1IF` |
| E2 | `TX1REG` libre | `TX1IF && TX1IE` (**hay que comprobar las dos**: `TX1IF` vale 1 siempre que `TX1REG` esté vacío) | La ISR saca el siguiente byte de la FIFO de TX y lo escribe en `TX1REG`. Al escribir el último byte, desactiva `TX1IE` |
| E3 | Error de hardware en RX (framing u overrun) | `FERR` / `OERR` | Marca la bandera de error. El framing **no genera interrupción**: se detecta al leer el byte. Si hay overrun (`OERR`), se recupera con `CREN = 0` y luego `CREN = 1` |
| E4 | FIFO de RX llena | la ISR no puede guardar | Descarta el byte y marca la bandera de overflow |
| E5 | FIFO de TX llena | `uart_tx_*` no puede encolar | Devuelve `false` al llamador (no bloquea) |
| E6 | Trama completa y válida recibida | el parser (programa principal) | Ejecuta el comando y encola la respuesta |
| E7 | Trama inválida (CRC malo o tipo desconocido) | el parser | Descarta, responde con trama de error y marca el LED |

---

## 6. Tareas

| ID | Tarea | Dónde corre | Descripción |
|---|---|---|---|
| T1 | `system_init()` | arranque | Oscilador, pines/PPS, EUSART, interrupciones, LED |
| T2 | bloque RX de la ISR | interrupción (E1, E3, E4) | Mover el byte de hardware a la FIFO de RX. Corto y sin lógica de protocolo |
| T3 | bloque TX de la ISR | interrupción (E2) | Mover un byte de la FIFO de TX al hardware |
| T4 | `uart_tx_DAT(const uint8_t *data, uint16_t size)` | llamada desde el programa principal | Encola un bloque de bytes. `true` si cupo todo, `false` si no (E5) |
| T5 | `uart_tx_string(const char *s)` | llamada desde el programa principal | Igual que T4, para cadenas terminadas en `'\0'` (firma pedida en clase) |
| T6 | `uart_rx_read(uint8_t *byte)` | llamada desde el programa principal | Saca un byte de la FIFO de RX. `false` si está vacía |
| T7 | `parser_task()` | bucle principal | Lee bytes de la FIFO de RX, busca el encabezado, verifica CRC y ejecuta el comando (E6/E7) |
| T8 | `status_task()` | bucle principal | Maneja el LED y reporta banderas de error |

**Bucle principal:** `while(1) { parser_task(); status_task(); }`, sin esperas bloqueantes (prohibido `while(!TXIF)` y `__delay_ms` en la lógica).

**Una sola ISR:** el chip tiene un único vector de interrupción (`0x0004`) y no tiene prioridades. T2 y T3 son dos bloques dentro de la misma función `void __interrupt() isr(void)`, que comprueba **bandera y habilitación** de cada fuente y limpia (o consume) la bandera antes de salir. No se rehabilita `GIE` dentro de la ISR [VERIFICADO: UG §5.8].

**Prioridad:** E1 y E2 son las únicas interrupciones. Si coinciden, la ISR atiende primero la recepción (perder un byte recibido es peor que retrasar uno enviado).

**Plazo de servicio de RX:** el receptor tiene una FIFO de solo 2 bytes y se produce overrun si llega completo un tercer byte sin haber leído `RC1REG`. Con 86,8 µs por byte, la ISR debe atenderse en menos de ~2 tiempos de byte (≈ 174 µs). En la Fase 2, ninguna otra ISR (ADC, timer) puede retener la CPU ese tiempo.

**Reglas de código de las ISR** [VERIFICADO: UG §5.8.5–5.8.7]:
- Variables compartidas con la ISR: `volatile`. Los accesos a más de 1 byte no son atómicos: proteger con `di()` / `ei()` en el programa principal.
- No llamar a la misma función desde la ISR y desde `main` (XC8 la duplica, porque no es reentrante). Los índices de las FIFO se manipulan con código propio de cada lado.

---

## 7. Estructuras de datos

| Estructura | Tamaño | Notas |
|---|---|---|
| FIFO de RX | 32 bytes | potencia de 2 para usar máscara en lugar de módulo |
| FIFO de TX | 64 bytes | |
| Índices (`head`, `tail`) | `uint8_t`, `volatile` | la ISR escribe uno y el programa principal el otro |

El chip tiene 2048 B de RAM [VERIFICADO], así que hay margen para agrandar las FIFO si la Fase 2 lo pide. Los motivos de cada tamaño y qué pasa al llenarse están en `docs/criterios_diseno.md` §6.1.

Las operaciones de la FIFO que compartan estado entre la ISR y el programa principal deben ser atómicas (índice de 8 bits, o sección crítica corta).

---

## 8. Protocolo (binario, sin ASCII)

### 8.1 Fase 1a: eco crudo

El PIC devuelve exactamente cada byte que recibe. Sirve para validar cableado, baud rate y las dos direcciones sin protocolo.

### 8.2 Fase 1b: trama de 9 bytes (borrador)

Una **trama** es un grupo de bytes con estructura fija. Todas miden 9 bytes y son iguales en ambos sentidos, así no hace falta campo de longitud.

| Byte | Campo | Descripción |
|---|---|---|
| 0 | `SYNC1` | `0xA5` |
| 1 | `SYNC2` | `0x5A` |
| 2 | `TYPE` | tipo de trama |
| 3 | `SEQ` | contador 0–255, para detectar tramas perdidas |
| 4–7 | `PAYLOAD` | 4 bytes, su significado depende de `TYPE` (en la Fase 2 llevará los datos medidos) |
| 8 | `CRC8` | CRC-8 (polinomio `0x07`, valor inicial `0x00`) calculado sobre los bytes 2 a 7 |

- **Orden de bytes:** big-endian (byte alto primero), que es el que usa LabVIEW por defecto al convertir bytes a enteros.
- **Resincronización:** si el CRC falla, el receptor descarta 1 byte y busca de nuevo `A5 5A`.
- **Tipos de la Fase 1:**

| TYPE | Sentido | Nombre | Payload |
|---|---|---|---|
| `0x01` | PC → PIC | PING | libre (4 bytes cualquiera) |
| `0x81` | PIC → PC | PONG | copia el payload y el `SEQ` del PING |
| `0x7F` | PIC → PC | ERROR | `payload[0]` = código de error |

- **Códigos de error:** `1` CRC inválido, `2` tipo desconocido, `3` overflow de la FIFO de RX, `4` error de hardware (framing/overrun).
- **Rendimiento:** 9 bytes = 90 bits → 781 µs por trama a 115200 baud (máx. ~1280 tramas/s).

[PENDIENTE DE CONFIRMAR] CRC-8 o suma XOR, y si se agregan más tipos de trama.

---

## 9. Pruebas de aceptación

| # | Prueba | Criterio de éxito |
|---|---|---|
| P1 | Eco de 1 byte desde una terminal serie en hexadecimal | vuelve el mismo byte |
| P2 | Ráfaga de 1000 bytes | vuelven los 1000 bytes sin pérdidas ni cambios |
| P3 | Enviar mientras el PIC transmite | sin pérdidas y el programa principal nunca se queda esperando |
| P4 | PING con payload conocido | PONG con el mismo `SEQ` y payload, CRC correcto |
| P5 | PING con CRC alterado | el PIC responde ERROR (código 1) y sigue funcionando |
| P6 | Bytes basura antes de una trama válida | el parser resincroniza y responde al PING |
| P7 | Desconectar y reconectar el cable | el enlace se recupera sin reiniciar el PIC |

---

## 10. Pendientes por confirmar

1. **Que tu chip sea `PIC16F18426` y no `PIC16LF18426`** (mira el marcado). El LF solo admite hasta 3,6 V y la placa trabaja a 3,7 V.
2. Que el PIC se alimente de los mismos 3,7 V que PCB2, y el modelo exacto del conversor USB-serie con su nivel lógico.
3. Que los pines propuestos (§3) coincidan con tu placa o protoboard.
4. **Revisión de silicio del chip** (MPLAB X → *Window > Dashboard*). Si es A4, ninguna errata del EUSART aplica. Si es A1/A2, la regla de escribir `TX1REG` solo con `TX1IF = 1` ya cubre el workaround (ER 1.9.2).
5. CRC-8 o XOR, y la lista de tipos de trama.
6. WDT: se deja desactivado en desarrollo. ¿Se activa en la versión final?
7. Valores exactos de los bits de configuración: generarlos con MPLAB X (*Window > Target Memory Views > Configuration Bits*) o consultar `pic_chipinfo.html` de la carpeta `docs` de XC8.
8. Qué terminal se usará para las pruebas (Realterm, Hercules, Tera Term) y si el PC ya tiene el driver del conversor.
9. Valor `VBOR` (brown-out) con VDD = 3,7 V, que falta leer en las especificaciones eléctricas.
10. Referencia del ADC = VDD (Fase 2). Comprobar en el capítulo del ADCC (`ADREF`) y revisar el ruido de la alimentación, porque la lectura depende de ella.

---

## 11. Bits de configuración [PROPUESTA — generar el pragma con MPLAB X]

| Bit | Valor | Motivo |
|---|---|---|
| `FEXTOSC` | sin oscilador externo (`100`) | se usa el HFINTOSC |
| `RSTOSC` | HFINTOSC 1 MHz (`110`) | punto de partida limpio para subir a 16 MHz en código |
| `CSWEN` | `1` | permite cambiar `NOSC`/`OSCFRQ` desde el código |
| `LVP` | **`1` (habilitado)** | con `LVP = 1`, RA3 es MCLR y `MCLRE` se ignora. Con `LVP = 0` haría falta alto voltaje en MCLR para programar |
| `WDTE` | `00` (desactivado) | solo durante el desarrollo |
| `BOREN` | `11` (activo) | protege contra caídas de tensión |
| `STVREN` | `1` | reset por desbordamiento de pila |
| `PPS1WAY` | `0` | permite reconfigurar PPS más de una vez (el valor de fábrica es 1) |
| `DEBUG` | lo gestiona MPLAB X/SNAP | no tocar a mano |

XC8 exige programar **todos** los bits de configuración, y los valores numéricos del `#pragma config` se escriben en decimal o hexadecimal, **nunca con `0b`** [VERIFICADO: UG §5.2.5].

---

## 12. Documentos de referencia (en `docs/`)

| Archivo | Contenido |
|---|---|
| `notas_pic16f18426.md` | **Resumen verificado** del chip: pines, oscilador, PPS, EUSART, errata, XC8, con páginas |
| `PIC16-L-F18426-46-Micorontrollers-with-XLP-40001985.pdf` | Datasheet (fuente de verdad) |
| `PIC16-L-F18426-46-Family-Silicon-Errata-...-80000799.pdf` | Errata de silicio |
| `50002737C XC8 C Compiler UG for PIC.pdf` | Guía del compilador XC8 |
| `TB3261-Technical-Brief-DS90003261A.pdf` | Cómo escribir C para PIC16/PIC18 |
| `00688b.pdf` | AN688: layout para ADC de 12 bits (relevante para PCB, no para el firmware) |
| `Preinforme Practica 2 Final-...pdf` | Alimentación, filtros de 20 Hz, referencias de 1,85 V |
