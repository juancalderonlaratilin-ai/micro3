# Archivo Maestro de Arquitectura y Tareas (Fuente de la Verdad)
# Micro 3: contexto del proyecto

Instrumento de inspección por corrientes de Foucault. El PIC adquirirá las señales I y Q, generará el auto-cero con PWM y se comunicará con LabVIEW por UART.

## Estructura de la carpeta

- `hardware/`: archivos de Altium (esquemático, PCB, BOM). Son binarios, no se editan desde aquí.
- `firmware/Micro_Firmware.X/`: proyecto de MPLAB X (PIC16F18426, XC8 2.40, programador SNAP).
- `docs/`: especificaciones y documentación. **Empieza por `docs/especificacion_diseno_uart.md`** (qué se va a construir) y consulta **`docs/notas_pic16f18426.md`** (resumen verificado del chip, con páginas) antes de abrir los PDF grandes (datasheet de ~700 páginas, guía de XC8 de ~580).

Criterios de diseño y valores escogidos (reloj, baud, formato, niveles, buffers, protocolo): `docs/criterios_diseno.md`. Circuito UART con el módulo CP2102: `docs/circuito_uart.svg`. El TX del PIC va al adaptador **por un divisor 1 kΩ + 2,2 kΩ**, no directo.

Pruebas de cada módulo (UART primero, luego protocolo, PWM, ADC, auto-cero e integración): `docs/protocolos_prueba.md`. Cada módulo debe cumplir sus criterios antes de pasar al siguiente.

MCC: el código generado en clase NO es el firmware del proyecto. Se guarda solo como referencia, fuera del proyecto compilado (`referencia_mcc/`). Sirve para comprobar nombres de registros y valores (por ejemplo, el registro de entrada PPS de RX1 se llama `RX1DTPPS` en el `.h`, aunque el datasheet lo llame `RX1PPS`).

## Requerimientos de la Práctica 3 (se cargan en cada sesión)

@docs/Especificacion_Tecnica_Practica3_Microcontroladores.md

Ese archivo es el enunciado de la práctica (qué pide el curso). Donde contradiga las **Reglas del proyecto** de abajo o `docs/especificacion_diseno_uart.md`, mandan estas últimas. Diferencias ya decididas:

- El enunciado sugiere tramas ASCII y 9600/19200 baud. Nosotros usamos **protocolo binario** (recomendación del profesor) a **115200**.
- Su fórmula `SPBRG = Fosc/(16·baud) − 1` es de otras familias; en este chip el baud se calcula con `BRG16=1`, `BRGH=1` (ver `docs/notas_pic16f18426.md`).
- Nombres como `SPBRG`, `TXREG`, `RCSTA`, `CCP1CON` son genéricos; aquí son `SP1BRG`, `TX1REG`, `RC1STA`, etc. Comprobar en `pic16f18426.h`.

## Reglas del proyecto

- C "bare-metal" escribiendo directamente en los registros (SFR). **No usar MCC.**
- Sin esperas bloqueantes en la lógica: nada de `while(!TXIF)` ni `__delay_ms` en el programa principal.
- UART por interrupciones con FIFO circular. Protocolo binario, no ASCII.
- Alimentación VCC = 3,7 V (fuente Keysight E36233A, según el preinforme). Conversor USB-serie a 3,3 V, nunca a 5 V.
- Chip fijo: PIC16F18426 (14 pines, 8 usados, 1 de reserva: RC2). La gráfica de impedancia I vs Q se dibuja **en el PC** (LabVIEW o Python); el PIC no maneja pantalla. Presupuesto de pines en `docs/especificacion_diseno_uart.md` §3.1.
- Oscilador HFINTOSC a 16 MHz, UART 8N1 a 115200 baud (SP1BRG = 34).
- Firmas pedidas en clase: `tx_DAT(const char *DATA, uint16_t size)` y `tx_string(const char *data)`.
- **Nunca escribir nombres de registros o bits de memoria.** Comprobarlos en `pic16f18426.h` (carpeta `pic\include\proc\` de la instalación de XC8) o en el datasheet. Cuando algo se verifique, anotarlo en `docs/notas_pic16f18426.md`.
- Una sola ISR (`void __interrupt() isr(void)`, vector único). Comprobar bandera **y** habilitación de cada fuente. `TX1IF` vale 1 siempre que `TX1REG` esté vacío, aunque `TX1IE` sea 0.
- Los pines de `ANSELx` están en modo analógico al reset: bajar el bit de cada pin digital.
- PWM del auto-cero: **PWM6 / PWM7** (no PWM3/PWM4, que no existen en este chip).
- Bits de configuración: `LVP` habilitado (RA3 es MCLR). No usar RA0/RA1/RA3 (programación).

## Registro de avance (se carga en cada sesión)

@docs/estado.md

**Regla:** al terminar cada tarea, actualiza `docs/estado.md`: marca lo hecho, reescribe "Siguiente paso" y agrega una fila al registro (fecha, quién, qué se hizo, resultado). Solo hechos comprobados; si algo no se probó, escribe "sin probar". Las decisiones de diseño van en `docs/criterios_diseno.md`, no ahí. Cuando termines, dile a la persona que haga el commit.

## Estado

- Fase 1 (actual): enlace UART PC ↔ PIC. Sin ADC ni PWM todavía.
- Fase 2: ADC de I/Q, PWM de auto-cero, máquina de estados.

Cuando cambie una decisión de diseño (pines, baud, formato de trama), actualizar `docs/especificacion_diseno_uart.md`.
