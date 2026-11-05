# Especificación del PWM: reconstruir I y Q a partir de las muestras (T-06)

Proyecto Micro 3 · Fecha: 2026-11-04 · Revisado: 2026-11-05 · Estado: **implementada, sin probar** (`firmware/Micro_Firmware.X/pwm.c/.h`; compilada con XC8 2.40 el 2026-11-04, 0 errores)

> **Cambio de enfoque del profesor (2026-11-04).** En Micro 3 el PWM ya **no** es el auto-cero (I0/Q0 como niveles de continua): eso pasa a **Macro** y quedó guardado en `docs/macro_autocero_pwm.md`, `docs/criterios_filtro_pwm.md` y `docs/guia_filtro_pwm_dac.md`.
>
> **Ahora:** con las señales muestreadas se controla **un PWM por canal**, y con un **filtro pasa bajos** en cada salida se vuelve a ver la señal analógica. Es un convertidor digital-analógico hecho con PWM: ADC → número → PWM → filtro → señal.
>
> **Etiquetas:** **[VERIFICADO]** = datasheet (DS40001985C) o documento de `docs/`. **[CÁLCULO]** = cuenta propia, por comprobar en la placa. **[SIMULACIÓN]** = simulación en Python del PWM real y del filtro ideal. **[PROPUESTA]** = decisión por aprobar.

---

## 1. Qué hace

```
           ADC (1 kHz)            PWM 15,625 kHz          filtro pasa bajos
I (RC0) ──► código 0..4095 ──► duty 0..1023 en RA2 ──► R-C / Sallen-Key ──► I reconstruida
Q (RC1) ──► código 0..4095 ──► duty 0..1023 en RA4 ──► R-C / Sallen-Key ──► Q reconstruida
```

1. El Timer0 dispara el ADC cada 1 ms; la ISR deja el par (I, Q) listo (T-09, sin cambios).
2. `main` lo lee con `adc_iq_get` y llama a `pwm_desde_adc(i, q)`: el duty de cada PWM pasa a ser el código del ADC dividido entre 4 (redondeado).
3. El hardware aplica el duty nuevo al empezar el siguiente periodo de 64 µs. Sin interrupciones.
4. El filtro pasa bajos se queda con el promedio del PWM: `V = VDD · duty / 1024`.
5. El mismo par se sigue mandando al PC como texto (`adc_ascii_par`, 500 líneas/s).

**Las señales que usa el PWM son las muestreadas:** el duty sale del mismo par (I, Q) que entrega `adc_iq_get`, una vez por milisegundo (1 kHz por canal). El PWM usa todos los pares; la UART, 1 de cada 2.

**El filtro de este documento es el de reconstrucción** (después del PWM). Antes del ADC **no hay filtro anti-aliasing** en esta entrega (indicación de la persona, 2026-11-05).

**Ganancia 1:** el ADC mide con referencia VDD y el PWM sale entre 0 y VDD, así que la salida filtrada repite la tensión de entrada: código 2048 → duty 512 → 1,850 V [CÁLCULO]. No hace falta calibrar la escala.

## 2. Números [CÁLCULO]

| Magnitud | Valor |
|---|---|
| Resolución del PWM | 10 bits = 1024 pasos (`T2PR` = 255) |
| Paso de salida | 3,7 V / 1024 = **3,61 mV** (4 códigos del ADC) |
| Conversión código → duty | `duty = (código + 2) >> 2`, recortado a 1023. Error ≤ medio paso (1,8 mV) |
| Rango de salida | 0 V (duty 0) a 3,696 V (duty 1023) |
| Frecuencia del PWM | 15,625 kHz (periodo 64 µs) |
| Actualización | 1000 veces por segundo (cada par del ADC); el duty cambia en el siguiente borde de 64 µs |
| Retardo muestra → PWM | ≈ 0,1 ms (fin de la ráfaga I+Q) + hasta 64 µs (próximo periodo) |
| SINAD ideal de un DAC de 10 bits | 6,02 · 10 + 1,76 = 62 dB a escala completa |

## 3. Recursos del chip [VERIFICADO salvo lo marcado]

