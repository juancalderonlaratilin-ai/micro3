# Notas del PIC16F18426 (resumen verificado contra los documentos)

Proyecto Micro 3 · 2026-10-16 · Para consultar en lugar de releer los PDF completos.

**Documentos leídos** (en esta misma carpeta `docs/`):

| Sigla | Documento | Revisión |
|---|---|---|
| **DS** | PIC16(L)F18426/46 Data Sheet (`...XLP-40001985.pdf`) | DS40001985C, 698 págs. |
| **ER** | PIC16(L)F18426/46 Silicon Errata (`...80000799.pdf`) | DS80000799D |
| **UG** | MPLAB XC8 C Compiler User's Guide for PIC (`50002737C...pdf`) | 50002737C (2020) |
| **TB** | TB3261 PIC1000: Getting Started with Writing C-Code (`TB3261...pdf`) | DS90003261A |
| **AN** | AN688 Layout Tips for 12-Bit A/D Converter Application (`00688b.pdf`) | DS00688B (1999) |

**Convención:** cada dato lleva su fuente y página del PDF (por ejemplo, *DS p.411*). Lo que está marcado **⚠️** es una deducción mía a partir de los documentos, no una cita directa. Verifícalo antes de depender de él.

---

## 1. El chip en una mirada

| Característica | Valor | Fuente |
|---|---|---|
| Encapsulado de 14 pines | PDIP / SOIC / TSSOP (el PIC16F18426 también existe en 16 pines QFN/UQFN) | DS p.6 |
| Memoria | 28 KB de Flash (16 K palabras), **2048 B de RAM**, 256 B de EEPROM | DS p.5 (Tabla 1) |
| Pines de E/S | 12 (RA3 solo es entrada: es MCLR) | DS p.5 |
| ADC | 12 bits con cómputo (ADCC), hasta 140 ksps, 11 canales externos | DS p.3 y p.5 |
| PWM | **2 PWM de 10 bits (PWM6 y PWM7)** y 4 módulos CCP (PWM de 10 bits en modo PWM) | DS p.2 |
| Comunicación | **1 EUSART**, 2 MSSP (SPI/I²C) | DS p.3 |
| Temporizadores | Timer0, Timer1/3/5 (16 bits), Timer2/4/6 (8 bits, base de tiempo del PWM) | DS p.3 |
| Oscilador interno | HFINTOSC hasta 32 MHz, **±2 % a la calibración (nominal)** | DS p.4 |
| Depuración | integrada en el chip (el SNAP la usa por RA0/RA1) | DS p.5 |

---

## 2. Alimentación y niveles lógicos

| Dato | Valor | Fuente |
|---|---|---|
| VDD del **PIC16F18426** (sin "L") | **2,3 – 5,5 V** con Fosc ≤ 16 MHz; 2,5 – 5,5 V con Fosc > 16 MHz | DS p.616, parámetro D002 |
| VDD del PIC16**L**F18426 | 1,8 – 3,6 V (**3,7 V lo excede**) | DS p.616 |
| Alimentación de tu placa | 3,7 V (preinforme §1.2) | Preinforme |

**Antes de energizar, mira el marcado del chip:** si dice **LF**, 3,7 V está fuera de rango. Tu proyecto de MPLAB usa `PIC16F18426` (sin L), que sí lo admite.

**Umbrales de entrada con VDD = 3,7 V** (rango 1,8 V ≤ VDD ≤ 4,5 V; DS Tabla 39-4, ~p.622). Calculados por mí ⚠️:

| Tipo de entrada | VIL máx. | VIH mín. |
|---|---|---|
| Schmitt Trigger (por defecto) | 0,2 × VDD = **0,74 V** | 0,8 × VDD = **2,96 V** |
| TTL | 0,15 × VDD = **0,56 V** | 0,25 × VDD + 0,8 = **1,73 V** |

Un conversor USB-serie a 3,3 V entrega un "1" de ~3,3 V (o algo menos), muy cerca de los 2,96 V del Schmitt. **Con TTL el margen es amplio.** Cada pin tiene su selector de nivel: `INLVLx` (reset = 1 = Schmitt; 0 = TTL) (DS p.217).

---

## 3. Pines (14 pines)

