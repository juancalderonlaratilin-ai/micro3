# Formato de envío: líneas de texto ASCII (T-09)

Proyecto Micro 3 · Decidido por la persona el 2026-11-04 (anotado en `docs/criterios_diseno.md` §10) · Código: `firmware/Micro_Firmware.X/adc_ascii.c/.h`

> **Resumen:** el PIC manda I y Q al PC como texto, una línea cada 2 pares, **500 líneas por segundo**, sin que el PC le pida nada:
> ```
> 2048,2051\r\n
> ```
> Primero va I y después Q, en decimal, separados por una coma. No llevan nombre: el orden dice cuál es cuál.
> Es el único formato del proyecto. El PIC no recibe comandos.

## 1. La línea

| Parte | Bytes | Qué es |
|---|---|---|
| valor de I | 1 a 4 | código del ADC en decimal, de `0` a `4095`, sin ceros a la izquierda |
| `,` | 1 | separa I de Q |
| valor de Q | 1 a 4 | igual que I |
| `\r\n` | 2 | fin de línea (`0x0D 0x0A`) |

- Largo: **11 bytes** con 4 cifras en los dos canales (`4095,4095\r\n`); menos si un valor tiene menos cifras (el mínimo es `0,0\r\n`). Con la señal cerca de la mitad de escala casi siempre son 11.
- Números separados por coma es un formato que entiende el **Serial Plotter del Arduino IDE**: dibuja dos curvas (la 1 es I y la 2 es Q), sin configurar nada.
- Volts = código × 3,7 V / 4096 (1 código = 0,903 mV).
- Al arrancar, el PIC manda una vez `Micro3 listo\r\n`. El PC la ignora porque no tiene la forma `número,número`.

## 2. Velocidad y uso de la línea

| Dato | Valor | De dónde sale |
|---|---|---|
| UART | 115200 baudios nominales, 8N1 (10 bits por byte) | `docs/criterios_diseno.md` §3 y §4 |
| Baud real | 16 MHz / (4 · (34 + 1)) = **114 286** baudios | `SP1BRG` = 34, `BRG16` = 1, `BRGH` = 1 |
| Tiempo de una línea de 11 bytes | 110 bits / 114 286 = **0,96 ms** | |
| Muestreo del ADC | 1000 pares por segundo (Timer0 a 1 ms) | `docs/especificacion_adc.md` |
| Líneas por segundo | 1000 / `ADC_PARES_POR_LINEA` = 1000 / 2 = **500** (fs = 500 Hz en el PC) | `adc_ascii.h` |
| Uso de la línea | 11 bytes × 500 / 11 429 bytes/s = **48,1 %** | |

**Por qué 1 línea cada 2 pares y no cada par:** una línea tarda 0,96 ms y llega un par nuevo cada 1 ms. Con `ADC_PARES_POR_LINEA` = 1 cabrían apenas (96 % de la línea): cualquier atraso o una línea de arranque llenaría el buffer de TX y se perderían líneas. Con 2 la línea va al 48 % y sobra la mitad. Si algún día hace falta fs = 1000 Hz, se puede probar con 1 revisando que `adcLineasPerdidas` quede en 0.

**Por qué alcanza fs = 500 Hz:** las pruebas de SINAD se hacen a 5 Hz y 20 Hz; sus armónicos 2 a 5 llegan a 100 Hz, por debajo de 250 Hz (la mitad de 500). El ADC sigue muestreando a 1 kHz con el disparo del Timer0 (I y Q en cada disparo) y el PWM usa todos los pares: solo la UART manda 1 de cada 2, sin promediar. El instante de cada muestra no tiene jitter de software.

**Sin filtro anti-aliasing (esta entrega):** no hay filtro antes del ADC. Una componente que llegue al pin por encima de 500 Hz se pliega ya en el ADC; una entre 250 y 500 Hz se ve bien en el PWM pero se pliega en los datos del PC, porque al descartar 1 de cada 2 pares no hay filtro digital.

## 3. Por qué texto (decisión del 2026-11-04)

