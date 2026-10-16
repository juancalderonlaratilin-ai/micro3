# Especificación de diseño: enlace UART PC ↔ PIC (Fase 1)

Proyecto Micro 3 · Firmware `firmware/Micro_Firmware.X` · Fecha: 2026-10-16

> **Estado:** borrador para revisión. Los valores marcados **[PROPUESTA]** los fijé yo y hay que confirmarlos contra el hardware real. Los marcados **[VERIFICAR]** hay que contrastarlos con el datasheet del PIC16F18426.

---

## 1. Alcance

**Dentro de la Fase 1:** comunicación serie bidireccional PC → PIC y PIC → PC, no bloqueante, con interrupciones, en binario (sin ASCII).

**Fuera de la Fase 1** (se diseña después, pero los pines ya quedan reservados): lectura de I y Q con el ADC, PWM de auto-cero (I0/Q0), máquina de estados de calibración y la interfaz gráfica en LabVIEW.

**Objetivo de la Fase 1:** demostrar que el PIC recibe y envía bytes y tramas sin pérdidas y sin bloquear el programa principal.

---

## 2. Parámetros de diseño

| Parámetro | Valor | Estado |
|---|---|---|
| Microcontrolador | PIC16F18426 (8 bits, encapsulado de 14 pines) | confirmado (configuración del proyecto) |
| IDE / compilador | MPLAB X + XC8 2.40 | confirmado |
| Programador | SNAP | confirmado |
| Estilo de código | C "bare-metal" sobre registros (SFR). **Sin MCC.** | regla de la práctica |
| Oscilador | **HFINTOSC interno a 16 MHz**, sin PLL ni cristal | [PROPUESTA] |
| Ciclo de instrucción | Fcy = Fosc/4 = 4 MHz → 250 ns | calculado |
| Formato UART | **8N1** (8 bits de datos, sin paridad, 1 bit de parada), asíncrono | [PROPUESTA] |
| Baud rate | **115200** | [PROPUESTA] |
| Alimentación del PIC (VDD) | por confirmar. La placa analógica trabaja a 3,7 V | [VERIFICAR] |
| Nivel lógico del conversor USB-serie | 3,3 V (CP2102, FTDI o CH340 en modo 3,3 V) | ver nota 2.3 |

### 2.1 Por qué 16 MHz

La carga del procesador es muy baja: en la Fase 2 solo se muestrearán dos señales de unos 20 Hz. 16 MHz sobra, consume menos que 32 MHz (la placa va con batería) y da un error de baud rate pequeño. Si aparecieran errores de framing, se puede subir a 32 MHz sin cambiar el diseño.

### 2.2 Cálculo del baud rate

Modo de alta velocidad con generador de 16 bits (`BRGH = 1`, `BRG16 = 1`):

```
SPBRG = Fosc / (4 × Baud) − 1 = 16 000 000 / (4 × 115 200) − 1 = 33.72  →  34
Baud real = 16 000 000 / (4 × (34 + 1)) = 114 286 baud
Error = (114 286 − 115 200) / 115 200 = −0.79 %
```

Registros: `SP1BRGH = 0`, `SP1BRGL = 34`. Un error de −0,79 % es aceptable, pero se suma a la exactitud del oscilador interno. Si el enlace pierde bytes por framing, hay que subir a 32 MHz (`SPBRG = 68`, error +0,64 %) o ajustar `OSCTUNE`.

### 2.3 Notas de hardware

1. **Presupuesto de tiempo a 115200 baud:** 1 byte = 10 bits = 86,8 µs ≈ 347 ciclos de instrucción. Las ISR de UART deben ser cortas (objetivo: RX + TX combinadas < 100 ciclos por byte).
2. **Niveles lógicos:** TX del PIC (nivel VDD) hacia RX del conversor, y TX del conversor hacia RX del PIC. Con VDD de 3,7 V **no** uses un conversor con salida a 5 V: la entrada del PIC no debe superar VDD + 0,3 V. Verifica el nivel del módulo que vas a usar.
3. **Tierra común** entre el conversor USB-serie y la placa.
4. **Por qué 115200 y no 9600** (de cara a la Fase 2): los filtros de I/Q son de 20 Hz, así que bastan ~100–200 muestras por segundo. Con tramas de 9 bytes eso son 900–1800 B/s (9–18 kbps de los 115,2 kbps). A 9600 baud el enlace se saturaría.

---

## 3. Asignación de pines [PROPUESTA — verificar contra el pinout del datasheet]

Evitar los pines de programación (RA0 = ICSPDAT, RA1 = ICSPCLK, RA3 = MCLR) para no perder el depurador SNAP. Las funciones digitales (UART, PWM) se asignan con PPS, así que se pueden mover a cualquier RAx/RCx sin cambiar el diseño.

| Pin | Función | Fase | Notas |
|---|---|---|---|
| **RC4** | **TX (EUSART1)** | **1** | salida, vía PPS |
| **RC5** | **RX (EUSART1)** | **1** | entrada, vía PPS |
| **RA5** | LED de estado (heartbeat / error) | **1** | salida digital, opcional pero muy útil para depurar |
| RC0 | Entrada analógica canal I (ANC0) | 2 | reservado |
| RC1 | Entrada analógica canal Q (ANC1) | 2 | reservado |
| RA2 | PWM de I0 (auto-cero) | 2 | reservado |
| RA4 | PWM de Q0 (auto-cero) | 2 | reservado |
| RC3 | Pulsador de auto-cero (IOC, pull-up) | 2 | reservado |
| RC2 | libre | – | |
| RA0 / RA1 / RA3 | ICSPDAT / ICSPCLK / MCLR | – | programación, no usar |
| VDD / VSS | alimentación | – | con capacitor de desacople de 100 nF cerca |

