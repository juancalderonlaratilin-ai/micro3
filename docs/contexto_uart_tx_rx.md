# Contexto: cómo transmitimos y cómo recibimos por la UART (Micro 3)

Documento de contexto para quien trabaje con el enlace serie del proyecto, sea una persona u otra IA. Describe el firmware tal como está en `firmware/Micro_Firmware.X` (archivos `mcc_generated_files/eusart1.c`, `protocol.c/.h`, `crc8.c`, `main.c`, `mcc_generated_files/interrupt_manager.c`) y la placa Micro3.

**Resumen en una línea:** el PIC nunca espera a la UART. Transmitir es copiar bytes a un buffer circular de 64 bytes y dejar que la interrupción de TX los saque uno por uno. Recibir es que la interrupción de RX guarde cada byte en un buffer circular de 32, y que el programa principal los lea y los arme en tramas de 9 bytes con CRC-8.

---

## 1. Capa física

| Elemento | Valor |
|---|---|
| Microcontrolador | PIC16F18426, VDD = 3,7 V, Fosc = 16 MHz (oscilador interno, ±2 %) |
| TX del PIC | **RC4** (pin 6), por PPS: `RC4PPS = 0x0F` (TX1) |
| RX del PIC | **RC5** (pin 5), por PPS: `RX1DTPPS = 0x15` (valor de reset) |
| Dirección de pines | `TRISC = 0x2F`: RC4 salida, RC5 entrada. RC5 es digital (`ANSELC` bit 5 = 0) |
| Nivel de entrada RX | TTL (`INLVLC5 = 0`): VIH = 1,73 V |
| Adaptador al PC | módulo USB-serie **CP2102** a 3,3 V, por el conector **J5** (1 GND, 2 TX del módulo, 3 RX del módulo, 4 3V3, 5 GND). Confirmar que el orden coincide con el módulo |
| Traducción de niveles (placa Micro3) | **U5 SN74LVC1T45**: PIC_TX (3,7 V) → RX del CP2102 (3,3 V). **U6 SN74LVC1T45**: TX del CP2102 → PIC_RX. El lado de 3,3 V se alimenta del 3V3 del propio módulo |
| Jumpers | **JP_TX** y **JP_RX** deben estar cerrados para que haya comunicación |
| Resistencias | 100 Ω en serie en cada línea; pull-ups de 100 kΩ (la línea queda en 1 si nada la maneja) |
| J6 | acceso directo a PIC_TX/PIC_RX a 3,7 V, sin traductor. Solo para depurar: un CP2102 a 3,3 V conectado ahí recibe 3,7 V |

En reposo la línea está en 1. Cada byte empieza con un bit de inicio en 0 y termina con un bit de parada en 1.

## 2. Configuración del EUSART1 (`EUSART1_Initialize()`)

| Registro | Valor | Qué hace |
|---|---|---|
| `BAUD1CON` | `0x08` | `BRG16 = 1`: generador de baud de 16 bits |
| `TX1STA` | `0x24` | `TXEN = 1` (transmisor encendido), `BRGH = 1` (alta velocidad), `SYNC = 0` (asíncrono), 8 bits |
| `RC1STA` | `0x90` | `SPEN = 1` (puerto serie encendido), `CREN = 1` (recepción continua), 8 bits |
| `SP1BRGH:SP1BRGL` | `0x00:0x22` (34) | divisor del baud |
| `PIE3.RC1IE` | 1 | interrupción de RX siempre encendida |
| `PIE3.TX1IE` | 0 al inicio | la de TX solo se enciende cuando hay algo que mandar |

**Baud:** con `BRGH = 1` y `BRG16 = 1`, baud = Fosc / (4·(SPBRG + 1)) = 16 000 000 / (4·35) = **114 286 baud**. El PC se abre a 115200; la diferencia es −0,79 %, dentro de la tolerancia (±5,3 % teórico, ±2–3 % práctico). Peor caso con el ±2 % del oscilador interno: ≈ −2,8 %.

