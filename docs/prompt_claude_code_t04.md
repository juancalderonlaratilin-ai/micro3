# Prompt base para Claude Code (tarea T-04: protocolo de tramas con CRC-8)

Se pega al abrir `claude` en `C:\Users\Tatan\Documents\micro3`.

```
Eres el desarrollador de código de este proyecto. Lee, en este orden:
CLAUDE.md (se carga solo), MASTER_PLAN.md (fila T-04 y "Cambios propuestos"),
docs/especificacion_protocolo.md (completo), docs/estado.md y, de eusart1.h, las
funciones EUSART1_Read, EUSART1_get_last_status, EUSART1_get_overflow y tx_DAT.

Tu tarea es T-04 y solo T-04: protocolo de tramas con CRC-8 en módulos NUEVOS
(crc8.c/.h y protocol.c/.h) encima de la UART. eusart1.c, eusart1.h,
interrupt_manager.c, pin_manager.c, mcc.c y device_config.c NO se tocan. NO pulses ni
simules "Generate" de MCC.

Antes de escribir código:
1. Comprueba que main.c es el eco de T-03 y que el proyecto compila (XC8 2.40).
2. Dime en máximo 8 líneas qué vas a hacer y qué dudas tienes. Espera mi "adelante".
   Valen las recomendaciones PR-1 a PR-5 de la especificación (CRC bit a bit, MODO_ECO
   como constante en main.c, SEQ de ERROR copiado si hay trama completa y 0 si no,
   sin timeout entre bytes, un ERROR por CRC malo).

Trabaja por pasos y detente después de cada uno para que yo lo revise y lo entienda
(sección 10 de la especificación):
  (1) crc8.c/.h, con los vectores de la sección 3 (verifica que "123456789" da 0xF4
      calculándolo tú con un programa aparte en C o Python, no a ojo);
  (2) frame_send: armar los 9 bytes, calcular el CRC y enviar con tx_DAT;
  (3) parser (máquina de estados ESPERA_A5, ESPERA_5A, CUERPO) con PING -> PONG;
  (4) ERROR 1 y 2 y la resincronización (descartar el primer byte y volver a pasar
      los otros 8 por el parser);
  (5) PE7 y PE8 (overflow y errores de hardware), MODO_ECO y main.c.

Reglas:
- Comentarios en español, línea por línea: qué hace y por qué. Decodifica los nombres
  la primera vez que aparecen. Formato fijo de MASTER_PLAN.md ("Cómo se explica el
  código"): una parte a la vez, "Parte N de M", no pasar a la siguiente hasta que yo
  diga "entendido".
- No inventes nombres de registros ni de bits: verifícalos en pic16f18426.h (XC8 2.40)
  o en el datasheet. Aquí casi no hacen falta: el protocolo no toca registros.
- Sin while que espere, sin __delay_ms. Nada de esto se llama desde la ISR.
- Los archivos existentes están en ISO-8859-1 con CRLF: usa la misma codificación en los
  nuevos para que MPLAB X no muestre caracteres raros.
- Agrega los archivos nuevos al proyecto de MPLAB X (nbproject/configurations.xml y
  Makefile-default.mk), como se hizo con heartbeat en T-05.
- No cambies decisiones de diseño (pines, baud, formato de trama, reloj). Si algo choca,
  detente y anótalo en "Preguntas para el planificador".
- No hagas commit. No marques nada como "probada".

Al terminar: compila con XC8 2.40, dime los warnings tal cual, marca T-04
"implementada, sin probar", actualiza docs/estado.md, y dime qué pruebas de la
especificación (2.1 a 2.13) debo correr y con qué herramienta
(herramientas/prueba_protocolo.py, que también es tuya de escribir en un paso aparte
si yo lo pido).
```

## Para después de probar en el chip

```
Resultados de las pruebas del protocolo con el SNAP:
[pega aquí la salida de herramientas/prueba_protocolo.py y lo que viste]
Analiza qué falla y por qué, propón el cambio mínimo, y no marques T-04 como probada
salvo que todas las pruebas hayan pasado. Actualiza docs/estado.md.
```
