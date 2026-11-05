# LabVIEW paso a paso: recibir las líneas `I,Q` del PIC

Guía para alguien que casi no ha usado LabVIEW. Vamos bloque por bloque y cable por cable. Está hecha para **LabVIEW 2021**.

El PIC manda, sin que se le pida nada, 500 líneas por segundo así: `2048,2051\r\n` (formato: `docs/especificacion_protocolo.md`). El VI tiene que hacer cuatro cosas: abrir el puerto, leer una línea, separar los dos números y graficarlos. Es **un solo VI**, sin subVIs.

Antes de empezar, comprueba que llegan las líneas con el Serial Plotter del Arduino IDE o con `python receptor_micro3.py COM5` (`docs/receptor_python_y_serial_plotter.md`), y **ciérralos**: solo un programa a la vez puede tener abierto el COM.

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
- **Cambiar el tipo de un número:** clic derecho sobre la constante → **Representation** → elegir (**I32** = entero normal, **DBL** = decimal).

### 0.6 Tres estructuras que vas a usar

- **While Loop** (Programming → Structures → While Loop): repite lo de adentro hasta que se cumpla la condición de paro. Se dibuja arrastrando un rectángulo. Abajo a la derecha tiene la **terminal de condición** (un circulito rojo 🔴 = «parar si es verdadero»).
- **Case Structure** (Programming → Structures → Case Structure): es un «if». Tiene dos páginas, **True** y **False**. Se cambia de página con las flechitas ◀ ▶ del título. La entrada **?** de la izquierda decide qué página se ejecuta.

### 0.7 Shift register (el que más confunde)

Es la «memoria» de un bucle: guarda un valor de una vuelta para la siguiente.
- **Crearlo:** clic derecho sobre el **borde** del bucle → **Add Shift Register**. Aparecen dos flechitas: **▼ a la izquierda** (sale el valor de la vuelta anterior) y **▲ a la derecha** (entra el valor nuevo).
- **Inicializarlo:** conecta una constante **desde afuera** del bucle a la flechita ▼ de la izquierda. Ese es el valor de la primera vuelta.
- **Leer el resultado final:** después de que el bucle termina, lo que salga de la flechita ▲ de la derecha (por fuera del bucle) es el último valor.

### 0.8 Túneles (los cuadritos en el borde)

Cuando un cable cruza el borde de un bucle o de un Case aparece un **túnel**.
- En un **Case**, un túnel de salida **relleno** ■ está bien conectado en todas las páginas. Si está **hueco** □, falta conectarlo en alguna página: revisa todas. *(Si no lo haces, LabVIEW manda «vacío» por ahí y pierdes los datos.)*
- En un **While Loop**, un túnel de salida se puede volver **indexado** (clic derecho → **Enable Indexing**): junta un valor por vuelta en un arreglo. Aquí **no** se usa: deja los túneles normales.

---

## PARTE 1. Panel frontal

Ctrl + E hasta ver el fondo gris. Clic derecho en el panel para la paleta de controles.

| # | Qué poner | Ruta en la paleta | Nombre (doble clic en la etiqueta para cambiarlo) |
|---|---|---|---|
| P1 | Selector del puerto | Modern → I/O → **VISA resource name** | `Puerto` |
| P2 | Gráfica en el tiempo | Modern → Graph → **Waveform Chart** | `I y Q (V)` |
| P3 | Plano I-Q | Modern → Graph → **XY Graph** | `Plano I-Q` (opcional, PARTE 4) |
| P4 | Dos números | Modern → Numeric → **Numeric Indicator** (dos veces) | `I (código)` y `Q (código)` |
| P5 | Contador | Modern → Numeric → **Numeric Indicator** | `Líneas malas` |
| P6 | Botón de parar | Modern → Boolean → **Stop Button** | `STOP` |

En la Waveform Chart: clic derecho → **Properties** → pestaña *Scales* → eje Y de **0 a 3.7**, y desmarca *Autoscale*. Clic derecho → **Chart History Length** → `2000` (4 s a 500 líneas/s).

## PARTE 2. Abrir el puerto (fuera del bucle)

Ctrl + E para pasar al diagrama de bloques. Los terminales de P1 a P6 ya están ahí.

**B1. VISA Configure Serial Port** (Instrument I/O → Serial → VISA Configure Serial Port). Ponlo a la izquierda. Conecta en sus entradas (usa la Ayuda contextual, Ctrl + H):

| Entrada | Qué conectar |
|---|---|
| `VISA resource name` | el terminal `Puerto` (P1) |
| `baud rate` | constante `115200` |
| `data bits` | constante `8` |
| `parity` | clic derecho → Create → Constant → **None** |
| `stop bits` | clic derecho → Create → Constant → **1.0** |
| `flow control` | clic derecho → Create → Constant → **None** |
| `Enable Termination Char` | constante **True** (verde, T) |
| `termination char` | constante `10` (es `\n`; así cada lectura termina al final de una línea) |
| `timeout (10sec)` | constante `1000` (1 s: si en 1 s no llega nada, hay error y el VI para) |

**B2. VISA Flush I/O Buffer** (Instrument I/O → Serial → VISA Flush I/O Buffer). A la derecha de B1. Conecta `VISA resource name out` de B1 a su `VISA resource name`, y `error out` de B1 a su `error in`. En `mask` crea una constante y elige *Receive buffer*. Sirve para tirar lo que llegó antes de abrir el VI.