**Formato:** **8N1**: 1 bit de inicio, 8 de datos (primero el menos significativo), sin paridad, 1 de parada = 10 bits por byte.

| Tiempo | Valor |
|---|---|
| Bit | 8,75 µs (8,68 µs a 115200 exactos) |
| Byte | 87,5 µs |
| Trama de 9 bytes | 787,5 µs |
| Capacidad de la línea | ≈ 11 520 bytes/s |

Las interrupciones globales (`PEIE`, `GIE`) se encienden en `main()` al final de la configuración, después de `adc_iq_init()`.

## 3. Transmisión (PIC → PC)

### 3.1 Camino de un byte

```
código de main ──► tx_DAT() / tx_string() / EUSART1_Write()
                     │  copia los bytes al buffer circular de TX (64 bytes)
                     │  y pone TX1IE = 1
                     ▼
               interrupción de TX (TX1IF = 1: TX1REG está vacío)
                     │  EUSART1_Transmit_ISR(): pasa UN byte del buffer a TX1REG
                     │  si el buffer quedó vacío: TX1IE = 0
                     ▼
               TX1REG ──► registro de desplazamiento (TSR) ──► pin RC4 ──► U5 ──► CP2102 ──► USB ──► PC
```

### 3.2 Buffer circular de TX

- Arreglo `eusart1TxBuffer[64]` con dos índices: `eusart1TxHead` (donde escribe main) y `eusart1TxTail` (de donde lee la ISR).
- Vacío: `head == tail`. Lleno: `(head + 1) & 63 == tail`. Se usan **63 bytes** (una posición queda libre para distinguir lleno de vacío): caben 7 tramas de 9 bytes.
- El índice avanza con `(i + 1) & (N − 1)`, una sola operación. Por eso los tamaños son potencias de 2 (hay un `#error` si no lo son).
- Main solo escribe `head` y la ISR solo escribe `tail`. Como cada índice es de 8 bits (una sola instrucción), no hace falta apagar interrupciones.

### 3.3 Funciones para transmitir

| Función | Qué hace | Si no cabe |
|---|---|---|
| `bool tx_DAT(const char *DATA, uint16_t size)` | encola un bloque completo: **todo o nada** (nunca sale media trama) | devuelve `false` y no encola nada |
| `bool tx_string(const char *data)` | encola una cadena terminada en `'\0'` (sin el `'\0'`); llama a `tx_DAT` con `strlen` | `false` |
| `bool EUSART1_Write(uint8_t b)` | encola un solo byte | `false` |
| `EUSART1_is_tx_done()` | `true` si el buffer está vacío **y** `TRMT = 1` (salió el último bit) | – |

Ninguna espera: si no hay espacio devuelven `false` al instante. El que llama **no reintenta**, solo cuenta la pérdida en un contador.

### 3.4 Qué transmite el PIC

- **Al arrancar:** el texto `Micro3 listo\r\n` (14 bytes ASCII), en cualquier modo. El parser del PC lo ignora porque no empieza con `A5 5A`.
- **Respuestas del protocolo:** PONG, ADC_ACK y ERROR (sección 5).
- **Datos del ADC:** una trama `DATA_IQ` cada 1 ms (una muestra de I y una de Q, sin promedio), solo si el PC la pidió con `ADC_STREAM`. El camino es `adc_iq_get()` → `protocol_send_iq(i, q)` → `frame_send()` → `crc8()` → `tx_DAT()`.

  | Paso | Duración |
  |---|---|
  | Armado de la trama | ≈ 215 µs [ESTIMADO] |
  | Salida por la línea | 787,5 µs, el 78,8 % de cada milisegundo |

- **Modo de prueba `MODO_PRUEBA_ADC = 1`:** texto `I:2048,Q:2051\r\n` cada 50 muestras (20 líneas/s) para el Serial Plotter de Arduino. En este modo no se atiende el protocolo.

