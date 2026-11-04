# Receptor en Python y cómo ver los datos en un Serial Plotter

## 1. El programa `receptor_micro3.py`

Hace lo mismo que el VI de LabVIEW:
1. abre el puerto a 115200 8N1 y manda el comando de encendido (`A5 5A 20 00 01 00 00 00 2D`);
2. busca `A5 5A`, toma 9 bytes y revisa el CRC-8;
3. de cada trama de datos (TYPE `A1`) saca SEQ, I y Q;
4. cuenta los CRC malos y las tramas perdidas (saltos de SEQ);
5. grafica I y Q en voltios en vivo;
6. al parar manda el comando de apagado (`A5 5A 20 01 00 00 00 00 59`), cierra el puerto y guarda un CSV.

**Igual que tu VI sin el bloque H6:** por defecto **no se detiene solo**. Para cuando cierras la ventana de la gráfica (o con Ctrl + C si corres sin gráfica). Si algún día quieres el corte automático, usa `--muestras 10000`: es lo que harían H6 y H7 en LabVIEW.

### 1.1 Instalar (una sola vez)

En una terminal (cmd o PowerShell):

```
pip install pyserial matplotlib
```

`pyserial` ya lo tienes si corriste `prueba_protocolo.py`.

### 1.2 Usarlo

Ve a la carpeta `herramientas` del proyecto y corre:

| Quieres | Comando |
|---|---|
| Probar que el programa funciona, **sin PIC** | `python receptor_micro3.py --simular` |
| Recibir del PIC con gráfica | `python receptor_micro3.py COM5` |
| Que pare solo a las 10 000 muestras | `python receptor_micro3.py COM5 --muestras 10000` |
| Cambiar el nombre del CSV | `python receptor_micro3.py COM5 --csv prueba1.csv` |
| Solo consola, sin gráfica | `python receptor_micro3.py COM5 --sin-grafica` |
| Ver más o menos puntos en la gráfica | `python receptor_micro3.py COM5 --ventana 5000` |

Cambia `COM5` por el COM de tu CP2102 (Administrador de dispositivos → Puertos COM y LPT). **Cierra LabVIEW, el Arduino IDE o cualquier programa que tenga abierto ese COM**: solo uno a la vez puede usarlo.

### 1.3 Qué vas a ver

- En la consola: `ADC_ACK recibido: el PIC está enviando.` Si en cambio sale `AVISO: no llegó el ADC_ACK`, revisa:
  - que el firmware tenga `MODO_PRUEBA_ADC 0`;
  - que JP_TX y JP_RX estén cerrados;
  - que el COM sea el correcto.
- Una ventana con dos curvas (I y Q en voltios) que se mueven. Arriba dice cuántas muestras van, los CRC malos y las perdidas.
- Al cerrar la ventana, un resumen así:

```
--- Resumen ---
Muestras recibidas : 10000
Duración           : 10.0 s  (1000 muestras/s)
CRC malos          : 0
Perdidas (SEQ)     : 0
I: promedio 2047.6  mín 2041  máx 2054
Q: promedio 2050.9  mín 2044  máx 2057
CSV guardado en    : captura_micro3.csv
```

- El CSV tiene las columnas `seq, i, q, i_V, q_V` y se abre en Excel.

En `--simular` el programa inventa a propósito 1 trama dañada cada 500 y 1 perdida cada 1000: ahí **sí** deben salir CRC malos y perdidas. Con el PIC de verdad deben quedar en 0.

## 2. Ver los datos en un Serial Plotter

El Serial Plotter del Arduino IDE (y programas parecidos) **solo entiende texto**, como `I:2048,Q:2051`. Nuestra trama de 9 bytes es binaria, así que no se puede abrir directo. Hay tres formas:

### Opción A: la gráfica del programa de Python (la más fácil)

Ya es un «serial plot»: `python receptor_micro3.py COM5`. Usa la trama binaria con CRC, así que ves las 1000 muestras por segundo.

### Opción B: el PIC manda texto directo (sin Python)

El firmware ya trae el modo de prueba en texto:
1. En `main.c`: `#define MODO_PRUEBA_ADC 1`. Compila y programa.
2. Arduino IDE → **Herramientas → Puerto** → el COM del CP2102.
3. **Herramientas → Serial Plotter** y, abajo, la velocidad en **115200**.
4. Verás dos curvas, **I** y **Q**, en códigos (de 0 a 4095).

Límite: manda **1 línea cada 50 muestras** (20 líneas por segundo), así que solo dibuja bien señales de menos de unos 10 Hz. Para más detalle, en `adc_prueba.h` cambia `PRUEBA_PARES_POR_LINEA` de `50` a `10`: serían 100 líneas por segundo y 1500 bytes/s, el 13 % de la línea, que cabe bien. En este modo **no** funciona el protocolo, ni el VI de LabVIEW, ni el programa de Python.

### Opción C: Python decodifica y reenvía texto al Serial Plotter

Sirve para usar el protocolo binario (con CRC) y además ver la señal en el Serial Plotter de Arduino o en el programa **SerialPlot**. Hace falta un **par de puertos COM virtuales**: lo que se escribe en uno sale por el otro.

1. Instala **com0com** (emulador de cable null-modem, gratis y de código abierto) y crea un par, por ejemplo **COM10 ↔ COM11**. Con el «Setup» de com0com puedes cambiarles el nombre a esos números.
2. Corre Python leyendo el PIC y reenviando a COM10:
   ```
   python receptor_micro3.py COM5 --reenviar COM10 --cada 10
   ```
   (`--cada 10` = 1 línea de cada 10 muestras = 100 líneas por segundo.)
3. **Serial Plotter de Arduino:** **Herramientas → Puerto → COM11**, abrir el Serial Plotter a **115200**. Formato por defecto `I:2048,Q:2051`.
4. **SerialPlot** (programa aparte): agrega `--formato csv` al comando del paso 2, para que mande `2048,2051`. En SerialPlot:

   | Opción | Valor |
   |---|---|
   | Port | `COM11` |
   | Baud Rate | `115200` |
   | Data Format | **ASCII** |
   | Number of channels | `2` |
   | Delimiter | **comma** |

   Abre el puerto: canal 1 = I y canal 2 = Q.

Por qué SerialPlot no lee la trama binaria directo: su modo binario («Custom Frame») espera los datos justo después del encabezado, y en nuestra trama están el TYPE y el SEQ en medio. Además usa una suma simple en vez del CRC-8.

## 3. Si algo falla

| Lo que pasa | Qué revisar |
|---|---|
| `could not open port 'COM5'` | otro programa tiene el COM abierto, o el número es otro |
| `ModuleNotFoundError: No module named 'serial'` | falta `pip install pyserial` |
| `No module named 'matplotlib'` | falta `pip install matplotlib`, o usa `--sin-grafica` |
| No llega el ADC_ACK | `MODO_PRUEBA_ADC` en 1, jumpers JP_TX/JP_RX abiertos, J5 al revés |
| Muchos CRC malos con el PIC | baud distinto, cable largo o suelto, tierra (GND) sin conectar |
| Perdidas > 0 con el PIC | el PIC no alcanzó a encolar (revisa `adcTramasPerdidas` en MPLAB) o la PC se atrasó: cierra otros programas |