---

## 4. Periféricos (Fase 1)

| Periférico | Uso | Configuración clave |
|---|---|---|
| Oscilador HFINTOSC | reloj del sistema | 16 MHz (`OSCFRQ`), `RSTOSC` = HFINTOSC |
| PPS | enrutar TX y RX a RC4 / RC5 | `RC4PPS` = TX1, `RX1PPS` = RC5 |
| EUSART1 | enlace con el PC | async, 8N1, `BRGH = 1`, `BRG16 = 1`, `SP1BRG = 34`, `SPEN`, `TXEN`, `CREN` |
| Interrupciones | RX y TX por evento | `RC1IE` (recepción) siempre activa. `TX1IE` solo mientras hay datos por enviar. `PEIE` y `GIE` activos |
| GPIO | LED de estado en RA5 | salida digital |
| Bits de configuración | desactivar lo que estorbe | WDT según decisión (ver pendientes), LVP desactivado si se usa SNAP, `MCLRE` según el hardware |

Periféricos reservados para la Fase 2: ADCC (2 canales), Timer 2 + PWM3/PWM4 (auto-cero), Timer 1 o Timer 0 (disparo periódico del ADC) e IOC (pulsador).

---

## 5. Eventos

Arquitectura guiada por eventos: las interrupciones solo capturan el evento y mueven bytes. El procesamiento ocurre en el programa principal.

| ID | Evento | Fuente | Respuesta |
|---|---|---|---|
| E1 | Llegó un byte | `RC1IF` | La ISR lee `RC1REG` y lo guarda en la FIFO de RX |
| E2 | `TXREG` libre | `TX1IF` (con `TX1IE` activa) | La ISR saca el siguiente byte de la FIFO de TX y lo escribe en `TX1REG`. Si la FIFO está vacía, desactiva `TX1IE` |
| E3 | Error de hardware en RX (framing u overrun) | `FERR` / `OERR` | Marca la bandera de error. Si hay overrun, resetea `CREN` para recuperar la recepción |
| E4 | FIFO de RX llena | la ISR no puede guardar | Descarta el byte y marca la bandera de overflow |
| E5 | FIFO de TX llena | `uart_tx_*` no puede encolar | Devuelve `false` al llamador (no bloquea) |
| E6 | Trama completa y válida recibida | el parser (programa principal) | Ejecuta el comando y encola la respuesta |
| E7 | Trama inválida (CRC malo o tipo desconocido) | el parser | Descarta, responde con trama de error y marca el LED |

---

## 6. Tareas

| ID | Tarea | Dónde corre | Descripción |
|---|---|---|---|
| T1 | `system_init()` | arranque | Oscilador, pines/PPS, EUSART, interrupciones, LED |
| T2 | `ISR_rx` | interrupción (E1, E3, E4) | Mover el byte de hardware a la FIFO de RX. Corta y sin lógica de protocolo |
| T3 | `ISR_tx` | interrupción (E2) | Mover un byte de la FIFO de TX al hardware |
| T4 | `uart_tx_DAT(const uint8_t *data, uint16_t size)` | llamada desde el programa principal | Encola un bloque de bytes. `true` si cupo todo, `false` si no (E5) |
| T5 | `uart_tx_string(const char *s)` | llamada desde el programa principal | Igual que T4, para cadenas terminadas en `'\0'` (firma pedida en clase) |
| T6 | `uart_rx_read(uint8_t *byte)` | llamada desde el programa principal | Saca un byte de la FIFO de RX. `false` si está vacía |
| T7 | `parser_task()` | bucle principal | Lee bytes de la FIFO de RX, busca el encabezado, verifica CRC y ejecuta el comando (E6/E7) |
| T8 | `status_task()` | bucle principal | Maneja el LED y reporta banderas de error |

**Bucle principal:** `while(1) { parser_task(); status_task(); }`, sin esperas bloqueantes (prohibido `while(!TXIF)` y `__delay_ms` en la lógica).

**Prioridad:** E1 y E2 son las únicas interrupciones. Si coinciden, la ISR atiende primero la recepción (perder un byte recibido es peor que retrasar uno enviado).

---

## 7. Estructuras de datos

| Estructura | Tamaño | Notas |
|---|---|---|
| FIFO de RX | 32 bytes | potencia de 2 para usar máscara en lugar de módulo |
| FIFO de TX | 64 bytes | |
| Índices (`head`, `tail`) | `uint8_t`, `volatile` | la ISR escribe uno y el programa principal el otro |

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

1. VDD real del PIC y nivel lógico del conversor USB-serie.
2. Pines TX/RX y LED en la placa o protoboard, contra el pinout del datasheet.
3. ¿WDT activado? (recomendado en la versión final)
4. CRC-8 o XOR, y la lista de tipos de trama.
5. Qué terminal se usará para las pruebas (Realterm, Hercules, Tera Term) y si el PC ya tiene el driver del conversor.
