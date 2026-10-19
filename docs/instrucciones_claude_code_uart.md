# Instrucciones para Claude Code: adaptar el EUSART de MCC (tarea T-03)

Proyecto Micro 3 · 2026-10-19 · Aprobado por la persona en el chat del 2026-10-19 (cambios C-06 a C-10 de `docs/revision_codigo_mcc.md`).

> **Qué se pide:** modificar a mano el código que MCC generó para la UART, de modo que **no bloquee**, use **buffers circulares con cabeza y cola (sin contadores)** y ofrezca `tx_DAT` y `tx_string`. El código tiene que quedar **muy comentado**, porque la persona lo va a leer línea por línea para aprenderlo y sustentarlo.
>
> **Qué NO se pide:** no cambiar pines, reloj, baud, bits de configuración, `pin_manager.c`, `mcc.c` ni `device_config.c`. No tocar `Micro_Firmware.mc3`. No agregar ADC, PWM ni protocolo de tramas.

## 0. Antes de empezar (lo hace la persona, no Claude Code)

1. En MCC, ventana EUSART1: tamaño de buffer **TX = 64** y **RX = 32** (cambio C-05). Pulsar *Generate* **una sola vez**.
2. Hacer commit de ese estado ("base MCC con buffers 64/32").
3. **Después de empezar este trabajo no se vuelve a pulsar *Generate***: MCC reescribe sus archivos y borraría todo lo que se haga a mano.

Claude Code: si al abrir `eusart1.c` los tamaños siguen en 8, **detente** y pídele a la persona que haga el paso 1.

## 1. Archivos que se modifican

| Archivo (en `firmware/Micro_Firmware.X/`) | Qué cambia |
|---|---|
| `mcc_generated_files/eusart1.c` | Núcleo del trabajo (secciones 3 y 4) |
| `mcc_generated_files/eusart1.h` | Prototipos nuevos y quitar los obsoletos |
| `mcc_generated_files/interrupt_manager.c` | Llamar directo a las dos ISR (sección 5) |
| `main.c` | Habilitar interrupciones y programa de eco para probar (sección 6) |

## 2. Estilo de comentarios (obligatorio)

Todo el código nuevo o cambiado lleva comentarios en **español**, para alguien que está aprendiendo.

- **Encabezado de cada función:** para qué sirve, quién la llama (`main` o la ISR), parámetros, qué devuelve, qué variables toca.
- **Cada línea o grupo de 2–3 líneas** lleva un comentario con **qué hace y por qué**.
- **Nombres de registros y bits:** la primera vez que aparecen, decodificarlos. Ejemplo: `PIE3bits.TX1IE` = *Transmit 1 Interrupt Enable*, el interruptor que permite que el fin de envío interrumpa.
- **Diagrama del buffer circular** en un comentario grande al inicio de la sección de buffers (cabeza, cola, vacío, lleno, la capacidad útil de N−1).
- **Etiqueta de cambio** en cada bloque modificado: `// [C-07] ...`, con el ID de `docs/revision_codigo_mcc.md`, para que la persona encuentre lo que cambió.
- **No dejar código viejo comentado.** Lo viejo está en git.
- **Nombres de registros y bits:** comprobarlos en `pic16f18426.h` (XC8 2.40) y en el datasheet, no de memoria. Lo que ya usa MCC en el archivo (por ejemplo `PIE3bits.TX1IE`) se puede reutilizar.

Ejemplo del nivel de detalle esperado:

```c
/* [C-08] Lleno si el siguiente a la cabeza es la cola.
 * Se usa "& MASK" en vez de "% SIZE" porque SIZE es potencia de 2:
 * una sola instrucción en lugar de una división por software. */
uint8_t next = (uint8_t)((eusart1TxHead + 1) & EUSART1_TX_MASK);
if (next == eusart1TxTail) {
    return false;   /* Buffer lleno: no se bloquea, se avisa al llamador */
}
```

## 3. Estructura de datos (cambio C-09)

- Tamaños: `EUSART1_TX_BUFFER_SIZE` = 64 y `EUSART1_RX_BUFFER_SIZE` = 32 (potencias de 2). Definir también `EUSART1_TX_MASK` = SIZE − 1 y `EUSART1_RX_MASK` = SIZE − 1.
- Índices `volatile uint8_t`: `eusart1TxHead`, `eusart1TxTail`, `eusart1RxHead`, `eusart1RxTail`.
- **Quitar** `eusart1TxBufferRemaining` y `eusart1RxCount` (y sus `extern` en el `.h`).
- Vacío: `head == tail`. Lleno: `((head + 1) & MASK) == tail`. Capacidad útil: SIZE − 1 (63 en TX, 31 en RX).
- **Quién mueve cada índice** (regla clave, ponerla en el comentario):

