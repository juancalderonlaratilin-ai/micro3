# LabVIEW paso a paso: recibir la trama de 9 bytes del PIC

Guía para alguien que casi no ha usado LabVIEW. Vamos bloque por bloque y cable por cable. Está hecha para **LabVIEW 2021**.

Vas a hacer **dos VIs**:
1. **CRC8.vi**: un VI pequeño que calcula el CRC de 6 bytes. Se usa dentro del otro.
2. **Receptor Micro3.vi**: el principal. Abre el puerto, recibe las tramas, saca I y Q y las grafica.

---

## PARTE 0. Lo mínimo que hay que saber de LabVIEW

### 0.1 Las dos ventanas

- **Panel frontal** (fondo gris): lo que ve el usuario (botones, números, gráficas).
- **Diagrama de bloques** (fondo blanco): donde se conectan los bloques. Es el «código».
- **Ctrl + E** cambia de una ventana a la otra.

### 0.2 Cómo encontrar un bloque

- **Clic derecho** en una parte vacía del diagrama de bloques abre la **paleta de funciones**. En esta guía la ruta se escribe así: **Programming → Numeric → Add** (entrar a Programming, luego a Numeric y tomar Add).
- **Atajo:** con el diagrama abierto, **Ctrl + Espacio** abre una búsqueda rápida: escribes el nombre (por ejemplo `Join Numbers`), Enter y haces clic donde lo quieres poner.

### 0.3 Cómo conectar un cable

- Pasa el mouse sobre la entrada o salida de un bloque: el cursor se vuelve un **carrete** 🧵. Haz clic, suelta, lleva el mouse al otro punto y haz clic otra vez.
- **Ctrl + H** abre la **Ayuda contextual**. Déjala abierta todo el tiempo: al pasar el mouse sobre un bloque te muestra el nombre de cada entrada y salida. **Es lo que más te va a ayudar.**
- Si un cable sale **punteado con una ✕ roja**, está roto. **Ctrl + B** borra todos los cables rotos de una vez.
- Si la flecha de **Run** (arriba a la izquierda) aparece **partida**, hay un error. Haz clic en ella y te dice cuál.

### 0.4 Colores de los cables (te dicen el tipo de dato)

| Color | Tipo |
|---|---|
| Rosado | texto (string) |
| Azul | número entero |
| Naranja | número decimal (DBL) |
| Verde | verdadero/falso (booleano) |
| Morado oscuro | el puerto VISA |
| Amarillo/café grueso | el cable de error |
| Línea gruesa o doble | arreglo (lista de varios valores) |

### 0.5 Crear constantes, controles e indicadores rápido

- Clic derecho sobre **una entrada** de un bloque → **Create → Constant**: crea una constante del tipo correcto ya conectada.
- Clic derecho sobre **una salida** → **Create → Indicator**: crea un indicador en el panel frontal ya conectado.
- **Cambiar el tipo de un número:** clic derecho sobre la constante → **Representation** → elegir (**U8** = byte de 0 a 255, **U16** = 0 a 65535, **I32** = entero normal).

### 0.6 Tres estructuras que vas a usar

- **While Loop** (Programming → Structures → While Loop): repite lo de adentro hasta que se cumpla la condición de paro. Se dibuja arrastrando un rectángulo. Abajo a la derecha tiene la **terminal de condición** (un circulito rojo 🔴 = «parar si es verdadero»).
- **For Loop** (Programming → Structures → For Loop): repite un número fijo de veces (la **N** de arriba a la izquierda).
- **Case Structure** (Programming → Structures → Case Structure): es un «if». Tiene dos páginas, **True** y **False**. Se cambia de página con las flechitas ◀ ▶ del título. La entrada **?** de la izquierda decide qué página se ejecuta.

### 0.7 Shift register (el que más confunde)