### 3.5 Interrupción de TX

`TX1IF` sube cada vez que `TX1REG` queda libre, o sea al empezar a salir cada byte. Al encender `TX1IE` con el registro vacío, la ISR entra enseguida y carga los dos primeros bytes (uno pasa directo al TSR). Después entra una vez por byte, cada 87,5 µs. Cuando no queda nada, apaga `TX1IE` para no entrar en bucle.

## 4. Recepción (PC → PIC)

### 4.1 Camino de un byte

```
PC ──► CP2102 ──► U6 ──► pin RC5 ──► registro de desplazamiento (RSR) ──► FIFO de 2 bytes ──► RC1REG
                                                                                     │ RC1IF = 1
                                                                                     ▼
                         EUSART1_Receive_ISR(): anota FERR/OERR, lee RC1REG y lo guarda
                         en el buffer circular de RX (32 bytes) junto con su estado de error
                                                                                     ▼
                         main: protocol_task() ──► EUSART1_Read() ──► parser ──► tramas
```

### 4.2 Interrupción de RX

1. Limpia el estado del byte y revisa `RC1STA.FERR` (*framing error*: el bit de parada llegó en 0) y `RC1STA.OERR` (*overrun*: llegó un tercer byte con la FIFO de 2 llena).
2. Si hubo overrun, reinicia el receptor apagando y encendiendo `CREN` (si no, el EUSART deja de recibir).
3. Lee `RC1REG` (esto baja `RC1IF`) y guarda el byte en `eusart1RxBuffer[32]`, con su estado en `eusart1RxStatusBuffer[32]`. Los bytes con error también se guardan, marcados, para que main se entere.
4. Si el buffer de RX está lleno (31 bytes útiles), **descarta** el byte y sube `eusart1RxOverflow`.

**Plazo:** la FIFO del chip guarda solo 2 bytes. Si la ISR no lee `RC1REG` antes de que termine un tercer byte, hay overrun: ≈ 2 bytes = **174 µs**. Ninguna otra ISR (ADC, TX) puede tardar tanto.

### 4.3 Lectura en el programa principal: `protocol_task()`

Se llama en cada vuelta del `while(1)` de `main()` (modo protocolo: `MODO_PRUEBA_ADC = 0`, `MODO_ECO = 0`):

1. **Bytes perdidos:** si `EUSART1_get_overflow()` devuelve `true`, manda `ERROR` código 3 y reinicia el parser.
2. **Lee hasta 32 bytes** con `EUSART1_Read(&b)`, que devuelve `false` al instante si no hay nada. El tope de 32 evita que una vuelta de main se alargue.
3. **Errores de hardware:** si el byte vino con FERR u OERR (`EUSART1_get_last_status()`), manda `ERROR` código 4, reinicia el parser y no usa ese byte.
4. Pasa cada byte bueno a `parser_feed()`.

### 4.4 Parser (máquina de estados)

| Estado | Llega | Hace |
|---|---|---|
| `ESPERA_A5` | `0xA5` | lo guarda, pasa a `ESPERA_5A` |
| `ESPERA_A5` | otro | lo descarta en silencio (basura, texto) |
| `ESPERA_5A` | `0x5A` | encabezado completo, pasa a `CUERPO` |
| `ESPERA_5A` | otro `0xA5` | se queda en `ESPERA_5A` (caso `A5 A5 5A`) |
| `ESPERA_5A` | otro | vuelve a `ESPERA_A5` |
| `CUERPO` | cualquiera | lo guarda **sin interpretarlo**. Al llegar al byte 9, vuelve a `ESPERA_A5` y procesa la trama |

El cuerpo no se interpreta: como el largo es fijo, un payload que contenga `A5 5A` no confunde al parser.

