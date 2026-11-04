# VI de LabVIEW para recibir y decodificar la trama de 9 bytes (Micro 3)

Guía para armar en **LabVIEW 2021** (sirve igual en versiones más nuevas) un VI que:
1. abre el puerto, enciende el envío del PIC y recibe las tramas `DATA_IQ`;
2. busca `A5 5A`, comprueba el CRC-8 y separa I, Q y SEQ;
3. cuenta tramas perdidas y con CRC malo;
4. grafica I y Q en voltios y guarda todo en un CSV.

Solo usa funciones estándar de LabVIEW, VISA y un *Formula Node* para el CRC.

## 0. La trama (recordatorio)

`A5 5A | TYPE | SEQ | P0 P1 P2 P3 | CRC8`, siempre 9 bytes.

| TYPE | Qué es | Payload |
|---|---|---|
| `A1` | DATA_IQ | I = P0·256 + P1, Q = P2·256 + P3 (0 a 4095) |
| `A0` | ADC_ACK | P0 = estado del envío (0 o 1) |
| `7F` | ERROR | P0 = código (1 CRC, 2 TYPE, 3 RX lleno, 4 framing/overrun) |
| `81` | PONG | copia del PING |

- CRC-8: polinomio `0x07`, valor inicial 0, sobre los bytes 2 a 7.
- Voltios: V = código × 3,7 / 4096.
- Comandos fijos (CRC ya calculado):
  - **Encender el envío:** `A5 5A 20 00 01 00 00 00 2D`
  - **Apagar el envío:** `A5 5A 20 01 00 00 00 00 59`

Al arrancar, el PIC manda el texto `Micro3 listo\r\n`; el VI lo ignora solo, porque no empieza con `A5 5A`. El firmware debe estar con `MODO_PRUEBA_ADC = 0` y `MODO_ECO = 0`.

## 1. Panel frontal

| Control / indicador | Tipo | Uso |
|---|---|---|
| `Puerto` | VISA resource name | el COM del CP2102 |
| `Muestras a capturar` | numérico I32, valor 10000 | se detiene al llegar |
| `STOP` | botón booleano | detener a mano |
| `Archivo CSV` | file path | dónde guardar |
| `Gráfica I-Q` | *Waveform Chart*, 2 curvas, eje Y de 0 a 3,7 V | I y Q en voltios |
| `Tramas OK` | numérico I32 | tramas DATA_IQ válidas |
| `CRC malos` | numérico I32 | tramas descartadas por CRC |
| `Perdidas (SEQ)` | numérico I32 | saltos del SEQ |
| `Último ERROR` | numérico U8 | último código recibido en una trama 7F |
| `Envío encendido` | LED | se prende con el ADC_ACK |

## 2. Diagrama de bloques, de izquierda a derecha

### 2.1 Abrir el puerto (antes del bucle)

1. **VISA Configure Serial Port** (*Instrument I/O → Serial*):
   - baud rate = **115200**, data bits = **8**, parity = **None**, stop bits = **1.0**, flow control = **None**
   - **enable termination char = False**. Importante: la trama es binaria y un byte puede valer `0x0A`; si queda en True, VISA corta la lectura ahí.
   - timeout = 1000 ms
2. **VISA Set I/O Buffer Size**: mask = 16 (recepción), size = 16384.
3. **VISA Flush I/O Buffer**: mask = 16, para tirar lo que haya llegado antes.
4. **Encender el envío**:
   - Una constante de arreglo U8 con `A5 5A 20 00 01 00 00 00 2D`. Para escribirla en hexadecimal: clic derecho en un elemento → *Display Format* → Hexadecimal.
   - **Byte Array To String** → **VISA Write**.

### 2.2 Bucle principal (*While Loop*)

**Shift registers** (inicializarlos afuera, a la izquierda):

| Shift register | Tipo | Valor inicial |
|---|---|---|
| `buffer` | string | vacío |
| `I`, `Q` | arreglo U16 | vacío |
| `SEQ` | arreglo U8 | vacío |
| `ok`, `crcMal`, `perdidas` | I32 | 0 |
| `seqAnterior` | I32 | −1 |

