# Interfaz de mediciones del ADC (Python)

`herramientas/interfaz_micro3.py` es una ventana que recibe las líneas de texto `2048,2051` del PIC (500 por segundo, `docs/especificacion_protocolo.md`), muestra I y Q en vivo y calcula las mediciones del ADC: **SINAD, ENOB, SNR, THD, SFDR**, ruido rms, histograma de códigos y plano I-Q.

## 1. Instalar y abrir

Una sola vez:
```
pip install pyserial numpy matplotlib
```
Abrir (desde la carpeta `herramientas`):
```
python interfaz_micro3.py
```
El PIC manda solo al encenderse: no hay que configurar nada en el firmware ni mandarle comandos. Cierra LabVIEW, el Arduino IDE o cualquier otro programa que tenga abierto el COM.

La frecuencia de muestreo (fs) se calcula sola con las líneas que llegan y se usa en el eje de tiempo, el espectro y el SINAD. Sale en el estado: `(498 /s → fs = 500 Hz)`. Si se cambia `ADC_PARES_POR_LINEA` en `adc_ascii.h`, la interfaz se ajusta sola (fs = 1000 / ese número).

## 2. La ventana

| Zona | Qué hace |
|---|---|
| **1. Conexión** | elige el COM del CP2102 y presiona **Conectar**. Para probar sin PIC hay dos simuladores: senoidal de 20 Hz y continua de 1,85 V (los dos a 500 líneas/s) |
| **2. Estado** | muestras recibidas, muestras/s (debe dar ≈ 500), líneas malas (debe quedar en 0 o 1, la primera suele llegar cortada) y el último I/Q con su mínimo y máximo |
| **3. Medición** | canal (I o Q), tipo de medición, cuántas muestras analizar y **Analizar ahora** (o «Analizar cada 1 s») |
| **4. Guardar** | CSV con todas las muestras (con su tiempo); reporte en `.txt` con las 4 gráficas en PNG |
| **Resultados** | los números de la última medición; el SINAD sale en verde si cumple ≥ 60 dB y en rojo si no |
| **Pestañas** | *Señal en vivo* (últimos 2 s), *Espectro* (dBFS, con la fundamental y los armónicos marcados), *Histograma* (un bin por código), *Plano I-Q* |

### 2.1 Si la gráfica sale vacía o rara

La zona **2. Estado** muestra el último código de I y Q (y en voltios) y el mínimo/máximo del último segundo. Con eso se sabe qué pasa:

| Lo que ves | Qué significa | Qué hacer |
|---|---|---|
| «No llega nada» | el PC no recibe líneas | revisa el COM, JP_TX, que el PIC esté programado y la GND común |
| I y Q en **0** (o casi) | no llega señal al pin | revisa la entrada J2/J7 y los jumpers JP_IN_I/Q y JP_ADC_I/Q |
| I o Q en **4095** (aviso «pegado a 4095») | el pin está en VDD (3,7 V) o la señal recorta | mide el pin (RC0 = pata 10, RC1 = pata 9) con el multímetro; si da 3,7 V es el circuito, no el código. Revisa jumpers, que la GND del generador esté unida a la de la placa y baja la amplitud u offset |
| códigos **> 4095** | el ADC está justificado a la izquierda | revisa ADFM = 1 en MCC |
| aviso «Solo 20 líneas/s» | `ADC_PARES_POR_LINEA` está en 50 | ponlo en 2 en `adc_ascii.h` para medir SINAD |
| ≈ 500 /s y códigos entre 100 y 4000 | todo bien | — |

Si los datos se salen de 0–3,7 V, el eje vertical se ajusta solo para que siempre se vean.

### 2.2 Programar el firmware

1. MPLAB X → **File → Open Project** → `Documents\micro3\firmware\Micro_Firmware.X` y márcalo como *Main Project* (clic derecho → Set as Main Project).
2. **Clean and Build** y luego **Make and Program Device** con la SNAP.
3. Abre la interfaz: debe decir ≈ 500 /s.

## 3. Prueba de SINAD (prueba 4.15)

