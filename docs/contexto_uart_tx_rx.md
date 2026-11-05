# Contexto: cómo transmitimos y cómo recibimos por la UART (Micro 3)

Documento de contexto para quien trabaje con el enlace serie del proyecto, sea una persona u otra IA. Describe el firmware tal como está en `firmware/Micro_Firmware.X` (archivos `mcc_generated_files/eusart1.c`, `adc_ascii.c/.h`, `adc_iq.c`, `main.c`, `mcc_generated_files/interrupt_manager.c`) y la placa Micro3.

**Resumen en una línea:** el PIC nunca espera a la UART. Cada 2 ms arma una línea de texto `2048,2051\r\n`, la copia a un buffer circular de 64 bytes y deja que la interrupción de TX la saque byte por byte. El PC solo lee líneas; el PIC no recibe comandos.

---

## 1. Capa física

| Elemento | Valor |
|---|---|
| Microcontrolador | PIC16F18426, VDD = 3,7 V, Fosc = 16 MHz (oscilador interno, ±2 %) |
| TX del PIC | **RC4** (pin 6), por PPS: `RC4PPS = 0x0F` (TX1) |
| RX del PIC | **RC5** (pin 5), por PPS: `RX1DTPPS = 0x15` (valor de reset) |
| Dirección de pines | `TRISC = 0x2F`: RC4 salida, RC5 entrada. RC5 es digital (`ANSELC` bit 5 = 0) |
| Nivel de entrada RX | **Schmitt** en el código (`INLVLC = 0x3F` en `pin_manager.c`): VIH = 0,8·VDD = 2,96 V. El diseño pedía TTL (`INLVLC5 = 0`, VIH = 1,73 V) y no se aplicó; con U6 en la placa el PIC recibe ≈ 3,7 V y alcanza |
| Adaptador al PC | módulo USB-serie **CP2102** a 3,3 V, por el conector **J5** (1 GND, 2 TX del módulo, 3 RX del módulo, 4 3V3, 5 GND). Confirmar que el orden coincide con el módulo |
| Traducción de niveles (placa Micro3) | **U5 SN74LVC1T45**: PIC_TX (3,7 V) → RX del CP2102 (3,3 V). **U6 SN74LVC1T45**: TX del CP2102 → PIC_RX. El lado de 3,3 V se alimenta del 3V3 del propio módulo |
| Jumpers | **JP_TX** debe estar cerrado para que lleguen los datos al PC (JP_RX hoy no hace falta: el PIC no recibe comandos) |
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
| `PIE3.RC1IE` | 1 | interrupción de RX encendida (el receptor queda configurado aunque hoy no se usa) |
| `PIE3.TX1IE` | 0 al inicio | la de TX solo se enciende cuando hay algo que mandar |

**Baud:** con `BRGH = 1` y `BRG16 = 1`, baud = Fosc / (4·(SPBRG + 1)) = 16 000 000 / (4·35) = **114 286 baud**. El PC se abre a 115200; la diferencia es −0,79 %, dentro de la tolerancia (±5,3 % teórico, ±2–3 % práctico). Peor caso con el ±2 % del oscilador interno: ≈ −2,8 %.

**Formato:** **8N1**: 1 bit de inicio, 8 de datos (primero el menos significativo), sin paridad, 1 de parada = 10 bits por byte.

| Tiempo | Valor |
|---|---|
| Bit | 8,75 µs (8,68 µs a 115200 exactos) |
| Byte | 87,5 µs |
| Línea `4095,4095\r\n` (11 bytes) | 0,96 ms |
| Capacidad de la línea | ≈ 11 430 bytes/s (a 114 286 baud) |

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
- Vacío: `head == tail`. Lleno: `(head + 1) & 63 == tail`. Se usan **63 bytes** (una posición queda libre para distinguir lleno de vacío): caben 5 líneas de 11 bytes.
- El índice avanza con `(i + 1) & (N − 1)`, una sola operación. Por eso los tamaños son potencias de 2 (hay un `#error` si no lo son).
- Main solo escribe `head` y la ISR solo escribe `tail`. Como cada índice es de 8 bits (una sola instrucción), no hace falta apagar interrupciones.

### 3.3 Funciones para transmitir

| Función | Qué hace | Si no cabe |
|---|---|---|
| `bool tx_DAT(const char *DATA, uint16_t size)` | encola un bloque completo: **todo o nada** (nunca sale media línea) | devuelve `false` y no encola nada |
| `bool tx_string(const char *data)` | encola una cadena terminada en `'\0'` (sin el `'\0'`); llama a `tx_DAT` con `strlen` | `false` |
| `bool EUSART1_Write(uint8_t b)` | encola un solo byte | `false` |
| `EUSART1_is_tx_done()` | `true` si el buffer está vacío **y** `TRMT = 1` (salió el último bit) | – |

Ninguna espera: si no hay espacio devuelven `false` al instante. El que llama **no reintenta**, solo cuenta la pérdida en un contador.

### 3.4 Qué transmite el PIC