**Al completar la trama** (`process_frame()`):
1. Recalcula el CRC-8 de los bytes 2 a 7 y lo compara con el byte 8.
   - **CRC malo:** manda `ERROR` 1 (con el SEQ de esa trama) y **resincroniza**: descarta solo el primer `A5` y vuelve a pasar por el parser los otros 8 bytes, por si entre ellos empezaba una trama verdadera.
2. **CRC bueno:** despacha por TYPE:
   - `PING` (`0x01`) → responde `PONG` (`0x81`) con el mismo SEQ y el mismo payload.
   - `ADC_STREAM` (`0x20`) → enciende (`P0 ≠ 0`) o apaga (`P0 = 0`) el envío de `DATA_IQ` y responde `ADC_ACK` (`0xA0`) con el mismo SEQ y `P0` = estado resultante.
   - Otro TYPE → `ERROR` 2.

## 5. Protocolo de tramas

### 5.1 Formato (9 bytes fijos, los dos sentidos, big-endian)

| Byte | Campo | Valor |
|---|---|---|
| 0 | SYNC1 | `0xA5` |
| 1 | SYNC2 | `0x5A` |
| 2 | TYPE | tipo de trama |
| 3 | SEQ | 0–255 |
| 4–7 | PAYLOAD | 4 bytes según el TYPE |
| 8 | CRC8 | sobre los bytes 2 a 7 |

### 5.2 Tipos

| TYPE | Sentido | Nombre | Payload |
|---|---|---|---|
| `0x01` | PC → PIC | PING | 4 bytes libres |
| `0x81` | PIC → PC | PONG | copia del PING; mismo SEQ |
| `0x20` | PC → PIC | ADC_STREAM | `P0` = 0 detener, ≠ 0 iniciar |
| `0xA0` | PIC → PC | ADC_ACK | `P0` = estado (0 o 1); mismo SEQ de la petición |
| `0xA1` | PIC → PC | DATA_IQ | I en `P0:P1`, Q en `P2:P3` (12 bits en 16, byte alto primero). SEQ propio que sube en cada muestra, **aunque la trama no quepa**, para que el PC vea el salto |
| `0x7F` | PIC → PC | ERROR | `P0` = código: 1 CRC malo, 2 TYPE desconocido, 3 bytes perdidos (RX lleno), 4 framing/overrun. SEQ de la trama culpable, o 0 si no la hay |

Convención: las peticiones del PC usan TYPE < `0x80` y la respuesta es `petición | 0x80`; ERROR es la excepción.

### 5.3 CRC-8

- Polinomio `0x07` (x⁸ + x² + x + 1), valor inicial `0x00`, sin reflejo, sin XOR final (CRC-8/SMBUS).
- Vector de prueba: `"123456789"` → `0xF4`.
- Implementación bit a bit (`crc8.c`):

```c
uint8_t crc8(const uint8_t *data, uint8_t len) {
    uint8_t crc = 0x00, i, bit;
    for (i = 0; i < len; i++) {
        crc ^= data[i];
        for (bit = 0; bit < 8; bit++)
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
    }
    return crc;
}
```

### 5.4 Tramas de ejemplo (CRC verificado)

| Qué | Bytes |
|---|---|
| Encender el envío (PC → PIC) | `A5 5A 20 00 01 00 00 00 2D` |
| Respuesta: ADC_ACK encendido | `A5 5A A0 00 01 00 00 00 C1` |
| Apagar el envío | `A5 5A 20 01 00 00 00 00 59` |
| Respuesta: ADC_ACK apagado | `A5 5A A0 01 00 00 00 00 B5` |
| PING, SEQ 1, payload `11 22 33 44` | `A5 5A 01 01 11 22 33 44 B2` |
| PONG correspondiente | `A5 5A 81 01 11 22 33 44 5E` |
| DATA_IQ: I = 2048, Q = 2051, SEQ 5 | `A5 5A A1 05 08 00 08 03 02` |
| ERROR 1 (CRC malo) de una trama con SEQ 1 | `A5 5A 7F 01 01 00 00 00 37` |