Es la «memoria» de un bucle: guarda un valor de una vuelta para la siguiente.
- **Crearlo:** clic derecho sobre el **borde** del bucle → **Add Shift Register**. Aparecen dos flechitas: **▼ a la izquierda** (sale el valor de la vuelta anterior) y **▲ a la derecha** (entra el valor nuevo).
- **Inicializarlo:** conecta una constante **desde afuera** del bucle a la flechita ▼ de la izquierda. Ese es el valor de la primera vuelta.
- **Leer el resultado final:** después de que el bucle termina, lo que salga de la flechita ▲ de la derecha (por fuera del bucle) es el último valor.

### 0.8 Túneles (los cuadritos en el borde)

Cuando un cable cruza el borde de un bucle o de un Case aparece un **túnel**.
- En un **Case**, un túnel de salida **relleno** ■ está bien conectado en todas las páginas. Si está **hueco** □, falta conectarlo en alguna página: revisa todas. *(Si no lo haces, LabVIEW manda «vacío» por ahí y pierdes los datos.)*
- En un **For Loop**, un túnel de entrada con el símbolo **[ ]** está **indexado**: en cada vuelta entra **un elemento** del arreglo. Un cuadrito **negro** ■ significa que entra el arreglo completo. Se cambia con clic derecho en el túnel → **Enable Indexing** / **Disable Indexing**.

---

## PARTE 1. El subVI CRC8.vi

**Qué hace:** recibe 6 bytes y devuelve el CRC-8 (polinomio 7, empezando en 0). Es la misma cuenta que hace el PIC.

Abre un VI nuevo: **File → New VI**.

### 1.1 Panel frontal (2 cosas)

**[P1] Control «bytes»** (arreglo de U8)
1. Clic derecho en el panel → **Modern → Array, Matrix & Cluster → Array**. Ponlo en el panel: es una caja vacía.
2. Clic derecho en el panel → **Modern → Numeric → Numeric Control**. Arrástralo **adentro** de la caja del arreglo.
3. Clic derecho sobre el número de adentro → **Representation → U8**.
4. Cambia el nombre (doble clic en la etiqueta) a `bytes`.

**[P2] Indicador «crc»**
1. Clic derecho en el panel → **Modern → Numeric → Numeric Indicator**.
2. Clic derecho → **Representation → U8**.
3. Nombre: `crc`.

### 1.2 Diagrama de bloques

Pasa al diagrama (**Ctrl + E**). Ya verás dos terminales: `bytes` y `crc`.

**[C1] For Loop grande** (Programming → Structures → For Loop). Dibújalo grande, que quepa otro adentro.

**[C2] Túnel de entrada indexado**
- Cable: salida de la terminal **`bytes`** → borde izquierdo de **[C1]**. Aparece un túnel.
- Fíjate que el túnel muestre **[ ]**. Si es un cuadrito negro: clic derecho en el túnel → **Enable Indexing**.
- *Así en cada vuelta entra un byte, y el bucle da 6 vueltas solo (no hace falta conectar la N).*

**[C3] Shift register del bucle grande**
- Clic derecho en el borde de **[C1]** → **Add Shift Register**. Aparecen ▼ (izquierda) y ▲ (derecha).

**[C4] Constante 0 (valor inicial del CRC)**
- Ponla **fuera** del bucle, a la izquierda: clic derecho → **Programming → Numeric → Numeric Constant**, escribe `0`.
- Clic derecho sobre ella → **Representation → U8**.
- Cable: **[C4]** → flechita **▼ izquierda de [C3]**.

**[C5] Exclusive Or** (Programming → Boolean → Exclusive Or). Va **dentro de [C1]**, pero **fuera** del bucle pequeño.
- Cable: flechita **▼ izquierda de [C3]** → entrada **x** de [C5].
- Cable: túnel **[C2]** (desde adentro) → entrada **y** de [C5].
- *Esto mezcla el byte nuevo con el CRC acumulado.*

**[C6] For Loop pequeño**, dentro de [C1], a la derecha de [C5].
- Clic derecho en la **N** de [C6] → **Create Constant** → escribe `8`. *(8 vueltas = 8 bits.)*

**[C7] Shift register del bucle pequeño**
- Clic derecho en el borde de **[C6]** → **Add Shift Register**.
- Cable: salida de **[C5]** → flechita **▼ izquierda de [C7]**.