**Dentro del bucle:**
1. **Property Node** (clase VISA) → `Bytes at Port`.
2. **VISA Read** con byte count = `Bytes at Port`. Si vale 0 no pasa nada: devuelve un string vacío.
3. **Concatenate Strings**: `buffer` + lo leído → `buffer nuevo`.
4. **Bucle interno de extracción** (otro *While Loop*, con shift register del buffer; ver 2.3). Sale con el buffer sobrante y los arreglos actualizados.
5. **Wait (ms)** = 10. En 10 ms llegan unos 90 bytes, sobra con el buffer de 16 kB.
6. **Condición de paro:** `STOP` **OR** (*Array Size* de `I` ≥ `Muestras a capturar`).

### 2.3 Bucle interno: sacar todas las tramas completas del buffer

Repetir mientras se saque una trama:

1. **Search/Split String** (*String*) sobre el buffer, con search string = constante `A55A` en *Hex Display* (clic derecho en la constante → *Hex Display*). Salidas: `match + rest` y `offset of match`.
2. **Case structure** sobre `offset of match = −1`:
   - **No hay encabezado:** dejar solo el último byte del buffer (podría ser la mitad `A5` de un encabezado) con **String Subset** (offset = largo − 1) y **parar** el bucle interno.
   - **Hay encabezado:** seguir con `match + rest`, que empieza en `A5`.
3. Si **String Length** de `match + rest` < 9: guardar `match + rest` como buffer y **parar** (la trama está incompleta, se termina en la próxima lectura).
4. Si hay 9 o más bytes:
   - **String Subset** (offset 0, length 9) → **String To Byte Array** → arreglo `t` de 9 U8.
   - **Array Subset** de `t` (index 2, length 6) → entra al **Formula Node del CRC** (sección 3) → `crc`.
   - Comparar `crc` con **Index Array** `t[8]`.
5. **Case structure** sobre «CRC igual»:
   - **CRC bueno:**
     - **Case structure** sobre el TYPE `t[2]` (*Index Array*). Poner los casos en hexadecimal: clic derecho en el selector → *Radix* → Hex.
       - **Caso `A1` (DATA_IQ):**
         - `I` = **Join Numbers** (`t[4]` alto, `t[5]` bajo) → U16 y `Q` = **Join Numbers** (`t[6]`, `t[7]`). *Join Numbers* está en *Numeric → Data Manipulation*.
         - Pérdidas: si `seqAnterior` ≥ 0, saltos = (`t[3]` − `seqAnterior` − 1 + 256) mod 256, con **Quotient & Remainder** entre 256; sumarlo a `perdidas`.
         - `seqAnterior` = `t[3]`.
         - **Build Array**: agregar I a `I`, Q a `Q` y `t[3]` a `SEQ`; `ok` + 1.
       - **Caso `A0` (ADC_ACK):** `Envío encendido` = (`t[4]` ≠ 0).
       - **Caso `7F` (ERROR):** `Último ERROR` = `t[4]`.
       - **Default:** no hacer nada.
     - Quitar los 9 bytes: **String Subset** de `match + rest` desde el offset 9 (sin largo = hasta el final) → nuevo buffer. Seguir en el bucle interno.
   - **CRC malo:** `crcMal` + 1 y quitar **solo el primer byte** (String Subset desde el offset 1). Así se resincroniza igual que el PIC: si el `A5` era falso, la trama verdadera puede empezar dentro de esos bytes. Seguir.

### 2.4 Gráfica en vivo

Dentro del bucle principal:
- Tomar los valores nuevos de I y Q de esa vuelta, convertirlos a voltios (× 3,7 / 4096) y mandarlos a la **Waveform Chart** con **Bundle** (I, Q), o con *Build Array* de dos arreglos para dibujar varios puntos por vuelta.
- Poner el *Chart History Length* en 10 000 o más.

### 2.5 Al salir del bucle

