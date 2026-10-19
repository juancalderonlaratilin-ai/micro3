# Guía: configurar MCC para obtener el código base de referencia

Proyecto Micro 3 · 2026-10-19 · Herramientas del equipo: MPLAB X 6, XC8 2.40 (ver `MASTER_PLAN.md`)

> **Para qué sirve este código:** es **referencia**, no firmware. El enunciado prohíbe MCC en el entregable. Se genera para (1) comparar nombres y valores de registros con lo que escribimos a mano, (2) ver cómo el profesor estructura el buffer circular con cabeza, cola y banderas, y (3) tener un ejemplo que compile y funcione para contrastar. Nada de `mcc_generated_files/` entra al firmware final.
>
> **Qué está verificado y qué no:** los *nombres de opciones y valores* de abajo salen de tu archivo `Micro_Firmware.mc3` y de los `.c` generados en clase (se leyeron). **La ubicación de cada casilla en las ventanas de MCC no está verificada**: no se ha visto la interfaz. Si algo no está donde dice la guía, mándame una captura y la corrijo.

## 0. Estado actual de tu proyecto (leído de los archivos)

| Cosa | Qué tiene | Consecuencia |
|---|---|---|
| `Micro_Firmware.mc3` (guardado después de la clase) | 16 MHz, 115200, `SPBRGL=34`, RX/TX por interrupción, buffers 8/8, RC4=TX, RC5=RX | La configuración ya casi es la que queremos |
| `mcc.c` y `eusart1.c` | Ya regenerados: `OSCFRQ=0x05`, `OSCCON1=0x60`, `SP1BRGL=0x22` (16 MHz, 115200) | Los valores de la sección 5 ya coinciden, salvo los buffers (siguen en 8/8) |
| Compilador del proyecto (`nbproject/configurations.xml`) | XC8 **2.40** | Es el de las herramientas del equipo: no hay que cambiar nada |
| Paquete de dispositivo (DFP) | `PIC16F1xxxx_DFP 1.9.163` | Anotarlo; define los nombres del `.h` |

## 1. Antes de abrir MCC: comprobar XC8 2.40

1. Clic derecho en el proyecto → *Properties* → *Conf: [default]* → *XC8 Global Options*. El compilador debe ser **2.40**.
2. *Clean and Build* una vez. Debe terminar sin errores.

> El código de MCC dice "probado con XC8 2.36 o superior"; 2.40 cumple.

## 2. Abrir MCC

1. Con el proyecto abierto: *Tools → Embedded → MPLAB Code Configurator* (o el ícono de MCC en la barra). Tu `.mc3` se carga solo.
2. En clase se usó **MCC clásico** (versión 1.81.8 según los comentarios generados). Se usa el mismo para que el resultado se parezca al del profesor.

## 3. Configuración que se pide

### 3.1 Oscilador (System Module / Internal Oscillator)

| Opción | Valor |
|---|---|
| Clock source (`NOSC`) | `HFINTOSC` |
| HF internal clock (`HFFRQ`) | `16_MHz` |
| Clock divider (`NDIV`) | `1` |
| External oscillator (`FEXTOSC`) | `OFF` ("Oscillator not enabled") |
| Valor de arranque (`RSTOSC`) | `HFINT1` ("HFINTOSC (1MHz)") |

`RSTOSC=HFINT1` es lo esperado: el chip arranca a 1 MHz y el código cambia a 16 MHz. Es lo mismo que decidimos hacer a mano.

### 3.2 Bits de configuración

| Bit | MCC (tu `.mc3`) | Nuestro diseño | ¿Cambiar? |
|---|---|---|---|
| `FEXTOSC` | OFF | OFF | no |
| `RSTOSC` | HFINT1 | 1 MHz de arranque | no |
| `CSWEN` | ON | ON | no |
| `FCMEN` | **ON** | sin definir | dejarlo, anotar como diferencia |
| `MCLRE` | ON | MCLR en RA3 | no |
| `PPS1WAY` | **ON** | OFF (propuesto) | dejarlo en la referencia; el diseño decide aparte |
| `BOREN` / `BORV` | ON / LO | BOR activo, VBOR sin decidir | dejarlo |
| `LVP` | ON | ON (obligatorio con SNAP) | **nunca** apagarlo |
| `WDTE` | OFF | OFF en desarrollo | no |
| `STVREN` | ON | ON | no |

### 3.3 EUSART1 (agregarlo desde *Device Resources*)

| Opción | Valor |
|---|---|
| Modo | Asincrónico (la plantilla guardada es `eusart_interrupt`) |
| Baud rate | `115200` (MCC debe mostrar **Error: −0.794 %**) |
| Transmisión | habilitada, 8 bits |
| Recepción | habilitada, recepción continua (`CREN`) |
| Interrupciones | *enabled* |
| Redirigir `stdio` | desactivado |
| Tamaño buffer TX | **64** (clase: 8) |
| Tamaño buffer RX | **32** (clase: 8) |

Los tamaños 64/32 son los del diseño (`docs/especificacion_diseno_uart.md`). Si MCC no los acepta, deja 8 y anótalo.

### 3.4 Pines (Pin Manager)

| Función | Pin |
|---|---|
| TX1 (llamado TX/CK en MCC) | **RC4** |
| RX1 (llamado RX/DT en MCC) | **RC5** |

Opcional, para la prueba de parpadeo: RA5 como salida con nombre `LED`.

## 4. Generar y compilar

1. Pulsa *Generate* en MCC. Debe reescribir `main.c` y `mcc_generated_files/`.
2. *Clean and Build* con XC8 2.40.
3. Si va a programarse: SNAP + *Make and Program Device*. El `main.c` generado no hace nada útil; el de la clase tenía lo del profesor.