Ahora **todo lo siguiente va DENTRO de [C6]**:

**[C8] And** (Programming → Boolean → And)
- Cable: flechita **▼ izquierda de [C7]** → entrada **x** de [C8].
- Entrada **y** de [C8]: clic derecho → **Create → Constant** → escribe `128` (128 es 0x80, el bit más alto) → clic derecho → **Representation → U8**.

**[C9] Not Equal To 0?** (Programming → Comparison → Not Equal To 0?)
- Cable: salida de **[C8]** → entrada **x** de [C9].
- *Da verdadero si el bit más alto vale 1.*

**[C10] Logical Shift** (Programming → Numeric → Data Manipulation → Logical Shift)
- Cable: flechita **▼ izquierda de [C7]** → entrada **x** de [C10]. *(El mismo cable puede ir a varios lugares: haz clic sobre el cable existente y llévalo al otro bloque.)*
- Entrada **y** de [C10]: **Create → Constant** → `1` (correr un bit a la izquierda).
- *Si en tu diagrama tenías un bloque de multiplicar (×), bórralo: este lo reemplaza.*

**[C11] Exclusive Or** (otro, Programming → Boolean → Exclusive Or)
- Cable: salida de **[C10]** → entrada **x** de [C11].
- Entrada **y** de [C11]: **Create → Constant** → `7` → **Representation → U8**.

**[C12] Select** (Programming → Comparison → Select). Tiene 3 entradas: **t** (arriba), **s** (al medio, verde) y **f** (abajo).
- Cable: salida de **[C11]** → entrada **t** (lo que sale si es verdadero).
- Cable: salida de **[C9]** → entrada **s** (la que decide).
- Cable: salida de **[C10]** → entrada **f** (lo que sale si es falso).
- Cable: salida de **[C12]** → flechita **▲ derecha de [C7]**.

**Borra el bloque `U16`** si lo tienes: todo debe quedar en U8.

Ahora fuera del bucle pequeño:

**[C13]** Cable: flechita **▲ derecha de [C7]** (por fuera de [C6]) → flechita **▲ derecha de [C3]**.

**[C14]** Cable: flechita **▲ derecha de [C3]** (por fuera de [C1]) → terminal **`crc`**.

### 1.3 Probarlo

1. En el panel frontal, escribe en `bytes`, de a uno: `161`, `5`, `8`, `0`, `8`, `3` (es `A1 05 08 00 08 03` en decimal).
2. Corre el VI (flecha **Run**). En `crc` debe salir **2**.
3. Prueba también `32`, `0`, `1`, `0`, `0`, `0` → debe salir **45**.

Si sale otra cosa: revisa que todas las constantes estén en U8 y que la entrada **s** del Select venga de [C9].

### 1.4 Convertirlo en subVI (para usarlo como un bloque)

1. En el panel frontal, arriba a la derecha, hay dos cuadros: el ícono y, a su izquierda, el **conector** (un cuadro dividido en casillas).
2. Haz clic en una casilla de la **izquierda** del conector y después clic en el control `bytes`. La casilla se pinta.
3. Haz clic en una casilla de la **derecha** del conector y después clic en el indicador `crc`.
4. Guarda: **File → Save As** → `CRC8.vi`, en la misma carpeta donde vas a guardar el VI principal.

---

## PARTE 2. El VI principal: Receptor Micro3.vi

### 2.0 Qué hace, en palabras

1. Configura el puerto COM a 115200.
2. Manda al PIC «empieza a enviar».
3. En un bucle: lee lo que llegó y lo agrega a un texto acumulado («buffer»).
4. En un bucle de adentro: mientras en el buffer haya una trama completa (empieza con A5 5A y tiene 9 bytes), la saca, revisa el CRC, saca I y Q y los guarda en dos listas. Después la borra del buffer.
5. Al terminar: manda «para», cierra el puerto y guarda un archivo CSV.

**Lo que ya tienes en tu diagrama y se queda:** el `Puerto COM`, el VISA Configure Serial Port, el VISA Write, el While Loop, el Property Node `Bytes at Port`, el `> 0`, el Case con el VISA Read, el Wait de 10 y el `stop`. Abajo te digo qué arreglar en cada uno y qué agregar.

