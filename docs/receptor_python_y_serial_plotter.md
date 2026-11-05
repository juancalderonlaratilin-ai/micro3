# Receptor en Python y cómo ver los datos en un Serial Plotter

El PIC manda 500 líneas por segundo así: `2048,2051\r\n` (formato: `docs/especificacion_protocolo.md`). Hay tres formas de verlas en el PC.

## 1. El programa `receptor_micro3.py`

1. Abre el puerto a 115200 8N1 (no manda nada al PIC: el PIC envía solo).
2. Junta los bytes y, en cada línea completa, saca I y Q.
3. Cuenta las líneas que llegan cortadas o con otra forma («líneas malas»).
4. Grafica I y Q en voltios en vivo.
5. Al parar, cierra el puerto y guarda un CSV.

Por defecto **no se detiene solo**: para cuando cierras la ventana de la gráfica (o con Ctrl + C si corres sin gráfica). Para el corte automático usa `--muestras 10000`.

Para las mediciones (SINAD, ENOB, ruido, histograma) usa `interfaz_micro3.py` (`docs/interfaz_mediciones.md`).

### 1.1 Instalar (una sola vez)

En una terminal (cmd o PowerShell):

```
pip install pyserial matplotlib
```

### 1.2 Usarlo

Ve a la carpeta `herramientas` del proyecto y corre:

| Quieres | Comando |
|---|---|
| Probar que el programa funciona, **sin PIC** | `python receptor_micro3.py --simular` |
| Recibir del PIC con gráfica | `python receptor_micro3.py COM5` |
| Que pare solo a las 10 000 muestras | `python receptor_micro3.py COM5 --muestras 10000` |
| Cambiar el nombre del CSV | `python receptor_micro3.py COM5 --csv prueba1.csv` |
| Solo consola, sin gráfica | `python receptor_micro3.py COM5 --sin-grafica` |
| Ver más o menos puntos en la gráfica | `python receptor_micro3.py COM5 --ventana 2000` |

Cambia `COM5` por el COM de tu CP2102 (Administrador de dispositivos → Puertos COM y LPT). **Cierra LabVIEW, el Arduino IDE o cualquier programa que tenga abierto ese COM**: solo uno a la vez puede usarlo.

### 1.3 Qué vas a ver

- En la consola: `Llegan líneas: el PIC está enviando.` Si en cambio sale `AVISO: no llega ninguna línea`, revisa:
  - que el PIC esté programado y alimentado;
  - que JP_TX esté cerrado;
  - que el COM sea el correcto.
- Una ventana con dos curvas (I y Q en voltios) que se mueven. Arriba dice cuántas muestras van y las líneas malas.
- Al cerrar la ventana, un resumen así:

```
--- Resumen ---
Muestras recibidas : 10000
Duración           : 20.0 s  (500 muestras/s)
Líneas malas       : 0
I: promedio 2047.6  mín 2041  máx 2054
Q: promedio 2050.9  mín 2044  máx 2057
CSV guardado en    : captura_micro3.csv
```

- El CSV tiene las columnas `n, t_s, i, q, i_V, q_V` (`t_s` = tiempo en segundos, 2 ms por muestra) y se abre en Excel.
- La primera línea suele llegar cortada al abrir el puerto: 1 línea mala al principio es normal. Con el PIC funcionando no deben aparecer más.

## 2. Ver los datos en un Serial Plotter

Como el PIC ya manda texto con dos números separados por coma, el Serial Plotter del Arduino IDE lo dibuja **directo, sin Python**:

1. Arduino IDE → **Herramientas → Puerto** → el COM del CP2102.
2. **Herramientas → Serial Plotter** y, abajo, la velocidad en **115200**.
3. Verás dos curvas en códigos (de 0 a 4095): la 1 (`value 1`) es **I** y la 2 (`value 2`) es **Q**, porque la línea no trae nombres.

A 500 líneas por segundo la gráfica corre rápido (el Serial Plotter muestra solo los últimos puntos). Para mirar una señal lenta con calma, en `adc_ascii.h` sube `ADC_PARES_POR_LINEA` (por ejemplo a 50 = 20 líneas/s) y vuelve a programar; para medir SINAD déjalo en 2.

**SerialPlot** (programa aparte) también sirve, sin filtros, porque la línea ya son solo dos números (canal 1 = I, canal 2 = Q):

| Opción | Valor |
|---|---|
| Port | el COM del CP2102 |
| Baud Rate | `115200` |
| Data Format | **ASCII** |
| Number of channels | `2` |
| Delimiter | **comma** |

## 3. Si algo falla

| Lo que pasa | Qué revisar |
|---|---|
| `could not open port 'COM5'` | otro programa tiene el COM abierto, o el número es otro |
| `ModuleNotFoundError: No module named 'serial'` | falta `pip install pyserial` |
| `No module named 'matplotlib'` | falta `pip install matplotlib`, o usa `--sin-grafica` |
| No llega ninguna línea | PIC sin programar o sin alimentar, JP_TX abierto, J5 al revés, COM equivocado |
| Llega basura en vez de texto | baud distinto de 115200, o la GND del CP2102 sin unir a la de la placa |
| Muchas líneas malas | cable largo o suelto, tierra sin conectar; en el PIC, revisa `adcLineasPerdidas` en MPLAB |
| Muestras/s muy por debajo de 500 | `ADC_PARES_POR_LINEA` distinto de 2, o la PC se atrasa: cierra otros programas |