- **Al arrancar:** el texto `Micro3 listo\r\n` (14 bytes). El PC lo ignora porque no tiene la forma `I,Q`.
- **Datos del ADC:** una línea `xxxx,xxxx\r\n` cada 2 pares del ADC = **500 líneas por segundo** (`ADC_PARES_POR_LINEA` = 2 en `adc_ascii.h`). Cada línea lleva una sola muestra de I y una de Q, sin promedio. El camino es `adc_iq_get()` (en `main`) → `adc_ascii_par(i, q)` → `a_decimal()` → `tx_string()`; el mismo par va antes a `pwm_desde_adc(i, q)` (T-06, `docs/especificacion_pwm.md`). Formato y cuentas: `docs/especificacion_protocolo.md`.

  | Paso | Duración |
  |---|---|
  | Salida de una línea de 11 bytes | 0,96 ms de cada 2 ms (48,1 % de la línea) |
  | Armado de la línea (`a_decimal` de dos números y `tx_string`) | ≈ 280 µs [ESTIMADO, sin medir] |

- El envío empieza solo al encender el PIC: el PC no tiene que mandar nada.

### 3.5 Interrupción de TX

`TX1IF` sube cada vez que `TX1REG` queda libre, o sea al empezar a salir cada byte. Al encender `TX1IE` con el registro vacío, la ISR entra enseguida y carga los dos primeros bytes (uno pasa directo al TSR). Después entra una vez por byte, cada 87,5 µs. Cuando no queda nada, apaga `TX1IE` para no entrar en bucle.

## 4. Recepción (PC → PIC)

El PIC **no lee** lo que llega por RX: no hay comandos. El receptor queda configurado por MCC y su interrupción guarda los bytes en el buffer de RX, pero `main` nunca los saca. Si el PC manda algo, el buffer se llena (31 bytes) y lo demás se descarta, sin afectar el envío.

### 4.1 Interrupción de RX (por si se usa después)

1. Limpia el estado del byte y revisa `RC1STA.FERR` (*framing error*: el bit de parada llegó en 0) y `RC1STA.OERR` (*overrun*: llegó un tercer byte con la FIFO de 2 llena).
2. Si hubo overrun, reinicia el receptor apagando y encendiendo `CREN` (si no, el EUSART deja de recibir).
3. Lee `RC1REG` (esto baja `RC1IF`) y guarda el byte en `eusart1RxBuffer[32]`, con su estado en `eusart1RxStatusBuffer[32]`. Los bytes con error también se guardan, marcados.
4. Si el buffer de RX está lleno (31 bytes útiles), **descarta** el byte y sube `eusart1RxOverflow`.

**Plazo:** la FIFO del chip guarda solo 2 bytes. Si la ISR no lee `RC1REG` antes de que termine un tercer byte, hay overrun: ≈ 2 bytes = **174 µs**. Ninguna otra ISR (ADC, TX) puede tardar tanto.

## 5. Modo de `main.c`

Hay un solo modo: en cada vuelta del `while(1)`, si `adc_iq_get()` entrega un par, `main()` llama a `pwm_desde_adc(i, q)` y a `adc_ascii_par(i, q)`. No hay constantes que cambiar.

## 6. Lado del PC

- **Abrir el puerto:** 115200, 8 bits, sin paridad, 1 de parada, sin control de flujo. En LabVIEW (*VISA Configure Serial Port*): **Enable Termination Char = True** y carácter de fin `\n` (`0x0A`), para que cada *VISA Read* devuelva una línea.
- **Leer:**
  1. Leer hasta `\n`.
  2. Separar con la forma `%d,%d` (*Scan From String* en LabVIEW, la expresión regular `^(\d{1,5}),(\d{1,5})\r?\n` (al inicio de la línea) en Python).
  3. Si la línea no tiene esa forma (por ejemplo, la primera, que llega cortada), descartarla y contarla como «línea mala».
- **Herramientas:** `herramientas/interfaz_micro3.py` (mediciones: `docs/interfaz_mediciones.md`) y `herramientas/receptor_micro3.py` (gráfica y CSV). El Serial Plotter del Arduino IDE también la dibuja directo.

## 7. Contadores para depurar (variables `volatile` visibles en MPLAB X)

| Variable | Archivo | Cuenta |
|---|---|---|
| `adcLineasPerdidas` | `adc_ascii.c` | líneas que no cupieron en el buffer de TX |
| `adcParesPerdidos` | `adc_iq.c` | pares del ADC que main no alcanzó a leer antes del siguiente |
| `eusart1RxOverflow` | `eusart1.c` | aviso de bytes de RX descartados (sube si el PC manda algo; no importa) |

`adcLineasPerdidas` y `adcParesPerdidos` deben quedar en 0 en una captura válida.

## 8. Cosas a tener en cuenta

- **La línea va al 48,1 %** con 500 líneas/s de 11 bytes: quedan ≈ 1 ms libre cada 2 ms. Mandar cada par (1000 líneas/s) cabría apenas: 0,96 ms de cada 1 ms (96 %), sin margen.
- **Prioridad de interrupciones** (`interrupt_manager.c`, un solo vector y sin anidamiento): se revisa primero TX, luego RX y luego el ADC. Una ISR de TX que coincide con la del ADC retrasa unos µs el arranque de la conversión de Q.
- **Sin suma de verificación ni número de secuencia:** una línea dañada solo se nota si pierde la forma `número,número`; una línea perdida no se nota. Por eso conviene revisar `adcLineasPerdidas` en el depurador y que el PC mida unas 500 líneas/s.
- Lo marcado [ESTIMADO] no se ha medido; se mide con RC2 y el osciloscopio (`ADC_PIN_PRUEBA` en `adc_iq.h`).