### 2.1 Panel frontal

| Nombre | Cómo crearlo | Para qué |
|---|---|---|
| `Puerto COM` | ya lo tienes (VISA resource name) | elegir el COM del CP2102 |
| `stop` | ya lo tienes | parar a mano |
| `Muestras a capturar` | Modern → Numeric → Numeric Control, Representation I32, valor `10000` | parar solo |
| `CRC malos` | Numeric Indicator, I32 | tramas dañadas |
| `Muestras` | Numeric Indicator, I32 | cuántas llevas |
| `Gráfica` | Modern → Graph → Waveform Graph | ver I y Q en voltios |
| `Archivo CSV` | Modern → String & Path → File Path Control | dónde guardar (escribe una ruta como `C:\Users\Tatan\Documents\captura.csv`) |

### 2.2 Antes del bucle (la parte de la izquierda)

Aquí todo va en **cadena**: el cable **morado** (`VISA resource name out`) y el cable de **error** (`error out`) de cada bloque entran al siguiente (`VISA resource name` y `error in`). Siempre los dos.

**[A1] VISA Configure Serial Port** (ya lo tienes; Instrument I/O → Serial → VISA Configure Serial Port). Con **Ctrl + H** revisa a qué entrada va cada constante:
- `VISA resource name` ← tu control **`Puerto COM`**.
- `baud rate` ← `115200` ✓
- `data bits` ← `8` ✓
- `stop bits` ← `1.0` ✓
- `enable termination char` ← **F** (falso) ✓ *(Es importante: si queda en verdadero, LabVIEW corta la lectura cuando llega un byte 0x0A, que puede aparecer dentro de una trama.)*
- `timeout` ← el `200` que tienes: comprueba con Ctrl + H que esté en **timeout** y no en *termination char*.
- `parity` y `flow control` sin conectar está bien (quedan en «None»).

**[A2] VISA Set I/O Buffer Size** (Instrument I/O → Serial → VISA Set I/O Buffer Size; si no lo ves ahí, búscalo con Ctrl + Espacio). *Agranda la memoria de recepción de Windows.*
- `VISA resource name` ← `VISA resource name out` de [A1].
- `error in` ← `error out` de [A1].
- `mask` ← Create → Constant → `16` (recepción).
- `size` ← Create → Constant → `16384`.

**[A3] VISA Flush I/O Buffer** (Instrument I/O → Serial → VISA Flush I/O Buffer). *Borra lo que llegó antes (por ejemplo el texto «Micro3 listo»).*
- `VISA resource name` ← salida de [A2]; `error in` ← `error out` de [A2].
- `mask` ← Create → Constant → `16`.

**[A4] VISA Write** (ya lo tienes). **Cambia su constante de texto:**
- `VISA resource name` ← salida de [A3]; `error in` ← `error out` de [A3].
- `write buffer` ← la constante rosada. **Arréglala así:**
  1. Haz clic sobre ella y borra todo el texto.
  2. Clic derecho sobre ella → asegúrate de que esté marcado **Hex Display**.
  3. Escribe: `A55A2000010000002D` (18 caracteres, sin espacios: LabVIEW los agrupa solo).
  4. Debe verse como `A55A 2000 0100 0000 2D`.
- *La que tenías (`A55A 2001 0100 0000 040F`) tenía 10 bytes y un CRC equivocado: el PIC la rechazaba y nunca empezaba a enviar.*
- Tu indicador `return count` debe mostrar **9** al correr.

### 2.3 El bucle principal (While Loop) y sus memorias

**[B1] While Loop**: el que ya tienes. Agrándalo bastante hacia la derecha y hacia abajo: va a tener otro bucle adentro.

Ya tienes dos shift registers (el morado del VISA y el amarillo del error). **Agrega cinco más** (clic derecho en el borde → Add Shift Register, cinco veces), con estos valores iniciales conectados **desde afuera, a la flechita ▼ izquierda**:

| Shift register | Constante inicial (afuera) | Cómo hacer la constante |
|---|---|---|
| **[SR-buffer]** | texto vacío | Programming → String → Empty String Constant |
| **[SR-I]** | arreglo vacío de U16 | Programming → Array → Array Constant; mete adentro una Numeric Constant y ponla en **U16**. Déjala vacía (gris) |
| **[SR-Q]** | arreglo vacío de U16 | igual que el anterior (copia y pega) |
| **[SR-SEQ]** | arreglo vacío de U8 | igual, pero el número de adentro en **U8** |
| **[SR-CRCmal]** | `0` (I32) | Numeric Constant, Representation I32 |

### 2.4 Dentro del bucle principal: leer el puerto

**[B2] Property Node «Bytes at Port»**: ya lo tienes. Revisa:
- `reference` (arriba a la izquierda) ← flechita ▼ del shift register **morado** (VISA).
- `error in` ← flechita ▼ del shift register **amarillo** (error).

**[B3] Greater?** (`> 0`): ya lo tienes. `x` ← `Bytes at Port`; `y` ← constante `0`.

**[B4] Case Structure**: la que ya tienes. `?` ← salida de [B3].
- **Página True:** el **VISA Read** que ya tienes:
  - `VISA resource name` ← salida `reference out` de [B2] (entra por un túnel).
  - `error in` ← `error out` de [B2].
  - `byte count` ← `Bytes at Port` de [B2].
  - Salidas: `VISA resource name out`, `error out` y `read buffer` (el texto leído) → cada una sale por un túnel al borde derecho del Case.
- **Página False** *(esto es lo que te falta)*: cambia de página con las flechitas ◀ ▶ y conecta:
  - el túnel de entrada del VISA → directo al túnel de salida del VISA;
  - el túnel de entrada del error → directo al túnel de salida del error;
  - el túnel de salida del `read buffer` → clic derecho sobre él (desde adentro) → **Create → Constant** (queda un texto vacío).
- Revisa que los tres túneles de salida del Case queden **rellenos** ■.

**[B5] Concatenate Strings** (Programming → String → Concatenate Strings). *Pega lo nuevo al final de lo acumulado.*
- Entrada de arriba ← flechita ▼ de **[SR-buffer]**.
- Entrada de abajo ← túnel `read buffer` que sale de [B4].
- La salida de [B5] es el **buffer completo**. Va al bucle de adentro (2.5).

**[B6] Conecta el VISA y el error a sus memorias:**
- Túnel de salida del VISA de [B4] → flechita ▲ del shift register **morado**.
- Túnel de salida del error de [B4] → flechita ▲ del shift register **amarillo**.

### 2.5 El bucle de adentro: sacar las tramas

**[D1] While Loop interno**: dibújalo dentro de [B1], a la derecha de [B5]. Hazlo grande.

**[D2] Cambia su condición:** clic derecho sobre la terminal de condición (circulito rojo, abajo a la derecha) → **Continue if True**. Se vuelve una flechita circular. *Ahora el bucle sigue mientras le llegue «verdadero».*

**[D3] Cinco shift registers en [D1]** (clic derecho en el borde → Add Shift Register, cinco veces). Conexiones a sus flechitas ▼ izquierdas:

| Shift register de [D1] | Su ▼ izquierda recibe… |
|---|---|
| **[d-buffer]** | la salida de **[B5]** |
| **[d-I]** | la flechita ▼ de **[SR-I]** |
| **[d-Q]** | la flechita ▼ de **[SR-Q]** |
| **[d-SEQ]** | la flechita ▼ de **[SR-SEQ]** |
| **[d-CRCmal]** | la flechita ▼ de **[SR-CRCmal]** |

Y al revés, al final: la flechita ▲ derecha de cada uno de [D1], **por fuera** de [D1], va a la ▲ derecha del shift register del bucle grande que le corresponde (**[d-buffer] → [SR-buffer]**, **[d-I] → [SR-I]**, y así).

**Dentro de [D1]:**