## PARTE 3. El bucle

**B3. While Loop.** Dibuja un rectángulo grande a la derecha de B2. Todo lo que sigue va **adentro**.

**B4. VISA Read** (Instrument I/O → Serial → VISA Read).
- `VISA resource name`: cable morado desde B2 (entra por un túnel en el borde del bucle).
- `error in`: cable de error desde B2.
- `byte count`: constante `50` (más largo que cualquier línea; la lectura termina antes, en el `\n`).
- `read buffer` (salida rosada): la línea, por ejemplo `2048,2051` + `\r\n`.

**B5. Scan From String** (Programming → String → Scan From String).
- `input string`: el `read buffer` de B4.
- `format string`: constante de texto **`%d,%d`** (clic derecho en la entrada → Create → Constant y escribe eso).
- Estira el bloque hacia abajo con el mouse hasta que tenga **dos** salidas `output`. Lo que sobra al final (`\r\n`) no importa.
- Conecta `output 1` al indicador `I (código)` y `output 2` a `Q (código)`.
- **No** conectes su `error out` al cable de error principal: una línea cortada daría error y pararía el VI. Ese error va al Case de B7.

**B6. Pasar a voltios.**
- Dos **Multiply** (Programming → Numeric → Multiply). En cada uno: una entrada desde `output 1` (o `output 2`) y la otra con la constante **`0.000903320`** (3,7 V / 4096).
- **Bundle** (Programming → Cluster → Bundle), con dos entradas: el voltaje de I arriba y el de Q abajo.
- La salida de Bundle va a la **Waveform Chart** `I y Q (V)`: dibuja dos curvas.

**B7. Contar las líneas malas.**
- Clic derecho en el borde del bucle → **Add Shift Register**. Afuera, conecta una constante `0` (I32) a la flechita ▼ de la izquierda.
- **Unbundle By Name** (Programming → Cluster → Unbundle By Name) sobre el `error out` de B5: elige `status` (verde: True si hubo error).
- **Case Structure**: la entrada **?** va al `status`.
  - Página **True**: un **Increment** (Programming → Numeric → Increment) entre la flechita ▼ y la ▲.
  - Página **False**: un cable directo de ▼ a ▲.
- Conecta lo que sale hacia la ▲ también al indicador `Líneas malas`.
- Para que la gráfica no reciba ceros en una línea mala, mete B6 en la página **False** del mismo Case (en la **True** no se grafica nada).

La primera línea casi siempre llega cortada: 1 línea mala al principio es normal.

**B8. Condición de paro.**
- **Unbundle By Name** sobre el `error out` de **B4** (el de VISA Read, no el de Scan From String) → `status`.
- **Or** (Programming → Boolean → Or): una entrada desde ese `status` y la otra desde el botón `STOP` (P6).
- La salida del Or va al circulito rojo 🔴 del bucle.

## PARTE 4. Plano I-Q (opcional)

1. Dos shift registers más. Afuera, a cada flechita ▼ conecta un arreglo vacío de DBL: clic derecho → Create → Constant sobre la entrada, o Programming → Array → **Array Constant** con un **Numeric Constant** DBL adentro.
2. En la página **False** del Case de B7: **Build Array** (Programming → Array → Build Array) con dos entradas: arriba la flechita ▼ (el arreglo), abajo el voltaje nuevo. Clic derecho en el bloque → **Concatenate Inputs** (si no, da error de tipos). Uno para I y otro para Q.
3. Para no guardar puntos sin fin: **Array Subset** (Programming → Array → Array Subset) después de cada Build Array, con `index` = largo − 1000 (usa **Array Size**, **Subtract** y **Max & Min** para que no sea negativo) y `length` = 1000. La salida va a la flechita ▲.
4. **Bundle** con dos entradas: arreglo de I (arriba = eje X) y arreglo de Q (abajo = eje Y). La salida va al **XY Graph** `Plano I-Q`.
5. En la página **True**, cable directo de ▼ a ▲ en los dos arreglos (si no, los túneles quedan huecos □).

## PARTE 5. Cerrar (fuera del bucle)

**B9. VISA Close** (Instrument I/O → Serial → VISA Close), a la derecha del bucle. Entran el cable morado y el de error que salen del bucle.

**B10. Simple Error Handler** (Programming → Dialog & User Interface → Simple Error Handler) después de B9: si algo falló, muestra el mensaje.

## PARTE 6. Probar

1. En el panel frontal, elige el COM del CP2102 en `Puerto`.
2. Run (la flecha, Ctrl + R).
3. Deben moverse las dos curvas. `I (código)` y `Q (código)` cambian todo el tiempo y `Líneas malas` queda en 0 o 1.
4. STOP para parar (no uses el botón rojo de abortar: deja el COM abierto).

| Lo que pasa | Qué revisar |
|---|---|
| Error de timeout (−1073807339) | no llega nada: COM equivocado, PIC sin programar, JP_TX abierto |
| Error al abrir el puerto | otro programa (Arduino IDE, Python) tiene el COM abierto |
| `Líneas malas` sube sin parar | `termination char` no es 10, `Enable Termination Char` en False o baud distinto de 115200 |
| Las curvas van lentas o se atrasan | normal en PCs lentas a 500 líneas/s; para mirar, sube `ADC_PARES_POR_LINEA` en `adc_ascii.h`. Para medir SINAD usa `interfaz_micro3.py` |