1. **Placa:** generador de funciones en **J7** (TEST_I, y luego TEST_Q), **JP_IN_I/Q en 2–3** (entrada de prueba) y **JP_ADC_I/Q en 2–3** (sin el filtro de 5 Hz de la placa: en esta entrega no se usa filtro anti-aliasing). Así la señal llega directo al pin por \~1,1 kΩ.
2. **Generador:** senoidal de **1,7 V de amplitud centrada en 1,85 V** (de 0,15 V a 3,55 V, sin recorte), a **5 Hz** y luego a **20 Hz**.
3. **En la interfaz:**
   1. Canal I, tipo «Senoidal», 4096 muestras (o 8192 o 10 000).
   2. Deja «f entrada» vacío: la frecuencia se detecta sola.
   3. Espera unos 10 s (4096 muestras a 500 por segundo tardan 8,2 s) y presiona **Analizar ahora**.
4. **Criterio:** SINAD ≥ 60 dB (ENOB ≥ 9,67 bits) a 5 Hz y a 20 Hz, en I y en Q.
5. **Guardar reporte** para el preinforme: guarda los números y las gráficas.

Qué significa cada número:

| Medición | Qué es |
|---|---|
| **SINAD** | señal contra todo lo demás (ruido + distorsión), en dB. Ideal de 12 bits: 74 dB |
| **ENOB** | bits efectivos = (SINAD − 1,76) / 6,02 |
| **SNR** | señal contra ruido, sin contar los armónicos |
| **THD** | potencia de los armónicos 2 a 5 contra la fundamental |
| **SFDR** | distancia entre la fundamental y el mayor espurio |
| **SINAD_FS / ENOB_FS** | el mismo SINAD llevado a una senoidal de escala completa (1,85 V pico). Sirve si la amplitud fue menor |

Avisos que puede mostrar:
- **Recorte:** la señal toca 0 o 4095. Baja la amplitud del generador.
- **Muy pocos ciclos:** hacen falta al menos 14 ciclos en la captura (con 4096 muestras a 500 Hz, unos 2 Hz).
- **No se ve una senoidal:** probablemente la entrada es continua.

## 4. Prueba de ruido con entrada fija (pruebas 4.3 y 8.1 del preinforme)

1. Entrada fija: J7 TEST_I a GND (0 V) o el reposo de 1,85 V, con los mismos jumpers.
2. Tipo «Continua», 10 000 muestras, **Analizar ahora**.
3. Resultados: media, **σ** en LSB y en µV, pico a pico, códigos distintos y resoluciones. Criterio propuesto: σ ≤ 1,4 LSB (ideal 0,29 LSB = 261 µV).
4. La pestaña *Histograma* muestra los códigos con una gaussiana de la misma σ encima.

## 5. Modo periodo

Si algún día se implementa `MODO_PERIODO` en el firmware (el PIC manda en Q el periodo medido con Timer1, en cuentas de 62,5 ns), el tipo «Periodo» saca el histograma del periodo y su desviación estándar. Con el firmware actual ese modo no tiene sentido.

## 6. Nota sobre el método

Usa el mismo cálculo que `docs/especificacion_adc.md` §6 (ventana de Kaiser β = 14, fundamental ± 6 bins) con una corrección. El script original contaba como ruido los bins 1 a 6, donde la ventana reparte la continua que sobra. Por eso, a 5 Hz con 4096 muestras daba unos 36 dB aunque la señal fuera perfecta. La interfaz quita la continua ponderada por la ventana y excluye esos bins.

Comprobado con señales sintéticas (12 bits, 3,7 V, 1,7 V de amplitud):

| Ruido agregado | 5 Hz | 20 Hz | Esperado |
|---|---|---|---|
| ninguno | 72,5 dB | 73,3 dB | 73,3 dB |
| 1 LSB rms | 62,0 dB | 62,1 dB | ≈ 62 dB |
| 1,45 LSB rms | 59,2 dB | 58,8 dB | ≈ 59 dB |

Hay que corregir igual el script de `especificacion_adc.md` §6.