| Buffer | Mueve la cabeza | Mueve la cola |
|---|---|---|
| TX | `main` (al escribir) | la ISR de TX (al enviar) |
| RX | la ISR de RX (al recibir) | `main` (al leer) |

- Al guardar un dato: **primero** escribir el byte en el arreglo, **después** avanzar el índice (la otra parte no debe ver el índice adelantado con el dato sin escribir).
- Agregar `volatile uint8_t eusart1RxOverflow;` (1 si se descartó un byte por buffer lleno) y `EUSART1_get_overflow()` que lo devuelve y lo borra.

## 4. Funciones: comportamiento exigido

### 4.1 `bool EUSART1_Write(uint8_t txData)` — cambio C-08

Pone un byte en el buffer de TX **sin esperar**.

1. Si el buffer está lleno → `return false`.
2. Escribir `txData` en `eusart1TxBuffer[eusart1TxHead]`.
3. Avanzar `eusart1TxHead` con la máscara.
4. Encender `PIE3bits.TX1IE` (así la ISR empieza o sigue enviando).
5. `return true`.

Se **elimina** la rama del MCC que escribía directo en `TX1REG`: todo pasa por el buffer. Con `TX1IE` encendido y `TX1REG` vacío, la ISR se dispara sola y manda el primer byte.

### 4.2 `bool EUSART1_Read(uint8_t *byte)` — cambio C-07

Saca un byte del buffer de RX **sin esperar**.

1. Si `eusart1RxHead == eusart1RxTail` → `return false` (no hay datos).
2. Copiar `eusart1RxBuffer[eusart1RxTail]` en `*byte`. Si existe `eusart1RxStatusBuffer`, copiar también el estado de ese byte a `eusart1RxLastError`, como hace el código actual.
3. Avanzar `eusart1RxTail` con la máscara.
4. `return true`.

Ya **no** se apaga `RC1IE` alrededor: no hay contador compartido.

### 4.3 `void EUSART1_Transmit_ISR(void)` — adaptar a C-09

- Si `eusart1TxHead != eusart1TxTail`: `TX1REG = eusart1TxBuffer[eusart1TxTail]`; avanzar la cola con la máscara.
- Si están iguales (nada que enviar): apagar `PIE3bits.TX1IE`. Comentar **por qué**: `TX1IF` vale 1 siempre que `TX1REG` esté vacío; con `TX1IE` encendido y sin datos, la ISR se repetiría sin parar.

### 4.4 `void EUSART1_RxDataHandler(void)` — adaptar a C-09

- Leer `RC1REG` **siempre** (leerlo baja `RC1IF`; si no se lee, la ISR se repite).
- Si el buffer de RX está lleno: **descartar** el byte y poner `eusart1RxOverflow = 1`. No pisar datos sin leer.
- Si no: guardar el byte en `eusart1RxBuffer[eusart1RxHead]` y avanzar la cabeza con la máscara.
- No tocar el manejo de `FERR`/`OERR` de `EUSART1_Receive_ISR`: se revisa en la parte 10.

### 4.5 `bool EUSART1_is_tx_ready(void)` y `bool EUSART1_is_rx_ready(void)`

- `is_tx_ready`: `true` si el buffer de TX **no** está lleno.
- `is_rx_ready`: `true` si `RxHead != RxTail`.

### 4.6 `bool EUSART1_is_tx_done(void)` — cambio C-10

`true` solo si el buffer de TX está vacío (`head == tail`) **y** `TX1STAbits.TRMT == 1`. (`TRMT` solo dice que el registro de desplazamiento terminó; el buffer puede tener bytes pendientes.)

### 4.7 `bool tx_DAT(const char *DATA, uint16_t size)` — firma pedida en clase

Encola un bloque de `size` bytes, **todo o nada**.

1. Si `DATA` es `NULL` → `false`. Si `size == 0` → `true`.
2. Espacio libre = `(eusart1TxTail - eusart1TxHead - 1) & EUSART1_TX_MASK`. Si `size` > espacio libre → `return false` **sin escribir nada**.
3. Copiar los `size` bytes al arreglo desde la cabeza, usando la máscara.
4. Avanzar `eusart1TxHead` **una sola vez** al final (la ISR ve el bloque completo).
5. Encender `TX1IE`. `return true`.