**[D4] Search/Split String** (Programming → String → Additional String Functions → Search/Split String). *Busca A5 5A en el buffer.*
- `string` ← flechita ▼ de **[d-buffer]**.
- `search string/char` ← clic derecho → Create → Constant. Clic derecho sobre esa constante → **Hex Display** → escribe `A55A`.
- Salidas que vas a usar: **`match + rest`** (el texto desde el A5 en adelante) y **`offset of match`** (la posición del A5, o −1 si no lo encontró).

**[D5] Greater Or Equal To 0?** (Programming → Comparison → Greater Or Equal To 0?)
- `x` ← `offset of match` de [D4]. *(Verdadero = sí encontró A5 5A.)*

**[D6] String Length** (Programming → String → String Length)
- `string` ← `match + rest` de [D4].

**[D7] Greater Or Equal?** (Programming → Comparison → Greater Or Equal?)
- `x` ← salida de [D6]; `y` ← Create → Constant → `9`. *(Verdadero = hay 9 bytes o más.)*

**[D8] And** (Programming → Boolean → And)
- `x` ← salida de [D5]; `y` ← salida de [D7].
- *Verdadero = hay una trama completa.*
- La salida de [D8] **también** va a la terminal de condición de [D1] (la flechita circular del paso [D2]). *Así el bucle de adentro sigue mientras haya tramas completas y se detiene cuando no.*

**[D9] Case Structure** («¿hay trama completa?»): grande, dentro de [D1].
- `?` ← salida de [D8].
- Entran por túneles: `match + rest`, y las flechitas ▼ de [d-buffer], [d-I], [d-Q], [d-SEQ] y [d-CRCmal].
- Salen por túneles hacia las flechitas ▲ derechas de [d-buffer], [d-I], [d-Q], [d-SEQ] y [d-CRCmal].

#### Página **False** de [D9] (no hay trama completa): no se toca nada

Une con cables rectos cada túnel de entrada con su túnel de salida:
- ▼[d-buffer] → ▲[d-buffer] *(se deja el buffer igual, para completarlo en la próxima lectura)*
- ▼[d-I] → ▲[d-I]
- ▼[d-Q] → ▲[d-Q]
- ▼[d-SEQ] → ▲[d-SEQ]
- ▼[d-CRCmal] → ▲[d-CRCmal]

#### Página **True** de [D9] (hay trama completa)

**[E1] String Subset** (Programming → String → String Subset). *Corta los 9 bytes de la trama.*
- `string` ← túnel `match + rest`.
- `offset` ← constante `0`; `length` ← constante `9`.

**[E2] String To Byte Array** (Programming → String → Conversion → String To Byte Array).
- `string` ← salida de [E1]. *Sale un arreglo de 9 números U8: la trama.*

**[E3] Array Subset** (Programming → Array → Array Subset). *Los 6 bytes que entran al CRC.*
- `array` ← salida de [E2]; `index` ← constante `2`; `length` ← constante `6`.

**[E4] CRC8.vi** (tu subVI). Para ponerlo: clic derecho → **Select a VI…** → busca `CRC8.vi`.
- Entrada `bytes` ← salida de [E3]. Salida: `crc`.

**[E5] Index Array** (Programming → Array → Index Array). *Saca los bytes de uno en uno.*
- `array` ← salida de [E2].
- Primer `index` ← constante `2`.
- **Agrándalo hacia abajo** (pon el mouse en el borde de abajo del bloque y arrastra) hasta que tenga **7 salidas**. Sin conectar más índices, te da los elementos 2, 3, 4, 5, 6, 7 y 8, en ese orden:

| Salida | Byte | Qué es |
|---|---|---|
| 1.ª | 2 | TYPE |
| 2.ª | 3 | SEQ |
| 3.ª | 4 | I alto |
| 4.ª | 5 | I bajo |
| 5.ª | 6 | Q alto |
| 6.ª | 7 | Q bajo |
| 7.ª | 8 | CRC que mandó el PIC |

**[E6] Equal?** (Programming → Comparison → Equal?) *¿El CRC calculado es igual al recibido?*
- `x` ← `crc` de [E4]; `y` ← 7.ª salida de [E5].

