# Contexto Técnico y Especificación de Proyecto: Práctica 3 / Proyecto Final (Microcontroladores)

> **Propósito del documento**: Servir como especificación técnica completa y *prompt de contexto de sistema* para modelos de inteligencia artificial y desarrolladores de firmware. Contiene todos los requerimientos funcionales, arquitectónicos, fórmulas matemáticas y restricciones de diseño extraídos de las notas de laboratorio, tablero y sesiones de clase.

---

## 1. Visión General del Proyecto

El sistema consiste en un instrumento de inspección basado en **ensayos por corrientes de Foucault (Eddy Current Testing) o detección de impedancia** para inspección no destructiva de tuberías/tubos.

El microcontrolador actúa como el núcleo de adquisición y control analógico, realizando:
1. **Calibración y Control de Auto-Cero**: Reemplazar trimmers físicos ($I_0$ y $Q_0$) mediante salidas analógicas generadas por **PWM** filtrado.
2. **Adquisición Dual síncrona/multiplexada (ADC)**: Muestreo de las componentes en cuadratura (Parte Real $I$ y Parte Imaginaria $Q$).
3. **Procesamiento y Transmisión Serial no bloqueante (UART)**: Envío de paquetes de datos formateados hacia un PC con interfaz gráfica (ej. **LabVIEW**).
4. **Visualización y Diagnóstico**: Plano de impedancias ($R$ vs $X_L$, o $I$ vs $Q$) para discriminar entre **tubo sano** o **tubo defectuoso/dañado**.

---

## 2. Hardware y Herramientas del Proyecto

* **Microcontrolador**: Familia Microchip PIC (referencia base de clase: `PIC18F...` / `PIC16F18426` u homologables de 8 bits con periféricos independientes del núcleo).
* **Entorno de Desarrollo (IDE)**: MPLAB X IDE (versión $6.00$ a $6.30$).
* **Compilador**: XC8 (versión $2.40$ a $2.45$).
* **Programador/Debugger**: PICkit / SNAP.
* **Interfaz de Comunicación PC**: Conversor USB a UART TTL (FTDI, CP2102, CH340).
* **Restricción de Diseño Crítica (REGLA DE ORO)**: 
  * ❌ **NO USAR MCC (MPLAB Code Configurator)**.
  * ✔️ Todo el código debe escribirse en **C bare-metal**, manipulando directamente los registros especiales (**SFRs**: `TRISx`, `ANSELx`, `PORTx`, `LATx`, `T2CON`, `PR2`, `CCPRx`, `RCSTA`, `TXSTA`, `SPBRG`, etc.).

---

## 3. Requerimientos de Firmware y Periféricos

```
                  +----------------------------------------------+
                  |            MICROCONTROLADOR PIC              |
                  |                                              |
[Boton Auto-Cero]->[GPIO INT]                                     |
                  |                                              |
[Canal I (Real)] ->[ADC CH0]  ---> [ Buffer Circular / FIFO ]   |
[Canal Q (Imag)] ->[ADC CH1]                  |                  |
                  |                           v                  |
                  |                    [ FSM / Dispatcher ]      |
                  |                           |                  |
                  |              +------------+------------+     |
                  |              |                         |     |
                  |              v                         v     |
                  |         [PWM Control]           [UART TX (ISR)]
                  |        (Timer 2 / CCP)                 |
                  +--------------|-------------------------|-----+
                                 |                         |
                                 v                         v
                       Filtro Pasa-Bajos            USB-Serial TTL
                          (DAC Pasivo)                     |
                                 |                         v
                         Ajuste de Cero              PC / LabVIEW
                        ($I_0$ / $Q_0$)           (Plano Impedancia)
```

---

### 3.1. Módulo de Auto-Cero (Calibración Inicial)

* **Problema físico**: En reposo (puntas al aire o sobre tubo patrón sin defecto), el puente analógico tiene un desbalance de offset ($I_0, Q_0$). Antiguamente se ajustaba manualmente con potenciómetros/trimmers.
* **Solución digital**:
  * El microcontrolador sustituye los trimmers mediante **dos salidas PWM** que alimentan filtros pasa-bajos pasivos RC (creando un **DAC por PWM**).
  * La tensión continua promedio $V_{out}$ está dada por:
    $$V_{out} = V_{DD} \cdot D = V_{DD} \cdot \left(\frac{T_{ON}}{T_{PWM}}\right)$$
  * **Mecanismo de ajuste**:
    * Se dispara mediante comando serial o por pulsador/botón físico.
    * El microcontrolador ejecuta un lazo de aproximación (búsqueda binaria o integración progresiva) leyendo el ADC y variando el duty cycle del PWM hasta que el valor medido en las puntas sea idéntico a cero (o el centro de escala deseado: $V_{ref}/2$ si es señal bipolar con offset).

---

### 3.2. Muestreo Analógico (ADC) y Teoría de Muestreo

* **Canales requeridos**: Mínimo 2 canales analógicos ($I$: En fase / $Q$: Cuadratura).
* **Criterio de Nyquist y Filtro Anti-Aliasing**:
  * Sea $f_{max}$ la componente armónica máxima de la señal AC que sale del demodulador síncrono.
  * La frecuencia de muestreo $f_s$ debe satisfacer estrictamente:
    $$f_s > 2 \cdot f_{max}$$
  * Se requiere un filtro pasa-bajos analógico (Anti-aliasing) de orden $n$ con frecuencia de corte:
    $$f_c \le \frac{f_s}{2}$$