## 5. Qué comprobar en el código generado

Abre los `.c` y confirma que **estos valores cambiaron**. Si no, MCC no regeneró.

| Archivo | Registro | Valor esperado | Antes (clase) |
|---|---|---|---|
| `mcc.c` | `OSCCON1` | `0x60` | `0x62` |
| `mcc.c` | `OSCFRQ` | `0x05` (16 MHz) | `0x02` |
| `eusart1.c` | `SP1BRGL` / `SP1BRGH` | `0x22` (34) / `0x00` | `0x01` / `0x00` |
| `eusart1.c` | `BAUD1CON` | `0x08` | `0x08` |
| `eusart1.c` | `RC1STA` | `0x90` | `0x90` |
| `eusart1.c` | `TX1STA` | `0x24` | `0x24` |
| `eusart1.c` | `EUSART1_TX_BUFFER_SIZE` / `RX` | 64 / 32 | 8 / 8 |
| `pin_manager.c` | `RC4PPS` | `0x0F` | `0x0F` |
| `pin_manager.c` | `RX1DTPPS` | `0x15` | `0x15` |

## 6. Diferencias con nuestro diseño (para revisar, no para copiar)

| Tema | Código de MCC | Nuestro diseño |
|---|---|---|
| Reloj y baud | los de arriba, tras regenerar | iguales (16 MHz, 115200, `SP1BRG=34`) |
| Entrada RX | `INLVLC=0x3F`: **Schmitt** en todos los pines | RC5 en **TTL** (`INLVLC5=0`), por el nivel del CP2102 a 3,7 V |
| RC4 (TX) | `ANSELC=0x1F`: queda analógico | `ANSELC4=0` (digital) |
| Lectura | `EUSART1_Read` y `Write` **bloquean** esperando | no bloquear: devolver `false`/sin dato |
| Tamaño de buffers | 8/8 | 32 RX / 64 TX |
| Sin protocolo | solo bytes | trama de 9 bytes con CRC-8 |
| `PPS1WAY` | ON | OFF (propuesto) |
| Funciones | `EUSART1_*` | `tx_DAT(const char*, uint16_t)` y `tx_string(const char*)` |

## 7. Reglas de uso

- `mcc_generated_files/` y el `.mc3` viven en `referencia_mcc/` (tarea T-01), **no** en el proyecto que se entrega.
- Si regeneras con otra configuración, no sobrescribas la referencia anterior sin anotar en `docs/estado.md` qué cambió.
- Para copiar un valor a mano, vuelve a comprobarlo en `pic16f18426.h` (XC8 2.40) o en el datasheet. MCC no es la fuente de verdad.

## 8. Revisión de `eusart1.c` (regenerado el 2026-10-19)

Lo que está bien y coincide con nuestro diseño:
- `BAUD1CON=0x08`, `RC1STA=0x90`, `TX1STA=0x24`, `SP1BRG=34`: 115200 con error −0,79 %.
- La ISR comprueba habilitación **y** bandera (`TX1IE && TX1IF`, `RC1IE && RC1IF`).
- `FERR` y `OERR` se leen antes de leer el dato; el overrun se limpia con el ciclo de `CREN`.
- Cabeza, cola y contador `volatile`; la interrupción se deshabilita un instante al tocar el contador compartido.
- `TX1IE` solo está en 1 mientras hay datos por enviar.

Lo que **no** sirve o hay que hacer distinto (no copiar):

| # | Qué pasa | Dónde | Por qué importa |
|---|---|---|---|
| 1 | `EUSART1_Read` y `EUSART1_Write` esperan en un `while` sin salida | `eusart1.c` 155 y 175 | El enunciado prohíbe bloquear; nosotros devolvemos `false` o "sin dato" |
| 2 | `EUSART1_RxDataHandler` **no comprueba si el buffer está lleno**: sigue escribiendo, pisa datos sin leer y `eusart1RxCount` puede pasar del tamaño | 240–248 | Pérdida silenciosa de bytes. Nosotros: si está lleno, descartar el byte y levantar una bandera de error |
| 3 | Tras un overrun, el manejador alterna `CREN` y luego `EUSART1_ErrorHandler` llama a `RxDataHandler`, que lee `RC1REG` y **guarda ese byte como dato válido** | 226–262 | Puede entrar un byte falso. Qué contiene `RC1REG` tras alternar `CREN` no está verificado en el datasheet; no suponer |
| 4 | Un error de trama (`FERR`) también guarda el byte como dato | 221–233 | El protocolo con CRC-8 lo descartaría, pero conviene contarlo |
| 5 | Buffers de 8 bytes con una trama de 9: no cabe una trama entera en RX, y en TX apenas | 56–57 | Diseño: RX 32, TX 64 (pendiente de cambiarlo en MCC) |
| 6 | Dos `if` independientes serían más seguros que `else if` para TX/RX | `interrupt_manager.c` | El `else if` atiende primero TX; se arregla solo porque `TX1IF` baja al cargar un byte, pero no hay que depender de eso |
| 7 | `main.c` generado **no activa** las interrupciones (`INTERRUPT_GlobalInterruptEnable` está comentado) | `main.c` | Con este `main`, nada de lo anterior funciona. Nosotros: `GIE` y `PEIE` en `system_init()` |
| 8 | `OSCILLATOR_Initialize` escribe `OSCCON1` y sigue sin esperar `OSCCON3[ORDY]` | `mcc.c` | Nosotros esperamos `ORDY` antes de configurar la UART |
| 9 | `FCMEN=ON` y `PPS1WAY=ON` en los bits de configuración | `device_config.c` | Diferencias con la propuesta (`PPS1WAY` OFF); se decide en el diseño, no aquí |

Nada de esto se ha compilado ni probado en el chip: es lectura del código.