**[E7] Case Structure** («¿CRC bueno?»), dentro de la página True de [D9].
- `?` ← salida de [E6].

##### Página **True** de [E7] (CRC bueno)

**[F1] Equal?** *¿Es una trama de datos (TYPE = A1)?*
- `x` ← 1.ª salida de [E5] (TYPE); `y` ← constante `161` (161 es A1), en U8.

**[F2] Case Structure** («¿es DATA_IQ?»).
- `?` ← salida de [F1].

En la **página True de [F2]** (es de datos):
- **[G1] Join Numbers** (Programming → Numeric → Data Manipulation → Join Numbers): `hi` ← 3.ª salida de [E5] (I alto); `lo` ← 4.ª salida (I bajo). Sale **I** (U16).
- **[G2] Join Numbers**: `hi` ← 5.ª salida (Q alto); `lo` ← 6.ª salida (Q bajo). Sale **Q** (U16).
- **[G3] Build Array** (Programming → Array → Build Array). Agrándalo a 2 entradas. Arriba ← ▼[d-I] (entra por túneles); abajo ← salida de [G1]. *(Agrega I al final de la lista.)* Salida → ▲[d-I].
- **[G4] Build Array**: arriba ← ▼[d-Q]; abajo ← salida de [G2]. Salida → ▲[d-Q].
- **[G5] Build Array**: arriba ← ▼[d-SEQ]; abajo ← 2.ª salida de [E5] (SEQ). Salida → ▲[d-SEQ].

En la **página False de [F2]** (no es de datos, por ejemplo un ADC_ACK): une cada lista de entrada con su salida sin tocarla (▼[d-I] → ▲[d-I], igual con Q y SEQ).

Todavía en la **página True de [E7]**:
- **[F3] String Subset** (borra la trama del buffer): `string` ← túnel `match + rest`; `offset` ← constante `9`; `length` **sin conectar** (así toma hasta el final). Salida → ▲[d-buffer].
- ▼[d-CRCmal] → ▲[d-CRCmal] directo (no cambia).

##### Página **False** de [E7] (CRC malo)

- **[F4] Increment** (Programming → Numeric → Increment): entrada ← ▼[d-CRCmal]; salida → ▲[d-CRCmal]. *(Cuenta una trama mala.)*
- **[F5] String Subset**: `string` ← `match + rest`; `offset` ← constante `1`; `length` sin conectar. Salida → ▲[d-buffer]. *(Borra solo el primer byte y vuelve a buscar, igual que hace el PIC.)*
- ▼[d-I] → ▲[d-I], ▼[d-Q] → ▲[d-Q], ▼[d-SEQ] → ▲[d-SEQ] directo.

**Cómo salen las cosas de la página True de [D9]:** todo lo que sale de los túneles de [E7] (buffer, I, Q, SEQ, CRC malos) va con un cable al túnel de salida de [D9] que le corresponde.

**Antes de seguir:** revisa en **cada página de cada Case** ([D9], [E7] y [F2]) que todos los túneles de salida estén **rellenos** ■. Si alguno está hueco □, falta un cable en alguna página.

### 2.6 Después del bucle de adentro (todavía dentro del bucle grande)

**[H1] Multiply** (Programming → Numeric → Multiply). *Pasa I a voltios.*
- `x` ← el cable que sale de ▲[d-I] (por fuera de [D1]) y va a ▲[SR-I]: haz clic sobre ese cable y saca una rama hasta [H1]. `y` ← constante `0.000903320` (= 3,7 / 4096).

**[H2] Multiply**: igual, con una rama del cable de Q (el que va de ▲[d-Q] a ▲[SR-Q]).

**[H3] Build Array**: 2 entradas: arriba ← [H1], abajo ← [H2]. *Queda una tabla de 2 filas (I y Q).*
- Salida → terminal **`Gráfica`**. *(La gráfica dibuja una curva por fila.)*

**[H4] Array Size** (Programming → Array → Array Size)
- `array` ← otra rama del mismo cable de I que va a ▲[SR-I]. Salida → indicador **`Muestras`**.

**[H5]** Una rama del cable que va de ▲[d-CRCmal] a ▲[SR-CRCmal] → indicador **`CRC malos`**.

