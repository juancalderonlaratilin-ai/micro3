# Prompt base para Claude Code (tarea T-03)

Se pega al abrir `claude` en `C:\Users\Tatan\Documents\micro3`.

```
Eres el desarrollador de código de este proyecto. Lee, en este orden:
CLAUDE.md (se carga solo), MASTER_PLAN.md, docs/instrucciones_claude_code_uart.md,
las filas C-05 a C-10 de docs/revision_codigo_mcc.md y docs/criterios_diseno.md §6.1.

Tu tarea es T-03 y solo T-03: adaptar la UART que generó MCC (eusart1.c, eusart1.h,
interrupt_manager.c, main.c) para que no bloquee, use buffers circulares sin contador
y tenga tx_DAT y tx_string. El detalle de cada función está en
docs/instrucciones_claude_code_uart.md. Síguelo al pie de la letra.

Antes de escribir código:
1. Comprueba que en eusart1.c los buffers ya son 64 (TX) y 32 (RX). Si siguen en 8, detente y avísame.
2. Dime en máximo 8 líneas qué vas a cambiar y qué dudas tienes. Espera mi "adelante".

Reglas:
- Trabaja por pasos y detente después de cada uno para que yo lo revise y lo entienda:
  (1) estructura de datos y quitar los punteros de handlers,
  (2) EUSART1_Write y EUSART1_Read,
  (3) las dos ISR (TX y RX),
  (4) tx_DAT y tx_string,
  (5) main.c de eco.
- Comentarios en español, línea por línea: qué hace y por qué. Decodifica los nombres
  de registros la primera vez (RC1IE = Receive 1 Interrupt Enable, etc.).
  Marca cada cambio con su etiqueta [C-0X].
- No inventes nombres de registros ni de bits: verifícalos en pic16f18426.h (XC8 2.40) o en el datasheet.
- No cambies pines, reloj, baud ni bits de configuración. No toques pin_manager.c, mcc.c,
  device_config.c ni el .mc3. NO pulses ni simules "Generate" de MCC.
- Sin while que espere, sin __delay_ms.
- No hagas commit. No marques nada como "probada".

Al terminar: compila con XC8 2.40, dime los warnings tal cual, lista las dudas que quedaron
y dime qué pruebas debo correr (guía: docs/guia_prueba_uart_snap.md).
```

## Para después de probar en el chip

```
Resultados de la prueba de la UART con el SNAP:
[pega aquí la salida de herramientas/prueba_uart.py y lo que viste]
Analiza qué falla y por qué, propón el cambio mínimo, y no marques T-03 como probada
salvo que todas las pruebas hayan pasado. Actualiza docs/estado.md.
```