Orden físico (DS p.6, Figura 1): `1 VDD · 2 RA5 · 3 RA4 · 4 RA3/MCLR · 5 RC5 · 6 RC4 · 7 RC3 · 8 RC2 · 9 RC1 · 10 RC0 · 11 RA2 · 12 RA1/ICSPCLK · 13 RA0/ICSPDAT · 14 VSS`

Funciones relevantes (DS p.8, Tabla 3):

| Pin | Puerto | Analógico | Funciones útiles para el proyecto |
|---|---|---|---|
| 2 | RA5 | ANA5 | Entrada digital libre, CLKIN/OSC1 |
| 3 | RA4 | ANA4 | CLKOUT/OSC2 |
| 4 | RA3 | — | **MCLR/VPP** (solo entrada) |
| 5 | RC5 | **ANC5** | **RX1/DT1 por defecto** (entrada PPS) |
| 6 | RC4 | **ANC4** | **CK1 por defecto**; sirve de salida TX1 vía PPS |
| 7 | RC3 | ANC3 | |
| 8 | RC2 | ANC2 | |
| 9 | RC1 | ANC1 | |
| 10 | RC0 | ANC0 | |
| 11 | RA2 | ANA2 | ADCVREF−, INT |
| 12 | RA1 | ANA1 | **ICSPCLK** (depurador), ADCVREF+ |
| 13 | RA0 | ANA0 | **ICSPDAT** (depurador) |

**Canales ADC disponibles (11):** ANA0, ANA1, ANA2, ANA4, ANA5, ANC0–ANC5.

**`ANSELx` (DS §14.4.6, p.191) [VERIFICADO en T-05]:** al reset todos sus bits valen 1 (pin analógico). Con el bit en 1 las lecturas digitales del pin dan siempre 0, pero **no afecta a las salidas digitales**: un pin con `TRIS` = 0 y `ANSEL` = 1 igual saca el nivel de `LAT`. Se baja en todo pin digital (regla del proyecto) y evita resultados raros al hacer leer-modificar-escribir sobre `PORTx`. **Nombre en C:** el datasheet lo llama `ANSELA5`, pero en `pic16f18426.h` el bit de RA5 es `ANSELAbits.ANSA5` (y `ANSELCbits.ANSC4` para RC4).

---

## 4. Oscilador

### 4.1 Bits de configuración (CONFIG1, DS p.27)

| Campo | Valores relevantes |
|---|---|
| `RSTOSC[2:0]` (reloj al arrancar) | `110` HFINTOSC a 1 MHz (OSCFRQ = 4 MHz, divisor 4:1) · `001` HFINTOSC con PLL ×2 a 32 MHz · `000` HFINTOSC a 32 MHz (según la tabla de CONFIG1) |
| `FEXTOSC[2:0]` | `100` = sin oscilador externo |
| `CSWEN` | `1` = el código puede cambiar `NOSC` y `NDIV` |
| `CLKOUTEN` | `1` = sin salida de reloj Fosc/4 (solo aplica con EC) |

> ⚠️ **Inconsistencia en el datasheet:** la tabla de `RSTOSC` lista `000` como "HFINTOSC a 32 MHz", pero la tabla de `NOSC` en `OSCCON1` (DS p.133) marca `000` como reservado. Para evitar el problema, la opción propuesta abajo no usa `RSTOSC = 000`.

### 4.2 Cómo llegar a 16 MHz [PROPUESTA ⚠️]

Se arranca con `RSTOSC = 110` y luego el código sube la frecuencia:

| Paso | Registro | Valor | Fuente |
|---|---|---|---|
| 1 | `OSCFRQ` (HFFRQ[2:0]) | `0b101` = 16 MHz (con NOSC = 110, sin PLL) | DS p.139 |
| 2 | `OSCCON1` | `NOSC = 110`, `NDIV = 0000` → `0x60` | DS p.133 |
| 3 | esperar | `OSCCON3[ORDY] = 1` (oscilador listo) | DS p.~137 |

Tabla de `OSCFRQ` (DS p.139): `110`→32 MHz · `101`→16 MHz · `100`→12 MHz · `011`→8 MHz · `010`→4 MHz · `001`→2 MHz · `000`→1 MHz.

### 4.3 Exactitud