**[H6] Greater Or Equal?**: `x` ← salida de [H4]; `y` ← control **`Muestras a capturar`**.

**[H7] Or** (Programming → Boolean → Or): `x` ← botón **`stop`**; `y` ← salida de [H6].
- Salida → terminal de condición (circulito rojo) de **[B1]**.
- *Si el `stop` ya estaba conectado directo al circulito, borra ese cable y pásalo por el Or.*

**[H8] Wait (ms) con 10**: el que ya tienes. Déjalo dentro de [B1], suelto.

### 2.7 Después del bucle grande (a la derecha, afuera)

Otra vez en cadena (morado y error):

**[K1] VISA Write** (Instrument I/O → VISA → VISA Write). *Le dice al PIC que pare.*
- `VISA resource name` ← flechita ▲ del shift register morado (por fuera de [B1]).
- `error in` ← flechita ▲ del shift register amarillo (por fuera).
- `write buffer` ← constante de texto con **Hex Display**: escribe `A55A20010000000059` (se ve `A55A 2001 0000 0000 59`).

**[K2] VISA Close** (Instrument I/O → VISA → VISA Close)
- `VISA resource name` ← salida de [K1]; `error in` ← `error out` de [K1].

**[K3] Simple Error Handler** (Programming → Dialog & User Interface → Simple Error Handler)
- `error in` ← `error out` de [K2]. *(Si algo falló, te muestra un mensaje.)*

**Guardar el CSV:**

**[K4] Build Array** (3 entradas):
- arriba ← ▲[SR-SEQ] (por fuera de [B1]);
- medio ← ▲[SR-I];
- abajo ← ▲[SR-Q].
- *Queda una tabla de 3 filas.*

**[K5] Write Delimited Spreadsheet** (Programming → File I/O → Write Delimited Spreadsheet)
- `file path` ← control **`Archivo CSV`**.
- `2D data` ← salida de [K4].
- `format` ← Create → Constant → escribe `%d` (números enteros).
- `delimiter` ← Create → Constant → escribe `,` (una coma).
- `transpose?` ← Create → Constant → **T**. *(Así queda en 3 columnas: SEQ, I, Q.)*

---

## PARTE 3. Probar

1. **Firmware:** en `main.c` deben estar `MODO_PRUEBA_ADC 0` y `MODO_ECO 0`. Compila y programa el PIC.
2. **Placa:** JP_TX y JP_RX cerrados; el CP2102 conectado en J5.
3. **En LabVIEW:** elige el COM en `Puerto COM`, escribe la ruta del CSV y presiona **Run**.
4. **Qué deberías ver:**
   - `return count` = 9;
   - `Muestras` subiendo unas 1000 por segundo;
   - `CRC malos` en 0;
   - la gráfica con dos líneas: con las entradas en reposo, cerca de 1,85 V (código ≈ 2048).
5. A las 10 000 muestras para solo, manda «para» al PIC y guarda el CSV.
6. **Revisar pérdidas:** abre el CSV en Excel. En la columna SEQ cada número debe ser el anterior + 1 (después de 255 vuelve a 0). Si salta, se perdieron tramas.

## PARTE 4. Si algo falla

| Lo que ves | Qué revisar |
|---|---|
| La flecha Run está partida | haz clic en ella: te dice qué bloque falla. Casi siempre es un túnel hueco □ en un Case o un cable sin conectar |
| `Muestras` no sube | ¿el PIC está en `MODO_PRUEBA_ADC 0`? ¿JP_TX/JP_RX cerrados? ¿`return count` = 9? |
| `CRC malos` sube mucho | revisa las constantes del CRC8.vi (U8, 128, 7, 1) y que `enable termination char` esté en F |
| Valores raros (miles) | en [G1]/[G2] se cruzaron `hi` y `lo`: el byte alto va en `hi` |
| La gráfica se pone lenta | normal pasadas muchas muestras; no afecta la captura |
| Error de VISA al empezar | otro programa (Arduino IDE, Python) tiene abierto el COM: ciérralo |