| Recurso | Valor | Fuente |
|---|---|---|
| Módulos | **PWM6** (canal I) y **PWM7** (canal Q), 10 bits | DS cap. 24 |
| Base de tiempo | **Timer2** para los dos: tras el reset `CCPTMRS1` tiene `P6TSEL = 01` y `P7TSEL = 01` (Timer2), así que **no hay que escribirlo** | DS §23.2.2, p.346 |
| Periodo | `(T2PR + 1) · 4 · Tosc · prescaler`. Con Fosc = 16 MHz, `T2PR = 255`, prescaler 1:1: 256 · 4 · 62,5 ns = **64 µs = 15,625 kHz** | DS Ec. 24-1 |
| Reloj de Timer2 | debe ser **Fosc/4**: `T2CLKCON.CS = 0001` (el valor de reset es el pin externo, no cuenta) | DS §24.3; `notas` §9.3 |
| Duty de 10 bits | `PWMxDCH` = 8 bits altos, `PWMxDCL[7:6]` = 2 bits bajos: `DCH = d >> 2`, `DCL = (d & 3) << 6` | DS §24.4 |
| Doble buffer del duty | se copian al hardware cuando `T2TMR = T2PR`. Si la copia cae entre escribir DCH y DCL, **un solo periodo** sale con un valor mezclado (error ≤ 3 pasos durante 64 µs); el filtro lo borra | DS §24.1; [CÁLCULO] |
| Habilitar | `PWMxCON.EN = 1` (bit 7). `POL = 0` (salida normal) | DS §24.12.2 |
| Pin | bit `TRIS` en 0 **y** `RxyPPS`: `RA2PPS = 0x0D` (PWM6OUT), `RA4PPS = 0x0E` (PWM7OUT) | DS Tabla 16-3 |
| Interrupciones | **ninguna**: el PWM corre solo en hardware | DS cap. 24 |
| `PPS1WAY` | ya está en OFF (C-01) | `estado.md` |
| RA4 | `FEXTOSC = 100` (sin oscilador externo): OSC2 no se usa y RA4 queda libre | `notas` §4 |

Los nombres en C (`PWM6CON`, `PWM6DCH`, `PWM6DCL`, `PWM7…`, `T2CON`, `T2CLKCON`, `T2HLT`, `T2PR`, `T2TMR`, `RA2PPS`, `RA4PPS`, `ANSELAbits.ANSA2`, `TRISAbits.TRISA2`, `LATAbits.LATA2`) siguen el patrón de MCC para esta familia y **están confirmados** en el `pic16f18426.h` de XC8 2.40 (`pwm.c` compila sin errores; `notas` §9.1 y §9.3). Los códigos PPS `0x0D`/`0x0E` y el reset de `CCPTMRS1` vienen del datasheet y falta probarlos en el chip.

**Timers ocupados:** Timer0 (disparo del ADC) y **Timer2 (PWM)**. Timer4 y Timer6 siguen libres.

## 4. Pines

| Pin físico | Puerto | Función | Dirección | Notas |
|---|---|---|---|---|
| 11 | **RA2** | PWM6 → I reconstruida | salida | `TRISA2 = 0`, `ANSELA2 = 0`, `RA2PPS = 0x0D`. En la placa: `PWM_I` |
| 3 | **RA4** | PWM7 → Q reconstruida | salida | `TRISA4 = 0`, `ANSELA4 = 0`, `RA4PPS = 0x0E`. En la placa: `PWM_Q` |
| 10 / 9 | RC0 / RC1 | entradas I y Q (ADC) | entrada | sin cambios |
| 6 / 5 | RC4 / RC5 | UART TX / RX | – | sin cambios |
| 8 | RC2 | salida del Timer0 (`ADC_PIN_PRUEBA` = 2) | salida | sin cambios |

`SYSTEM_Initialize()` deja `TRISA = 0x3F` y `ANSELA = 0x37` (todo entrada); `pwm_init()` baja los bits 2 y 4 de los dos. **No se cambia ningún pin ya decidido.** Durante el reset los pines son entradas: el filtro queda flotando hasta que corre `pwm_init()`.

## 5. Código (T-06)

### 5.1 Archivos

| Archivo | Qué se hace |
|---|---|
| `pwm.h`, `pwm.c` | **nuevos** (§5.2 a §5.4) |
| `main.c` | incluir `pwm.h`, llamar a `pwm_init()` y repartir cada par (§5.5) |
| `adc_ascii.c/.h` | `adc_ascii_task()` pasa a `adc_ascii_par(uint16_t i, uint16_t q)`: ya no llama a `adc_iq_get`, recibe el par desde `main`, porque ahora dos módulos usan el mismo par (`docs/especificacion_protocolo.md` §4) |
| `nbproject/configurations.xml`, `nbproject/Makefile-default.mk` | registrar `pwm.c` y `pwm.h` igual que `adc_ascii.c/.h` (lista de fuentes, objetos, dependencias y las dos reglas de compilación) |
| **No se tocan** | `eusart1.*`, `interrupt_manager.*`, `adc_iq.*`, `adcc.*`, `tmr0.*`, `pin_manager.*`, `mcc.*`, `device_config.*`. El PWM no agrega ninguna fuente a la ISR |

