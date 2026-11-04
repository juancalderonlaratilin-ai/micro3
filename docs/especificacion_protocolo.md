# Formato de envío: líneas de texto ASCII (T-09)

Proyecto Micro 3 · Decidido por la persona el 2026-11-04 (anotado en `docs/criterios_diseno.md` §10) · Código: `firmware/Micro_Firmware.X/adc_ascii.c/.h`

> **Resumen:** el PIC manda I y Q al PC como texto, una línea por par, **500 líneas por segundo**, sin que el PC le pida nada:
> ```
> I:2048,Q:2051\r\n
> ```
> Es el único formato del proyecto. El PIC no recibe comandos.

## 1. La línea

| Parte | Bytes | Qué es |
|---|---|---|
| `I:` | 2 | nombre del canal I |
| valor de I | 1 a 4 | código del ADC en decimal, de `0` a `4095`, sin ceros a la izquierda |
| `,Q:` | 3 | separador y nombre del canal Q |
| valor de Q | 1 a 4 | igual que I |
| `\r\n` | 2 | fin de línea (`0x0D 0x0A`) |

- Largo: **15 bytes** con 4 cifras en los dos canales (`I:4095,Q:4095\r\n`); menos si un valor tiene menos cifras (el mínimo es `I:0,Q:0\r\n`). Con la señal cerca de la mitad de escala casi siempre son 15.
- `nombre:valor` separados por coma es el formato que entiende el **Serial Plotter del Arduino IDE**: dibuja dos curvas, I y Q, sin configurar nada.
- Volts = código × 3,7 V / 4096 (1 código = 0,903 mV).
- Al arrancar, el PIC manda una vez `Micro3 listo\r\n`. El PC la ignora porque no tiene la forma `I:…,Q:…`.

## 2. Velocidad y uso de la línea

| Dato | Valor | De dónde sale |
|---|---|---|
| UART | 115200 baudios nominales, 8N1 (10 bits por byte) | `docs/criterios_diseno.md` §3 y §4 |
| Baud real | 16 MHz / (4 · (34 + 1)) = **114 286** baudios | `SP1BRG` = 34, `BRG16` = 1, `BRGH` = 1 |
| Tiempo de una línea de 15 bytes | 150 bits / 114 286 = **1,31 ms** | |
| Muestreo del ADC | 1000 pares por segundo (Timer0 a 1 ms) | `docs/especificacion_adc.md` |
| Líneas por segundo | 1000 / `ADC_PARES_POR_LINEA` = 1000 / 2 = **500** (fs = 500 Hz en el PC) | `adc_ascii.h` |
| Uso de la línea | 15 bytes × 500 / 11 429 bytes/s = **65,6 %** | |

**Por qué 1 línea cada 2 pares y no cada par:** una línea tarda 1,31 ms y llega un par nuevo cada 1 ms. Con `ADC_PARES_POR_LINEA` = 1 no caben (131 % de la línea) y se perderían líneas. Con 2 sobra un 34 %.

**Por qué alcanza fs = 500 Hz:** el filtro anti-aliasing de la placa corta en ≈ 5 Hz y las pruebas de SINAD se hacen a 5 Hz y 20 Hz. Sus armónicos 2 a 5 llegan a 100 Hz, por debajo de 250 Hz (la mitad de 500). El ADC sigue muestreando a 1 kHz con el disparo del Timer0, así que el instante de cada muestra no tiene jitter de software.

## 3. Por qué texto (decisión del 2026-11-04)

| | Texto ASCII (lo escogido) |
|---|---|
| A favor | Se lee directo en cualquier terminal y en el Serial Plotter. En LabVIEW basta leer hasta `\n` y separar con *Scan From String* (`I:%d,Q:%d`). Es lo que sugiere el enunciado (§3.4 D). El PC no tiene que mandar nada para que empiece el envío |
| En contra | Ocupa más bytes por par, así que se manda 1 de cada 2 pares (fs = 500 Hz en vez de 1000). No hay suma de verificación: una línea dañada solo se detecta si no tiene la forma `I:número,Q:número`. No hay número de secuencia: una línea perdida no se ve. El PC no puede mandar comandos (el PWM por serial, T-07, queda por replantear) |

## 4. En el PIC

Archivo `adc_ascii.c`, función `adc_ascii_task()`, que `main` llama en cada vuelta del `while(1)`:

1. `adc_iq_get(&i, &q)`: si la ISR del ADC no dejó un par nuevo, regresa enseguida (no espera).
2. Cuenta los pares; solo cada `ADC_PARES_POR_LINEA` arma la línea.
3. `a_decimal()` escribe cada número en decimal restando 1000, 100, 10 y 1 (sin `printf` ni división).
4. `tx_string(linea)` mete los bytes en el buffer circular de TX (64 bytes) y regresa; la ISR de TX los manda.
5. Si no caben, la línea se descarta y se cuenta en `adcLineasPerdidas` (debe quedar en 0; se ve en el depurador).

Lo que llega por RX se queda en el buffer sin leer.

## 5. En el PC

| Programa | Cómo lee |
|---|---|
| `herramientas/interfaz_micro3.py` | ventana con I y Q en vivo, SINAD, ENOB, ruido, histograma y plano I-Q (`docs/interfaz_mediciones.md`) |
| `herramientas/receptor_micro3.py` | gráfica simple y CSV (`docs/receptor_python_y_serial_plotter.md`) |
| Serial Plotter del Arduino IDE | Herramientas → Puerto → el COM; Serial Plotter a 115200 |
| LabVIEW | *VISA Configure Serial Port* (115200, 8N1, carácter de fin `\n` habilitado) → *VISA Read* en un bucle → *Scan From String* con `I:%d,Q:%d` → I y Q al *XY Graph* |

Para separar los valores en cualquier lenguaje: la expresión regular `I:(\d{1,5}),Q:(\d{1,5})\r?\n`. Una línea que no la cumple se descarta y se cuenta como «línea mala».

## 6. Pruebas

Módulo 2 de `docs/protocolos_prueba.md` (2.1 a 2.6): forma de la línea, 500 líneas/s, cero líneas malas y cero `adcLineasPerdidas` en 10 minutos, arranque sin comando y valores correctos con una tensión conocida.

## 7. Lo que no se verificó

- Nada de esto se ha medido todavía en el chip con `ADC_PARES_POR_LINEA` = 2 (500 líneas/s). La persona confirmó el 2026-11-04 que el modo ASCII funciona.
- El 65,6 % supone siempre 15 bytes por línea; con valores de menos de 4 cifras es menos.