Conversión a voltios en el PC: V = código × 3,7 / 4096 (1 LSB = 0,903 mV; 2048 = 1,85 V).

## 6. Modos de `main.c`

| `MODO_PRUEBA_ADC` | `MODO_ECO` | Qué hace el bucle |
|---|---|---|
| 1 | – | solo manda texto `I:…,Q:…\r\n` para el Serial Plotter; no lee RX |
| 0 | 1 | eco crudo: devuelve cada byte que llega (pruebas 1.5–1.7). Pérdidas en `echoBytesLost` |
| 0 | 0 | **protocolo**: `protocol_task()` + envío de `DATA_IQ` si está encendido |

Se cambian con `#define` en `main.c` y hay que recompilar.

## 7. Lado del PC

- **Abrir el puerto:** 115200, 8 bits, sin paridad, 1 de parada, sin control de flujo. En LabVIEW (VISA Configure Serial Port): **Enable Termination Char = False**, porque la trama es binaria y un dato puede valer `0x0A`.
- **Leer:**
  1. Acumular bytes en un buffer y buscar `A5 5A`.
  2. Cuando haya 9 bytes desde ahí, comprobar el CRC.
  3. Si es bueno, usar la trama y quitarla del buffer. Si es malo, descartar solo el primer byte y volver a buscar.
  4. Revisar que el SEQ de `DATA_IQ` suba de 1 en 1 (un salto = tramas perdidas).
- **Escribir:** mandar los 9 bytes de un comando de una vez y esperar la respuesta con el mismo SEQ.
- **Herramienta existente:** `herramientas/prueba_protocolo.py` (Python + pyserial) arma tramas, calcula el CRC por su cuenta y corre las pruebas 2.1 a 2.13. Su clase `Lector` es un ejemplo de parser del lado del PC.

## 8. Contadores para depurar (variables `volatile` visibles en MPLAB X)

| Variable | Archivo | Cuenta |
|---|---|---|
| `protoTxDropped` | `protocol.c` | tramas que no cupieron en el buffer de TX |
| `adcTramasPerdidas` | `main.c` | tramas `DATA_IQ` que no cupieron |
| `adcParesPerdidos` | `adc_iq.c` | pares del ADC que main no alcanzó a leer antes del siguiente |
| `echoBytesLost` | `main.c` | bytes que el eco no pudo devolver |
| `adcPruebaLineasPerdidas` | `adc_prueba.c` | líneas de texto descartadas en el modo de prueba |
| `eusart1RxOverflow` | `eusart1.c` | aviso de bytes de RX descartados (lo consume `EUSART1_get_overflow()`) |

Todos deben quedar en 0 en una captura válida.

## 9. Cosas a tener en cuenta

- **La línea va al 78 %** con `DATA_IQ` a 1000 tramas/s: quedan ≈ 210 µs libres por milisegundo. Una respuesta (PONG, ADC_ACK, ERROR) cabe, porque el buffer de TX guarda 7 tramas.
  - Si el buffer llega a llenarse, tarda ≈ 25 ms en vaciarse: entran 9 bytes por milisegundo y salen 11,5.
- **Prioridad de interrupciones** (`interrupt_manager.c`, un solo vector y sin anidamiento): se revisa primero TX, luego RX y luego el ADC. Una ISR de TX que coincide con la del ADC retrasa unos 6 µs el arranque de la conversión de Q.
- **El PIC no tiene timeout entre bytes:** una trama a medias se completa con los bytes que lleguen después. Si se cortó, la trama siguiente sale con CRC malo y el parser resincroniza solo.
- **ASCII frente a binario:** el protocolo es binario por pedido del profesor (el ASCII es pesado para el procesador y no cabe a 1000 muestras/s). El texto solo se usa en el arranque y en el modo de prueba.
- Lo marcado [ESTIMADO] no se ha medido; se mide con un pin marcador y el osciloscopio.