| | Texto ASCII (lo escogido) |
|---|---|
| A favor | Se lee directo en cualquier terminal y en el Serial Plotter. En LabVIEW basta leer hasta `\n` y separar con *Scan From String* (`%d,%d`). Es lo que sugiere el enunciado (§3.4 D). El PC no tiene que mandar nada para que empiece el envío |
| En contra | Ocupa más bytes por par que un formato binario, así que se manda 1 de cada 2 pares (fs = 500 Hz en vez de 1000). Sin nombres, el PC depende del orden: el primer número es I y el segundo Q. No hay suma de verificación: una línea dañada solo se detecta si no tiene la forma `número,número`. No hay número de secuencia: una línea perdida no se ve. El PC no puede mandar comandos (el PWM por serial, T-07, pasa a Macro) |

## 4. En el PIC

`main` lee cada par y se lo pasa a dos módulos (T-09 y T-06):

```c
while (1) {
    if (adc_iq_get(&i, &q)) {      // true solo si la ISR del ADC dejó un par nuevo (cada 1 ms)
        pwm_desde_adc(i, q);       // T-06: PWM6/PWM7 repiten la señal (docs/especificacion_pwm.md)
        adc_ascii_par(i, q);       // T-09: cada 2 pares, una línea de texto
    }
}
```

Archivo `adc_ascii.c`, función `adc_ascii_par(uint16_t i, uint16_t q)`:

1. Cuenta los pares (contador `static uint8_t`); solo cada `ADC_PARES_POR_LINEA` arma la línea con el último par. `#error` si `ADC_PARES_POR_LINEA` no va de 1 a 255.
2. `a_decimal()` escribe cada número en decimal restando 1000, 100, 10 y 1 (sin `printf` ni división), sin ceros a la izquierda; el 0 se escribe `0`.
3. Arma en `char linea[12]`: I, `','`, Q, `'\r'`, `'\n'`, `'\0'`.
4. `tx_string(linea)` mete los bytes en el buffer circular de TX (64 bytes) y regresa; la ISR de TX los manda.
5. Si no caben, la línea se descarta y se cuenta en `volatile uint16_t adcLineasPerdidas` (debe quedar en 0; se ve en el depurador).

`adc_ascii.c` no llama a `adc_iq_get` ni toca registros: solo usa `tx_string` de `eusart1.h`. Al arrancar, `main` manda una vez `Micro3 listo\r\n`. Lo que llega por RX se queda en el buffer sin leer.

## 5. En el PC

| Programa | Cómo lee |
|---|---|
| `herramientas/interfaz_micro3.py` | ventana con I y Q en vivo, SINAD, ENOB, ruido, histograma y plano I-Q (`docs/interfaz_mediciones.md`) |
| `herramientas/receptor_micro3.py` | gráfica simple y CSV (`docs/receptor_python_y_serial_plotter.md`) |
| Serial Plotter del Arduino IDE | Herramientas → Puerto → el COM; Serial Plotter a 115200 |
| LabVIEW | *VISA Configure Serial Port* (115200, 8N1, carácter de fin `\n` habilitado) → *VISA Read* en un bucle → *Scan From String* con `%d,%d` → I y Q al *XY Graph* |

Para separar los valores en cualquier lenguaje: la expresión regular `^(\d{1,5}),(\d{1,5})\r?\n` (al inicio de la línea). Una línea que no la cumple se descarta y se cuenta como «línea mala».

## 6. Pruebas

Módulo 2 de `docs/protocolos_prueba.md` (2.1 a 2.6): forma de la línea, 500 líneas/s, cero líneas malas y cero `adcLineasPerdidas` en 10 minutos, arranque sin comando y valores correctos con una tensión conocida.

## 7. Lo que no se verificó

- Nada de esto se ha medido todavía en el chip con `ADC_PARES_POR_LINEA` = 2 (500 líneas/s). La persona confirmó el 2026-11-04 que el modo ASCII funciona con el formato anterior, con nombres (`I:2048,Q:2051`); el formato sin nombres (`2048,2051`) está sin probar.
- El 48,1 % supone siempre 11 bytes por línea; con valores de menos de 4 cifras es menos.