Formato de los archivos del firmware: **ISO-8859-1 (Latin-1) con CRLF**, como el resto. Comentarios en español, línea por línea (`docs/instrucciones_claude_code_uart.md` §2), con la etiqueta `[T-06]`.

### 5.2 `pwm.h`

```c
#define PWM_CANAL_I      0      // PWM6, RA2
#define PWM_CANAL_Q      1      // PWM7, RA4
#define PWM_DUTY_MAX     1023u  // 4 * (T2PR + 1) - 1
#define PWM_DUTY_INICIAL 512u   // 50 % = 1,85 V

void     pwm_init(void);
bool     pwm_set(uint8_t canal, uint16_t duty);   // false si canal > 1 o duty > 1023 (no toca nada)
uint16_t pwm_get(uint8_t canal);                  // copia en RAM; 0xFFFF si el canal no existe
void     pwm_desde_adc(uint16_t i, uint16_t q);   // duty = min((código + 2) >> 2, 1023)
```

### 5.3 `pwm_init()`: escrituras, en este orden

| # | Escritura | Por qué |
|---|---|---|
| 1 | `PWM6CON = 0x00; PWM7CON = 0x00; T2CON = 0x00;` | apagar mientras se configura |
| 2 | `LATAbits.LATA2 = 0; LATAbits.LATA4 = 0;` | el pin sale en 0 al volverlo salida |
| 3 | `ANSELAbits.ANSA2 = 0; ANSELAbits.ANSA4 = 0;` | digital (al reset son analógicos) |
| 4 | `TRISAbits.TRISA2 = 0; TRISAbits.TRISA4 = 0;` | salida (el PWM necesita el TRIS en 0) |
| 5 | `RA2PPS = 0x0D; RA4PPS = 0x0E;` | PWM6OUT y PWM7OUT al pin (DS p.233, `notas` §5) |
| 6 | `T2CLKCON = 0x01;` | `CS` = 0001 = Fosc/4 (el reset, 0000, es el pin externo y no contaría) |
| 7 | `T2HLT = 0x00;` | modo libre (`MODE` = 00000), es el valor de reset |
| 8 | `T2PR = 255; T2TMR = 0;` | periodo de 256 cuentas = 64 µs, 10 bits |
| 9 | `pwm_set(PWM_CANAL_I, 512); pwm_set(PWM_CANAL_Q, 512);` | duty inicial antes de encender |
| 10 | `T2CON = 0x80;` | `ON` = 1, `CKPS` = 000 (1:1), `OUTPS` = 0000 (1:1) |
| 11 | `PWM6CON = 0x80; PWM7CON = 0x80;` | `EN` = 1, `POL` = 0 (en alto durante el duty) |

`CCPTMRS1` **no** se escribe (tras el reset ya asigna PWM6 y PWM7 al Timer2). Sin interrupciones del Timer2 (`TMR2IE` queda en 0).

### 5.4 `pwm_set()` y `pwm_desde_adc()`

- `pwm_set`: revisa primero (`canal > PWM_CANAL_Q || duty > PWM_DUTY_MAX` → `return false`); luego `PWMxDCH = (uint8_t)(duty >> 2)` y `PWMxDCL = (uint8_t)((duty & 3) << 6)`; guarda el valor en `static uint16_t pwmDuty[2]` y devuelve `true`. No apaga interrupciones ni espera (ver el doble buffer en §3).
- `pwm_desde_adc`: `a_duty(código) = (código + 2) >> 2`, recortado a 1023 (4095 daría 1024). Ejemplos: 0 → 0, 2048 → 512, 4093 → 1023, 4095 → 1023.

### 5.5 `main.c`

```c
uint16_t i, q;
SYSTEM_Initialize();
adc_iq_init();                     // T-09
pwm_init();                        // T-06: antes de encender las interrupciones
INTERRUPT_PeripheralInterruptEnable();
INTERRUPT_GlobalInterruptEnable();
tx_string("Micro3 listo\r\n");
while (1) {
    if (adc_iq_get(&i, &q)) {
        pwm_desde_adc(i, q);       // primero el PWM: la salida cambia unos µs después de la muestra
        adc_ascii_par(i, q);       // después el texto (500 líneas/s)
    }
}
```

- Sin `while` que espere ni `__delay_ms`.
- Los nombres en C de §3 y §5.3 se confirman en el `pic16f18426.h` de XC8 2.40 antes de usarlos, y se anotan en `docs/notas_pic16f18426.md` §9.1.

## 6. Filtro pasa bajos de reconstrucción [PROPUESTA]

### 6.1 Qué se le pide