1. **Apagar el envío:** constante `A5 5A 20 01 00 00 00 00 59` → Byte Array To String → **VISA Write**.
2. **VISA Close**.
3. **Guardar:** **Build Array** de 2D con `SEQ`, `I` y `Q` (convertidos a DBL), transponer, y **Write Delimited Spreadsheet** con delimitador `,` en `Archivo CSV`.
4. **Simple Error Handler** al final de la cadena de error.

## 3. Formula Node del CRC-8

Crear un **Formula Node** (*Mathematics → Scripts & Formulas*):
- Entrada `d`: arreglo U8 de 6 elementos (los bytes 2 a 7).
- Salida `crc`: I32.

Pegar este código:

```c
int32 c, i, j;
c = 0;
for (i = 0; i < 6; i++) {
    c = c ^ d[i];
    for (j = 0; j < 8; j++) {
        if (c & 128)
            c = ((c << 1) ^ 7) & 255;
        else
            c = (c << 1) & 255;
    }
}
crc = c;
```

Se usan números decimales (128 = 0x80, 7 = 0x07, 255 = 0xFF) para no depender de cómo el Formula Node lee los hexadecimales.

**Probarlo antes de usarlo:**

| Entrada `d` (hex) | `crc` esperado |
|---|---|
| `A1 05 08 00 08 03` | 2 (`0x02`) |
| `20 00 01 00 00 00` | 45 (`0x2D`) |
| `20 01 00 00 00 00` | 89 (`0x59`) |
| `A0 00 01 00 00 00` | 193 (`0xC1`) |

**Alternativa sin Formula Node (G puro):**
- *For Loop* de 6 vueltas sobre `d`, con un shift register `c` (U8) iniciado en 0.
- Adentro: `c` **XOR** `d[i]`, y otro *For Loop* de 8 vueltas con shift register:
  - si (`c` AND 0x80) ≠ 0 → (`c` **Logical Shift** 1) XOR 0x07
  - si no → `c` Logical Shift 1
  - Usar **Select** para elegir.
- En U8 el desplazamiento descarta solo el bit que sale.

## 4. Cómo probarlo por partes

1. **Sin PIC:** conectar el TX y el RX del CP2102 entre sí (lazo), mandar a mano la trama `A5 5A A1 05 08 00 08 03 02` varias veces. El VI debe mostrar I = 2048, Q = 2051 y CRC malos = 0. Cambiar el último byte a `03` y comprobar que sube `CRC malos`.
2. **Con el PIC:** al correr, `Envío encendido` debe prenderse (llega el ADC_ACK) y `Tramas OK` debe subir unas 1000 por segundo, con `Perdidas (SEQ)` = 0.
3. **Comparar** con `herramientas/prueba_protocolo.py`, que arma y revisa las mismas tramas en Python.

## 5. Errores típicos

| Síntoma | Causa probable |
|---|---|
| Lee pocos bytes y se corta | *enable termination char* quedó en True |
| Todo sale con CRC malo | puerto a otro baud, o bytes en orden distinto (revisar la constante hex) |
| SEQ salta mucho | el bucle principal es lento (quitar gráficas pesadas, subir el buffer) o el PIC pierde tramas (`adcTramasPerdidas`) |
| No llega nada | JP_TX/JP_RX abiertos, J5 al revés, o el firmware en `MODO_PRUEBA_ADC = 1` (manda texto, no tramas) |
| Valores de 0 a 65535 raros | I o Q armados con los bytes al revés: el byte alto va primero |

## 6. Para el histograma del periodo

Si se activa el `MODO_PERIODO` propuesto (el campo Q lleva el periodo medido con Timer1, ≈ 16 000 cuentas de 62,5 ns), este mismo VI sirve tal cual:
1. Juntar 10 001 valores de Q y quitar el primero.
2. Multiplicar por 62,5 para pasar a ns.
3. Usar **Histogram.vi** y **Std Deviation and Variance.vi** (*Mathematics → Probability & Statistics*).

No convertir Q a voltios en ese modo.