- **±2 % a la calibración (nominal)** (DS p.4). Puede derivar con VDD y temperatura (DS p.410).
- Ajuste fino con **`OSCTUNE[HFTUN]`**, 6 bits con signo (`011111` = máx., `000000` = centro, `100000` = mín.) (DS p.138). **El datasheet lo señala como el método preferido** para corregir el baud rate.
- Si cambias el reloj con el receptor activo puede haber errores de recepción: espera a que `RCIDL = 1` antes (DS p.411).

---

## 5. PPS (selección de pines de periféricos)

| Dato | Valor | Fuente |
|---|---|---|
| Entrada RX1 por defecto | `RX1PPS` (en el `.h` de XC8 se llama **`RX1DTPPS`**) apunta a **RC5** (`0x15`) | DS p.231 (Tabla 16-1), p.232 (16-2) |
| Entrada CK1 por defecto | `CK1PPS` apunta a RC4 (`0x14`) | DS p.231 |
| **Salida TX1/CK1** | se asigna con `RxyPPS = 0b001111` = **`0x0F`**. Ejemplo: `RC4PPS = 0x0F` | DS p.233 (Tabla 16-3) |
| Salida PWM6 | `RxyPPS = 0x0D` | DS p.233 |
| Salida PWM7 | `RxyPPS = 0x0E` | DS p.233 |
| Salidas CCP1–CCP4 | `0x09`, `0x0A`, `0x0B`, `0x0C` | DS p.233 |
| Valores de entrada (`xxxPPS`) | RC5 = `0x15`, RC4 = `0x14`, RC3 = `0x13`, RC2 = `0x12`, RC1 = `0x11`, RC0 = `0x10`, RA5 = `0x05`, RA4 = `0x04`, RA2 = `0x02`, RA1 = `0x01`, RA0 = `0x00` | DS p.232 (Tabla 16-2) |
| Estado tras un reset | `PPSLOCKED = 0` (desbloqueado). Tras un POR, las selecciones vuelven a su valor por defecto | DS p.234–235 |
| Bloqueo opcional | secuencia `0x55`, `0xAA` en `PPSLOCK`, luego `PPSLOCKED = 1`, con interrupciones suspendidas | DS p.234 |

**No hace falta desbloquear PPS** para configurar el pin de TX al arrancar. Bloquear después es opcional.

---

## 6. EUSART1

### 6.1 Registros (DS p.425–431)

| Registro | Función | Bits clave |
|---|---|---|
| `RC1REG` | dato recibido (solo lectura) | |
| `TX1REG` | dato a transmitir | |
| `SP1BRGL` / `SP1BRGH` | generador de baud rate (16 bits) | |
| `RC1STA` | control de recepción | `SPEN`, `CREN`, `FERR` (solo lectura), `OERR` |
| `TX1STA` | control de transmisión | `TXEN`, `SYNC`, `BRGH`, `TRMT` |
| `BAUD1CON` | control del baud | `BRG16`, `RCIDL`, `SCKP` |
| `PIE3` | habilitación de interrupciones | `RC1IE` (bit 5), `TX1IE` (bit 4) (DS p.106) |
| `PIR3` | banderas | `RC1IF`, `TX1IF` (**solo lectura**) |
| `INTCON` | interrupciones globales | `GIE` (bit 7), `PEIE` (bit 6) (DS p.102) |