| Requisito | Por qué |
|---|---|
| Dejar pasar la señal (hasta ≈ 20 Hz) sin atenuarla | se quiere ver la misma señal que entró al ADC |
| Quitar el PWM de 15,625 kHz | es una cuadrada de 0 a 3,7 V |
| Atenuar los escalones de 1 ms (imágenes en 1 kHz ± f) | el duty cambia una vez por milisegundo |

Una frecuencia de corte de **≈ 100 Hz** cumple las tres: queda 5 veces por encima de 20 Hz y 150 veces por debajo de 15,6 kHz.

### 6.2 Opciones [CÁLCULO]

| Filtro | fc (−3 dB) | a 5 Hz | a 20 Hz | a 1 kHz | a 15,625 kHz | Rizado del PWM (peor caso, duty 50 %) |
|---|---|---|---|---|---|---|
| **A. Sallen-Key de ganancia 1** (recomendado): R1 = R2 = **22 kΩ**, C (realimentación) = **100 nF**, C (a tierra) = **47 nF**, un op-amp rail-to-rail (por ejemplo, TLV9062/TLV9064 o el MCP604 de la placa) | 109 Hz (f₀ = 105 Hz, Q = 0,73) | 0,00 dB | −0,01 dB, −15° | −39 dB | −87 dB | ≈ 0,21 mV pp (0,24 códigos) |
| **B. RC pasivo de 2 etapas** (para la protoboard, sin op-amp): 10 kΩ + 100 nF, dos veces | 60 Hz | −0,03 dB | −0,46 dB, −21° | −33 dB | −80 dB | ≈ 0,49 mV pp; salida de alta impedancia: medir con punta ×10 |
| C. La escalera de la placa Micro3 (R50–R52 10 kΩ, C 1 µF, mezcla con `VREF_BUF`), en `TP_PWM_I` / `TP_PWM_Q` | polos en 11 y 44 Hz | −0,8 dB | **−6,9 dB** | −66 dB | −114 dB | despreciable |

**Sobre la opción C (placa):** esa red se diseñó para el auto-cero (ahora Macro). Su salida es `0,25 · Vpwm + 0,75 · Vref` (la señal sale **reducida a la cuarta parte** alrededor de 1,85 V) y es lenta: sirve para mirar señales de unos pocos hertz, no para 20 Hz. Si se usa la placa: medir en `TP_PWM_I`/`TP_PWM_Q`, dejar **JP_AZEN en 1–2** y **no** conectar J3 (I0_CMD/Q0_CMD) a la placa de la Práctica 2, o la señal reconstruida volvería a entrar como cero y se cerraría un lazo.

### 6.3 Qué se espera de la salida [SIMULACIÓN]

Simulación del PWM real (16 MHz, 10 bits, duty actualizado cada 1 ms en el siguiente periodo de 64 µs) con una senoidal de 1,7 V de amplitud centrada en 1,85 V, cuantizada con 12 bits, y el filtro de 2.º orden ideal:

| fc | SINAD a 20 Hz | Amplitud a 20 Hz |
|---|---|---|
| 50 Hz | 55,9 dB | 1,678 V |
| **100 Hz** | **54,3 dB** | **1,698 V** |
| 200 Hz | 52,2 dB | 1,699 V |

- A 5 Hz con fc = 100 Hz: ≈ 64 dB.
- Lo que más limita a 20 Hz es un **2.º armónico** propio del PWM de flanco final: el centro del pulso se corre con el duty (hasta 32 µs), y eso mueve un poco la fase al ritmo de la señal. Un DAC ideal de 10 bits con el mismo filtro daría ≈ 64 dB.
- Para mejorar (no implementado): un PWM centrado, o más resolución (menos `T2PR` sube la frecuencia pero baja los bits).
- El SINAD de la salida reconstruida **no es** el requisito de > 60 dB, que es del ADC (prueba 4.15).

## 7. Pruebas

Módulo 3 de `docs/protocolos_prueba.md` (3.1 a 3.8): frecuencia del PWM, duty que sigue a la entrada, salida filtrada igual a la entrada en continua, linealidad, rizado, seguimiento de una senoidal, retardo y que el envío ASCII sigue funcionando.

## 8. Lo que no se verificó

- Compilado con XC8 2.40 (2026-11-04, nombres comprobados en el `.h`), pero nada se ha medido en el chip.
- El filtro A/B no está montado; los valores son cálculos con componentes ideales.
- El SINAD de la salida es una simulación con un filtro ideal; el op-amp real, las tolerancias y el ruido de VDD lo bajan.
- Si el profesor quiere una frecuencia de corte o una ganancia distintas para la salida reconstruida.