Comentar que leer `eusart1TxTail` aquí es seguro: es un byte, y si la ISR lo mueve mientras tanto el espacio solo aumenta.

### 4.8 `bool tx_string(const char *data)`

Si `data` es `NULL` → `false`. Si no, `return tx_DAT(data, (uint16_t)strlen(data))`. El `'\0'` **no** se envía. Incluir `<string.h>`.

### 4.9 `void EUSART1_Initialize(void)` — adaptar

Se mantiene todo lo de los registros (`BAUD1CON`, `RC1STA`, `TX1STA`, `SP1BRG`). Cambios:

- Quitar las llamadas a `EUSART1_SetTxInterruptHandler` y `EUSART1_SetRxInterruptHandler`.
- En el bloque de "estado del driver": los cuatro índices en 0, `eusart1RxOverflow = 0`; quitar `TxBufferRemaining` y `RxCount`.
- Los manejadores de error (`Framing`, `Overrun`, `Error`) **se dejan como están** por ahora.

## 5. `interrupt_manager.c` y los punteros (cambio C-06)

- En `eusart1.c`: eliminar las variables `EUSART1_TxDefaultInterruptHandler` y `EUSART1_RxDefaultInterruptHandler` y las funciones `EUSART1_SetTxInterruptHandler` y `EUSART1_SetRxInterruptHandler` (y sus `extern` y prototipos en el `.h`).
- En `interrupt_manager.c`: donde llama `EUSART1_TxDefaultInterruptHandler();` poner `EUSART1_Transmit_ISR();`, y donde llama al de recepción, `EUSART1_Receive_ISR();`.
- **No** cambiar nada más de ese archivo (la parte 7 aún no se revisa). Conservar la comprobación de bandera **y** habilitación (`TX1IE && TX1IF`, `RC1IE && RC1IF`).
- Verificar que `EUSART1_Transmit_ISR` y `EUSART1_Receive_ISR` estén declaradas en `eusart1.h`.
- Regla de XC8 (ya anotada en las notas): **no llamar desde la ISR y desde `main` a una misma función**. `EUSART1_Write`, `Read`, `tx_DAT` y `tx_string` se llaman solo desde `main`.

## 6. `main.c`: programa de prueba (eco)

- Llamar a `SYSTEM_Initialize()`.
- Habilitar interrupciones: `INTERRUPT_PeripheralInterruptEnable()` y luego `INTERRUPT_GlobalInterruptEnable()` (los macros que `main.c` ya trae comentados). Comentar qué es `PEIE` y qué es `GIE`.
- Enviar con `tx_string("Micro3 listo\r\n")` al arrancar.
- Bucle infinito: `if (EUSART1_Read(&b)) { EUSART1_Write(b); }`. Si `Write` devuelve `false`, el byte se pierde; comentar esa decisión (en la Fase 1 basta contarlo, no reintentar).
- Sin `while` que espere y sin `__delay_ms`.

## 7. Cómo verificar y qué reportar

1. Compilar con XC8 2.40 (*Clean and Build*). **Cero errores y cero warnings** nuevos. Pegar la salida si hay algo.
2. Revisar que no queden referencias a lo eliminado (`TxBufferRemaining`, `RxCount`, `SetTxInterruptHandler`, `SetRxInterruptHandler`): buscar en todo el proyecto.
3. **No marcar nada como `probada`.** Marcar `implementada, sin probar` en `docs/revision_codigo_mcc.md`.
4. Decir a la persona qué pruebas de `docs/protocolos_prueba.md` correr: **1.5** (eco de 1 byte), **1.6** (los 256 valores), **1.7** (ráfaga de 1000 bytes), **1.9** (desbordamiento del buffer), **1.13** (`TX1IE` solo con datos) y **1.14** (`tx_DAT`/`tx_string`). Las pruebas 1.1 a 1.4 (niveles y ancho de bit) las hace la persona con osciloscopio antes.
5. Actualizar `docs/estado.md` (fila nueva en el registro).
6. Listar al final **cada duda** que haya surgido y cualquier cosa que no se pudo verificar (por ejemplo, qué contiene `RC1REG` tras alternar `CREN`: no verificado, es la parte 10).
7. No hacer commit hasta que la persona lo pida (un commit por tarea, con el ID: `T-03: ...`).