* **Sincronización por Hardware**:
  * Se recomienda disparar el ADC mediante un **Timer** periódico (p. ej. Timer 1 con overflow o CCP Special Event Trigger), **nunca con retardos por software (`__delay_ms`)**.

---

### 3.3. Temporizadores (Timers) y Generación de PWM

Según las notas de diseño de clase:
1. **Timer 0 / Timer 1 (Temporización de 16 bits y Eventos)**:
   * Funcionamiento por conteo y preescalador (*prescaler*).
   * Para una frecuencia de oscilador $F_{osc}$, la frecuencia del ciclo de instrucción es:
     $$F_{cy} = \frac{F_{osc}}{4}$$
   * Conteo para temporizador de carga:
     $$\text{Valor Carga} = \text{Resolución Máx} - \frac{T_{evento} \cdot F_{osc}}{4 \cdot \text{Prescaler}}$$
2. **Timer 2 (Base de tiempo para PWM)**:
   * Opera por comparador periódico contra el registro `PR2` (reseteo automático sin recarga manual por software).
   * Periodo del PWM:
     $$T_{PWM} = [(PR2) + 1] \cdot 4 \cdot T_{osc} \cdot (\text{TMR2 Prescale Value})$$
   * Resolución del Duty Cycle mediante `CCPRxL` y bits `<DCxB1:DCxB0>` en `CCP1CON`.

---

### 3.4. Comunicación Serial UART y Manejo de Buffers

#### A. Requisitos de Transmisión
* Baudrates estándar de trabajo: **9600 bps** o **19200 bps** (o mayores según la tasa de muestras del ADC).
* Tiempo de bit:
  $$T_{bit} = \frac{1}{\text{Baudrate}}$$
  *(ej. Para $9600 \text{ bps}$, $T_{bit} \approx 104.16\,\mu\text{s}$)*.
* Cálculo del Baud Rate Generator (BRG):
  $$\text{SPBRG} = \frac{F_{osc}}{16 \cdot \text{Baudrate}} - 1 \quad (\text{en modo alta velocidad, } BRGH=1)$$

#### B. Arquitectura de Transmisión No Bloqueante (ISR / Flags)
* **Prohibido**: Esperar en bucles activos bloqueantes tipo `while(!TXIF);` dentro de la lógica principal de adquisición.
* **Mecanismo**:
  * **TXREG**: Registro de retención de transmisión (1 byte).
  * **TSR**: Transmit Shift Register (registro de desplazamiento de hardware).
  * **Flag TXIF**: Indica que `TXREG` está vacío y listo para recibir el siguiente byte.
  * **Flag TRMT**: Indica que `TSR` terminó de enviar el último bit de stop.
* **Firmware**: Implementar una cola o **Buffer Circular (FIFO)** para la transmisión.

#### C. Firmas de Funciones Exigidas en Clase:
```c
// Envía un bloque arbitrario de bytes apuntado por DATA de longitud 'size'
bool tx_DAT(const char *DATA, uint16_t size);

// Envía una cadena de caracteres terminada en NULL ('\0')
bool tx_string(const char *data);
```

* **Comportamiento esperado de `tx_DAT` y `tx_string`**:
  * Verifican si el periférico o el buffer circular está libre/disponible.
  * Retornan `true` si el paquete fue aceptado o encolado exitosamente; `false` si el buffer está lleno o el bus ocupado.
  * La transmisión byte a byte debe ser asistida por la interrupción `TXIE` / `TXIF`.

#### D. Protocolo de Comunicación (Formato de Trama ASCII)
* Codificación en **ASCII legible** para visualización directa en terminal o parser en LabVIEW:
  * Formato sugerido: `I:<valor_real>,Q:<valor_imag>\r\n` o `$DAT,<I>,<Q>*<checksum>\n`
  * Todo mensaje termina con delimitador de fin de línea: `\n` (`0x0A`) o `\r\n` (`0x0D, 0x0A`).
  * En strings C, el terminador local de memoria es el byte nulo `0x00`.

---

## 4. Máquina de Estados y Arquitectura de Software

El firmware debe estructurarse mediante una arquitectura guiada por eventos (*Event-Driven Architecture*):

1. **Estado 0: INIT**:
   * Configuración de oscilador interno/externo ($F_{osc}$).
   * Configuración de puertos (entradas analógicas en ADC, salidas PWM, pines TX/RX).
   * Configuración de Timers, UART, PWM y habilitación de interrupciones globales (`GIE`, `PEIE`).
2. **Estado 1: CALIBRATE (Auto-Cero)**:
   * Ajuste iterativo del PWM de $I_0$ y $Q_0$ hasta equilibrar lecturas basales del ADC a cero.
3. **Estado 2: RUN / ACQUIRE**:
   * Muestreo temporizado por interrupción de Timer.
   * Empaquetado de mediciones ($I, Q$).
   * Puesta en cola de datos hacia el buffer serial.
4. **Estado 3: ERROR / ALARM**:
   * Detección de pérdida de señal, saturación de entrada o buffer overrun.

---

## 5. Software PC / Interfaz Gráfica (LabVIEW o equivalente)

* **Entrada**: Flujo continuo de tramas ASCII por puerto COM virtual.
* **Funciones**:
  1. Desempaquetado de cadenas de texto y conversión numérica a punto flotante ($I$ y $Q$).
  2. **Gráfico XY (Plano de Impedancia Compleja)**:
     * Eje X: Parte Real ($I$).
     * Eje Y: Parte Imaginaria ($Q$).
  3. **Diagnóstico**:
     * Umbralización espectral / radio de curvatura.
     * Indicador booleano en panel frontal: **TUBO SANO (Verde)** vs **TUBO DAÑADO (Rojo)**.
