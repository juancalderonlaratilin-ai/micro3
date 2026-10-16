# Micro 3: contexto del proyecto

Instrumento de inspección por corrientes de Foucault. El PIC adquirirá las señales I y Q, generará el auto-cero con PWM y se comunicará con LabVIEW por UART.

## Estructura de la carpeta

- `hardware/`: archivos de Altium (esquemático, PCB, BOM). Son binarios, no se editan desde aquí.
- `firmware/Micro_Firmware.X/`: proyecto de MPLAB X (PIC16F18426, XC8 2.40, programador SNAP).
- `docs/`: especificaciones. **Empieza por `docs/especificacion_diseno_uart.md`.**

## Reglas del proyecto

- C "bare-metal" escribiendo directamente en los registros (SFR). **No usar MCC.**
- Sin esperas bloqueantes en la lógica: nada de `while(!TXIF)` ni `__delay_ms` en el programa principal.
- UART por interrupciones con FIFO circular. Protocolo binario, no ASCII.
- Oscilador HFINTOSC a 16 MHz, UART 8N1 a 115200 baud (SP1BRG = 34).
- Firmas pedidas en clase: `tx_DAT(const char *DATA, uint16_t size)` y `tx_string(const char *data)`.

## Estado

- Fase 1 (actual): enlace UART PC ↔ PIC. Sin ADC ni PWM todavía.
- Fase 2: ADC de I/Q, PWM de auto-cero, máquina de estados.

Cuando cambie una decisión de diseño (pines, baud, formato de trama), actualizar `docs/especificacion_diseno_uart.md`.