Los nombres en C probablemente serán `RC1STAbits.SPEN`, `PIE3bits.RC1IE`, etc. ⚠️ **Confírmalos en `pic16f18426.h`** (carpeta de XC8: `...\xc8\v2.40\pic\include\proc\`).

### 6.2 Baud rate

Con `BRGH = 1` y `BRG16 = 1`: `Baud = Fosc / [4 × (n + 1)]` (n = `SP1BRGH:SP1BRGL`) (DS p.411–412, y descripción de `BRGH` en p.430).

| Fosc | Baud deseado | n (`SP1BRG`) | Baud real | Error | Fuente |
|---|---|---|---|---|---|
| 32 MHz | 115200 | **68** | 115,9 k | +0,64 % | tabla DS 28-2, p.412–414 |
| **16 MHz** | **115200** | **34** | 114,3 k | **−0,79 %** | calculado con la fórmula ⚠️ |
| 16 MHz | 57600 | 68 | 57,97 k | +0,64 % | calculado ⚠️ |

El error de −0,79 % se suma a la exactitud del HFINTOSC (±2 % nominal). Peor caso combinado ≈ −2,8 %: **al límite**. Si hay errores de framing, usa `OSCTUNE` o prueba con 32 MHz.

### 6.3 Secuencia de inicialización (TX + RX)

Resumida de DS p.406 (TX) y p.409 (RX), con el orden que recomiendo ⚠️:

| # | Acción | Detalle |
|---|---|---|
| 1 | Reloj | sección 4.2 |
| 2 | Pines analógicos → digitales | `ANSELC4 = 0` y `ANSELC5 = 0` (**por defecto están en 1 = analógico**, DS p.205) |
| 3 | Dirección | RC4 como salida (`TRISC4 = 0`), RC5 como entrada (`TRISC5 = 1`) |
| 4 | Nivel de RX (opcional) | `INLVLC5 = 0` (TTL) |
| 5 | PPS | `RX1DTPPS = 0x15` (en el datasheet se llama `RX1PPS`) (ya es el valor por defecto), `RC4PPS = 0x0F` |
| 6 | Baud | `BRG16 = 1`, `BRGH = 1`, `SP1BRGH = 0`, `SP1BRGL = 34` |
| 7 | Modo | `SYNC = 0` (asíncrono) |
| 8 | Habilitar | `SPEN = 1`, `TXEN = 1`, `CREN = 1` |
| 9 | Interrupciones | `RC1IE = 1`; **`TX1IE = 0`** al inicio; `PEIE = 1`; `GIE = 1` |

### 6.4 Comportamiento que condiciona el diseño (todo DS p.405–410)

- **`TX1IF` está en 1 siempre que `TX1REG` esté vacío, aunque `TX1IE` sea 0.** Por eso la ISR debe comprobar **`TX1IE && TX1IF`** y no solo la bandera.
- **Habilita `TX1IE` solo cuando haya datos por enviar** y deshabilítalo al escribir el último byte.
- `TX1IF` es de solo lectura: se limpia al escribir `TX1REG`. Es válido solo desde el segundo ciclo de instrucción tras la escritura (no lo leas inmediatamente después).
- **El receptor tiene una FIFO de 2 bytes.** Un tercer byte completo sin que se haya leído `RC1REG` provoca **overrun** (`OERR = 1`) y el módulo deja de recibir. Se limpia con **`CREN = 0` y luego `CREN = 1`** (o `SPEN = 0/1`).
- Con ~86,8 µs por byte a 115200 baud, la ISR de RX debe atenderse antes de que termine de llegar el tercer byte: **≈ 2 tiempos de byte (~174 µs)** ⚠️. En la Fase 2 ninguna otra ISR (ADC, timer) puede retener la CPU tanto tiempo.
- `FERR` se lee **antes** que `RC1REG` (aplica al byte de la cima de la FIFO). Un error de framing **no genera interrupción** y no impide recibir.
- `RC1IF` es de solo lectura: se limpia al leer `RC1REG`.
- `SPEN = 1` habilita automáticamente el driver de salida del pin TX (DS p.406). Aun así, dejo `TRISC4 = 0` ⚠️.
- Paridad: **no existe en hardware** (se haría con el 9.º bit por software).

### 6.5 Errata aplicable (ER p.2–9)

| Elemento | Qué pasa | ¿Aplica a la revisión A4 (actual)? |
|---|---|---|
| 1.9.2 Doble byte transmitido | un byte escrito en `TX1REG` justo cuando el TSR se vacía puede salir dos veces | **No** (solo A1 y A2) |
| 1.9.1 Pérdida del segundo byte | solo en modo síncrono | **No** (solo A1 y A2) |
| 1.7.1 Duty del CCP incorrecto | solo con T2PR = 0 y prescaler ≠ 1:1 | **No** (solo A1 y A2) |
| 1.1.2 ADCC doble muestra | tiempo de adquisición extra entre las dos conversiones | **Sí** (afecta a A4) |
| 1.6.1 Resistencia de la Flash | mínimo 1000 ciclos de escritura (no los 10 000 que dice el datasheet) | **Sí** |
| 1.2.1 I²C, 1.8.1 SMT | no se usan en el proyecto | Sí, irrelevante |

**Comprueba tu revisión de silicio:** MPLAB X → *Window > Dashboard* → botón de refrescar (ER p.1). ID del dispositivo del PIC16F18426 = `0x30D2`; revisiones: A1 = `0x2001`, A2 = `0x2002`, **A4 = `0x2004`**.

Si resultara ser A1/A2, la regla "escribe `TX1REG` solo cuando `TX1IF = 1`" ya cubre el workaround de 1.9.2 (el diseño de la ISR de TX la cumple).

---

## 7. Interrupciones y XC8

**Vector:** único, en `0x0004` (DS p.~100; sin VIC ni prioridades). El hardware guarda el contexto básico automáticamente en registros sombra (DS p.~100); XC8 genera el resto.

**Reglas de la guía de XC8 (UG 5.8, p.111–118):**

| Regla | Cita |
|---|---|
| Declarar como `void __interrupt() nombre(void)` (sin `high_priority`/`irq()` en este chip) | UG p.113 |
| Dentro de la ISR, comprobar **bandera y habilitación** de cada fuente | UG p.113–114 |
| **Limpiar la bandera** al terminar de atender la fuente (en RC1IF/TX1IF se limpia leyendo/escribiendo el registro, no a mano) | UG p.113 |
| **No** rehabilitar `GIE` dentro de la ISR (lo hace `retfie`) | UG 5.8.5 |
| ISR lo más corta y simple posible | UG p.113 |
| Variables compartidas con la ISR: declararlas **`volatile`** | UG 5.8.6 |
| Acceso a variables de más de 1 byte compartidas: **no es atómico**. Proteger con `di()` / `ei()` si hace falta | UG 5.8.6 |
| No llamar a una misma función desde la ISR y desde `main`: XC8 la **duplica** (no reentrante) | UG 5.8.7 |
| Macros `ei()` y `di()` (afectan `GIE`) disponibles con `#include <xc.h>` | UG 5.8.5 |

---

## 8. Bits de configuración en XC8

| Tema | Detalle | Fuente |
|---|---|---|
| Sintaxis | `#pragma config "SETTING=VALOR"`. Valores numéricos en decimal o `0x…`, **nunca en binario (`0b`)** | UG p.77 |
| Nombres exactos de cada opción | archivo **`pic_chipinfo.html`** en la carpeta `docs` de la instalación de XC8 | UG p.77 |
| Generar los pragmas | MPLAB X: *Window > Target Memory Views > Configuration Bits*, luego *Generate Source Code to Output* (copiar y pegar) | UG p.77 |
| Recomendación | **programar todos** los bits de configuración, sin dejar ninguno en su valor por defecto | UG p.77 |

**Propuesta de valores [⚠️ a confirmar con el generador de MPLAB X]:**

| Bit | Valor propuesto | Motivo |
|---|---|---|
| `FEXTOSC` | sin oscilador externo (`100`) | se usa el HFINTOSC |
| `RSTOSC` | HFINTOSC 1 MHz (`110`) | punto de partida limpio para el código de §4.2 |
| `CSWEN` | `1` | el código cambia `NOSC`/`OSCFRQ` |
| `LVP` | **`1` (habilitado)** | con `LVP = 1`, RA3 es MCLR y el bit `MCLRE` se ignora (DS p.33). Con `LVP = 0` haría falta alto voltaje en MCLR para programar |
| `WDTE` | `00` (desactivado) durante el desarrollo | activar en la versión final (DS p.31) |
| `BOREN` | `11` (BOR activo) | protege contra caídas de tensión (DS p.29) |
| `STVREN` | `1` | reset en desbordamiento de pila |
| `PPS1WAY` | `0` si se quiere poder cambiar PPS más de una vez | el reset de fábrica es 1 (DS p.29) |
| `DEBUG` | lo gestiona MPLAB X/SNAP | DS p.29 |
| `PWRTS` | el bit alto no funciona en A1 (ER 1.11.1); usar `01` o `00` | |
| `CONFIG5` (protección de código) | **sin leer todavía** | pendiente |

---

## 9. Fase 2 (resumen; falta lectura detallada)

### 9.1 PWM para el auto-cero (I0 / Q0)

| Dato | Valor | Fuente |
|---|---|---|
| Módulos | **PWM6 y PWM7** (10 bits) o CCP1–CCP4 en modo PWM | DS p.2 |
| Registros | `PWMxDCH` (8 bits altos) + `PWMxDCL[7:6]` (2 bits bajos); `PWMxCON` | DS cap. 24 |
| Base de tiempo | Timer2 por defecto (`CCPTMRS`: reset → TMR2) | DS cap. 24 |
| Período | `(T2PR + 1) × 4 × Tosc × Prescaler(T2)` | DS Eq. 24-1 |
| Ciclo de trabajo | `PWMxDC(10 bits) / [4 × (T2PR + 1)]` | DS Eq. 24-3 |
| Reloj de Timer2 | **debe ser Fosc/4** para que el PWM funcione bien | DS §24.3 |
| Actualización | `PWMxDCH/L` están **doble buferizados**: se actualizan cuando `T2TMR = T2PR` | DS §24.1 |
| Pin | hay que **limpiar el bit TRIS** y enrutar con `RxyPPS` | DS cap. 24 |

Ejemplo ⚠️: con Fosc = 16 MHz, `T2PR = 255`, prescaler 1:1 → período = 256 × 4 × 62,5 ns = **64 µs (15,6 kHz)** con **10 bits de resolución** (4 × 256 = 1024 pasos). El filtro RC de salida debe atenuar bien esa frecuencia.

### 9.2 ADC

12 bits (ADCC), 11 canales externos, FVR de 1,024 / 2,048 / 4,096 V. Registros que existen: `ADCON0–3`, `ADPCH`, `ADREF`, `ADCLK`, `ADACQ`, `ADPRE`, `ADCAP`, `ADRES`, `ADSTAT`, `ADACT`, `ADCNT`. **Pendiente** leer: tiempo de adquisición, alineación del resultado, disparo automático por Timer (`ADACT`) y referencia (`ADREF`). Con VDD = 3,7 V y I/Q centradas en ~1,85 V, cada cuenta vale ~0,9 mV (3,7 V / 4096) ⚠️.

### 9.3 Timer2/4/6 (8 bits) como base de tiempo con interrupción [VERIFICADO en T-05]

Los temporizadores pares (Timer2, 4 y 6) son el mismo módulo con los mismos registros: lo que dice el DS del Timer2 vale para los otros (DS cap. 20, p.285). En T-05 el parpadeo de prueba usa **Timer4** y deja Timer2 para el PWM del auto-cero.

| Dato | Valor | Fuente |
|---|---|---|
| Direcciones (`pic16f18426.h`) | Timer2: `T2TMR` 0x28C, `T2PR` 0x28D, `T2CON` 0x28E, `T2HLT` 0x28F, `T2CLKCON` 0x290, `T2RST` 0x291. Timer4: las mismas seis, desde 0x292 hasta 0x297 (`T4CON` 0x294, `T4CLKCON` 0x296). El registro de período se llama `TxPR`, no `PRx` | .h |
| `TxCON` | bit 7 `ON`; bits 6:4 `CKPS` (111 = 1:128); bits 3:0 `OUTPS` (0000 = 1:1 … 1111 = 1:16, o sea valor + 1) | DS §20.11, p.300–305 |
| `TxCLKCON.CS` | `0001` = **Fosc/4**; `0010` = Fosc; **`0000` (el valor de reset) = el pin externo `TxCKIPPS`**, no el reloj del chip. Sin escribir `CS` el temporizador no cuenta | DS Tabla 20-3, p.305 |
| Período | `TxTMR` sube hasta `TxPR` y a la cuenta siguiente vuelve a 0: **`TxPR` + 1 cuentas** | DS §20.1, p.286 |
| Interrupción | se genera cuando el postscaler completa `OUTPS` + 1 coincidencias con `TxPR` | DS §20.4, p.287 |
| Escribir `TxCON` | limpia prescaler y postscaler, pero **no** pone `TxTMR` en 0 | DS §20.1, p.286 |
| Bandera | `PIR4bits.TMRxIF` "debe borrarse por software". Habilitación: `PIE4bits.TMRxIE`. Timer2 = bit 1, Timer4 = bit 3, Timer6 = bit 5 | DS p.116 (descripción de PIR4); .h |
| Modo | `TxHLT` vale 0 al reset (`MODE` = 00000, compuerta por software): cuenta libre mientras `ON` = 1 | DS cap. 20, Fig. 20-3 |
| `PMD1` | `mcc.c` lo deja en 0x00: todos los temporizadores tienen reloj | código del proyecto |

Cuenta usada en `heartbeat.c` (código de prueba `[T-05 PRUEBA]`): Fosc 16 MHz → Fosc/4 = 4 MHz → prescaler 1:128 = 31 250 Hz → `T4PR` = 124 (125 cuentas = 4 ms) → postscaler 1:5 = una interrupción cada 20 ms → 25 interrupciones = 0,5 s → el LED de RA5 se invierte cada 0,5 s = parpadeo de 1 Hz. Es exacto con el reloj nominal; la exactitud real depende del oscilador (±2 %), que mide la prueba 0.4. Compilado y revisado en el ensamblador (`T4CON` = 0xF4, `T4CLKCON` = 0x01, `T4PR` = 124); **sin probar en el chip**.

---

## 10. Buenas prácticas de C para PIC (TB3261 y XC8)

| Práctica | Fuente |
|---|---|
| `#include <xc.h>` y acceder a registros con la forma `REGISTERbits.BIT` (ej. `ADCON0bits.ADON = 1`) | TB §3.1.1, UG §5.2.7 |
| Alternativas equivalentes: máscaras `_REG_BIT_MASK` y posiciones `_REG_BIT_POSITION` | TB §3.1.2–3.1.3 |
| **No asumir que los registros arrancan en 0.** Ejemplos reales: `ANSELC = 0xFF`, `INLVLC = 0xFF`. Escribir explícitamente cada bit que importa | TB §3.2 |
| Para las pausas de inicialización hace falta `_XTAL_FREQ` definido (los retardos están prohibidos en la lógica de la práctica) | UG (sección de funciones de retardo) |
| Algunos registros multibyte se leen en un orden obligatorio (en PIC18/timers 16 bits); usar las macros de XC8 | UG §5.2.7.1 |

**AN688 (layout del ADC):** es una nota de 1999 sobre un ADC externo, pero sus principios aplican al diseño de PCB: separar la parte digital de la analógica, usar plano de tierra sin cortes, desacoplar cada integrado y no hacer que el ADC "cabalgue" entre tierras. Es relevante para decidir cómo conectar el PIC a PCB2, no para el firmware.

---

## 11. Efecto sobre `especificacion_diseno_uart.md`

| Punto de la especificación | Estado tras leer los documentos |
|---|---|
| PWM de auto-cero en "PWM3/PWM4" | **Corregir:** en este chip son **PWM6/PWM7** (o CCP1–4) |
| Código PPS de TX | **`RC4PPS = 0x0F`** (verificado) |
| Pines RC4/RC5 para TX/RX | **Válidos.** RX1 viene por defecto en RC5; hay que bajar `ANSELC4/5` |
| `LVP` "desactivado con SNAP" | **Error mío:** debe quedar **habilitado (`LVP = 1`)** |
| Margen de nivel de RX (≈3,0 V) | **Confirmado:** Schmitt exige 2,96 V; con TTL basta 1,73 V |
| SP1BRG = 34 a 16 MHz | **Confirmado** (−0,79 %), pero añadir que el oscilador es ±2 % |
| RAM | 2048 B (no 1 KB) |
| Errores del EUSART | A4: ninguno aplica. A1/A2: la regla de escribir con `TX1IF = 1` los cubre |
| Pines de los pendientes `[VERIFICAR]` | Pinout y VDD resueltos |

---

## 12. Pendientes de leer a fondo

- Capítulo 7 (Interrupts): latencia exacta y registros `PIR*`/`PIE*` restantes.
- ADCC (cap. 27): adquisición, justificación, disparo por Timer, tiempos de conversión.
- Timer2 (cap. 20): leído para T-05, ver §9.3. Falta `CCPTMRS` y cómo comparten Timer2 los dos PWM.
- IOC (cap. 15) para el pulsador de auto-cero.
- Especificaciones de BOR (`VBOR`) para elegir `BORV` con VDD = 3,7 V.
- `CONFIG5` (protección de código) y `PMD` (apagar periféricos sin uso).
- Cabecera real `pic16f18426.h` de la instalación de XC8 para confirmar los nombres en C.
