# Especificación del protocolo de tramas con CRC-8 (tarea T-04)

Proyecto Micro 3 · Fase 1b · Fecha: 2026-10-21 · Estado: **PROPUESTA, sin aprobar** (no hay código)

> **Para qué sirve:** fijar dónde vive el código, el formato exacto, el CRC, los eventos, las tareas y las pruebas del protocolo **antes de escribir código**. Es la base sobre la que irán después `SET_PWM`/`GET_PWM` (`docs/especificacion_pwm.md`) y las tramas de datos I/Q.
>
> **Etiquetas:** **[VERIFICADO]** = sale de un documento de `docs/` o del código actual. **[CÁLCULO]** = cuenta propia comprobada con un script. **[PROPUESTA]** = decisión por aprobar.
>
> **Fuentes:** `docs/especificacion_diseno_uart.md` §5–§8, `docs/criterios_diseno.md` §5, `docs/protocolos_prueba.md` Módulo 2, y `eusart1.h`/`eusart1.c` tal como quedaron en T-03.

---

## 1. ¿Cambia `eusart1.c`? No: es un módulo nuevo encima

El protocolo vive en archivos nuevos. `eusart1.c` **no se toca** (y no hay que pulsar *Generate* en MCC). Ya ofrece todo lo que el protocolo necesita [VERIFICADO en `eusart1.h`]:

| Función de `eusart1` | Qué le da al protocolo |
|---|---|
| `EUSART1_Read(&b)` | saca un byte del buffer de RX; `false` si está vacío |
| `EUSART1_get_last_status()` | errores de hardware (`ferr`, `oerr`) del último byte leído |
| `EUSART1_get_overflow()` | `true` si la ISR descartó bytes por buffer lleno (y borra el aviso) |
| `tx_DAT(data, size)` | encola un bloque completo (todo o nada); `false` si no cabe |

```
 main.c  ─────────►  protocol_task()   ← se llama en cada vuelta del while(1)
                      │   parser (máquina de estados)  +  despacho por TYPE
                      │   crc8()  +  frame_send()
                      ▼
                  eusart1.c  (sin cambios: buffers circulares + ISR de RX y TX)
                      ▼
                    RC4 / RC5  ⇄  PC
```

Es la capa de arriba: **la UART mueve bytes; el protocolo los junta en tramas, los verifica y decide qué hacer.** Todo corre en el programa principal, nunca en la ISR.

## 2. Formato de la trama [VERIFICADO: `especificacion_diseno_uart.md` §8.2]

9 bytes fijos, iguales en los dos sentidos, big-endian:

| Byte | Campo | Valor |
|---|---|---|
| 0 | `SYNC1` | `0xA5` |
| 1 | `SYNC2` | `0x5A` |
| 2 | `TYPE` | tipo de trama |
| 3 | `SEQ` | contador 0–255 |
| 4–7 | `PAYLOAD` | 4 bytes, significado según `TYPE` |
| 8 | `CRC8` | sobre los bytes 2 a 7 (seis bytes) |

A 115200 baud una trama dura 781 µs. Como el largo es fijo, el payload puede contener `A5 5A` sin confundir al parser (no se interpreta nada dentro de una trama en curso).

**Tipos de la Fase 1** (los de PWM y datos se agregan en tareas posteriores):

| TYPE | Sentido | Nombre | Payload | Respuesta |
|---|---|---|---|---|
| `0x01` | PC → PIC | PING | 4 bytes libres | PONG |
| `0x81` | PIC → PC | PONG | copia del payload del PING | – |
| `0x7F` | PIC → PC | ERROR | `P0` = código de error, `P1–P3` = 0 | – |

Convención [PROPUESTA]: las peticiones del PC usan `TYPE < 0x80` y las respuestas del PIC usan `petición | 0x80`; ERROR es la excepción (`0x7F`). Las respuestas copian el `SEQ` de la petición.

**Códigos de ERROR:** `1` CRC inválido · `2` tipo desconocido · `3` overflow del buffer de RX · `4` error de hardware (framing/overrun). (`5` y `6` los agrega la especificación del PWM.)

## 3. El CRC-8

Un CRC es un resto de división de polinomios: se alimenta byte a byte y al final queda un byte que cambia si cambia cualquier bit de la entrada.

| Parámetro | Valor |
|---|---|
| Polinomio | `0x07` (x⁸ + x² + x + 1) |
| Valor inicial | `0x00` |
| Reflejo de entrada y de salida | no |
| XOR final | `0x00` |
| Bytes cubiertos | `TYPE, SEQ, P0, P1, P2, P3` (bytes 2 a 7) |
| Valor de comprobación | CRC de los 9 caracteres ASCII `"123456789"` = **`0xF4`** [CÁLCULO] |

Este es el CRC-8 «clásico» (el de SMBus); en Python y en LabVIEW se calcula con las mismas cuatro líneas, lo que sirve para la prueba 2.1.

Algoritmo (bit a bit): `crc ^= byte;` y 8 veces: si el bit 7 es 1, `crc = (crc << 1) ^ 0x07`; si no, `crc = crc << 1`.

**Forma de calcularlo en el PIC** [PROPUESTA]:

| Opción | Costo | Comentario |
|---|---|---|
| **Bit a bit** (la de arriba) | sin tablas; ≈ 50 iteraciones por trama, del orden de 100 µs a 4 MIPS [CÁLCULO, estimado] | simple y fácil de leer línea a línea. **Recomendada** |
| Tabla de 256 bytes en flash | 256 B de flash, ≈ 6 consultas | más rápida, menos didáctica. La flash sobra (28 KB) |

Con cualquiera cabe sobrado en el presupuesto: se calcula en `main`, no en la ISR, y una trama llega cada 781 µs como mínimo.

**Vectores de prueba** [CÁLCULO, con el script de verificación; recalcular en Python antes de usarlos]:

| Trama | Bytes (hex) |
|---|---|
| PING, SEQ 0, payload 00 00 00 00 | `A5 5A 01 00 00 00 00 00 29` |
| PING, SEQ 7, payload 11 22 33 44 | `A5 5A 01 07 11 22 33 44 F9` |
| PONG a ese PING | `A5 5A 81 07 11 22 33 44 15` |
| ERROR código 1, SEQ 0 | `A5 5A 7F 00 01 00 00 00 55` |
| TYPE desconocido `0x55`, SEQ 3 | `A5 5A 55 03 00 00 00 00 C3` |

## 4. Archivos

| Archivo (en `firmware/Micro_Firmware.X/`) | Contenido |
|---|---|
| `crc8.c` / `crc8.h` | `uint8_t crc8(const uint8_t *data, uint8_t len)` |
| `protocol.c` / `protocol.h` | parser, despacho, `frame_send`, `protocol_init`, `protocol_task` |
| `main.c` | **cambia**: llama `protocol_task()` en cada vuelta en lugar del eco |
| `eusart1.c/.h`, `interrupt_manager.c`, `pin_manager.c`, `mcc.c`, `device_config.c` | **no cambian** |

Los archivos nuevos hay que añadirlos al proyecto de MPLAB X (*Source Files / Header Files*); eso actualiza `nbproject/configurations.xml` y el `Makefile`. RAM estimada: buffer de trama 9 B + estado y contadores ≈ 15 B [CÁLCULO].

## 5. Eventos

| ID | Evento | Cómo se detecta | Respuesta |
|---|---|---|---|
| PE1 | Hay un byte por procesar | `EUSART1_Read` devuelve `true` | lo pasa al parser |
| PE2 | Encabezado `A5 5A` visto | estado del parser | empieza a juntar los 7 bytes restantes |
| PE3 | Trama completa (9 bytes) | el contador llega a 9 | calcula el CRC y lo compara con el byte 8 |
| PE4 | CRC correcto | comparación | despacha por `TYPE` |
| PE5 | CRC incorrecto | comparación | encola ERROR(1) y **resincroniza**: descarta el primer byte (`A5`) y vuelve a pasar los otros 8 por el parser, por si dentro había un encabezado verdadero |
| PE6 | `TYPE` desconocido (con CRC bueno) | despacho | encola ERROR(2) |
| PE7 | Overflow del buffer de RX | `EUSART1_get_overflow()` = `true` | encola ERROR(3) y reinicia el parser (se perdieron bytes: la trama en curso ya no es confiable) |
| PE8 | Byte con error de hardware | `EUSART1_get_last_status()` con `ferr` u `oerr` tras leer | encola ERROR(4) y reinicia el parser |
| PE9 | El buffer de TX no tiene espacio para la respuesta | `tx_DAT` devuelve `false` | la respuesta se pierde (no se espera) y se cuenta en `protoTxDropped` |
| PE10 | PING válido | despacho | encola PONG con el mismo `SEQ` y payload |

Bytes basura **sin** `A5 5A` se descartan en silencio: no generan ERROR. No hay temporizador entre bytes [PROPUESTA]: si una trama queda a medias, los bytes de la siguiente la completan, falla el CRC y la resincronización de PE5 encuentra la trama buena. Por eso la prueba 2.6 (trama partida) funciona sin timeout.

## 6. Tareas

| ID | Función | Dónde corre | Descripción |
|---|---|---|---|
| Q1 | `uint8_t crc8(const uint8_t *data, uint8_t len)` | main | calcula el CRC-8 de §3 |
| Q2 | `protocol_init()` | arranque | pone el parser en `ESPERA_A5` y los contadores en 0 |
| Q3 | `protocol_task()` | cada vuelta del `while(1)` | atiende PE7; lee **todos los bytes disponibles** (máx. 32, el tamaño del buffer de RX), revisa PE8 por byte y alimenta el parser |
| Q4 | `parser_feed(uint8_t b)` | interna | máquina de estados `ESPERA_A5 → ESPERA_5A → CUERPO (7 bytes)`; al completar la trama, PE3 a PE6 |
| Q5 | `frame_send(type, seq, payload[4])` | interna | arma los 9 bytes, calcula el CRC y llama a `tx_DAT`; maneja PE9 |
| Q6 | `handle_ping(frame)` | interna | PE10 |

Reglas: sin `while` que espere ni `__delay_ms`; nada de esto se llama desde la ISR; `protocol_task()` y `crc8()` solo desde `main`, para no duplicar código ni compartir estado con la ISR. Comentarios en español, línea por línea, con el formato de `MASTER_PLAN.md`. Los nombres de campos de `eusart1_status_t` (`ferr`, `oerr`) ya están verificados en `eusart1.h`.

## 7. Modo eco para las pruebas de UART

Las pruebas 1.5, 1.6 y 1.7 usan el **eco crudo**, que desaparece si `main` pasa a llamar al protocolo. Propuesta [PROPUESTA]: una constante de compilación `MODO_ECO` en `main.c` (1 = eco, 0 = protocolo). Así se puede repetir el Módulo 1 sin borrar código. Alternativa: borrar el eco al terminar de probar la UART.

## 8. Pruebas

Las del **Módulo 2** de `docs/protocolos_prueba.md` (2.1 a 2.8) tal como están, más estas [PROPUESTA]:

| ID | Qué se verifica | Procedimiento | Criterio |
|---|---|---|---|
| 2.9 | Falso encabezado | enviar `A5 A5 5A` + un PING válido | responde al PING |
| 2.10 | `A5 5A` dentro del payload | PING con payload `A5 5A 00 00` | PONG correcto; el parser no se desincroniza |
| 2.11 | Overflow del RX | enviar > 32 bytes sin dejar que `main` los lea (p. ej. con un punto de parada en el depurador) | ERROR(3) y luego sigue funcionando |
| 2.12 | Error de framing | cambiar el baud del PC a otro valor un instante | ERROR(4), y se recupera al volver a 115200 |
| 2.13 | Vector `"123456789"` | comparar la implementación de Python con `0xF4` | igual (valida la herramienta del PC antes de la 2.1) |

Herramienta de prueba: `herramientas/prueba_protocolo.py` [PROPUESTA], que calcula el CRC por su cuenta (independiente del PIC), envía las tramas y compara las respuestas. La existente `prueba_uart.py` queda para el Módulo 1.

## 9. Decisiones abiertas

| ID | Decisión | Mi recomendación |
|---|---|---|
| PR-1 | CRC-8 bit a bit o con tabla | bit a bit |
| PR-2 | `MODO_ECO` por constante o borrar el eco | constante, hasta cerrar el Módulo 1 |
| PR-3 | `SEQ` de las tramas ERROR | copiar el `SEQ` recibido si hay trama completa; `0` si no hay (overflow, framing) |
| PR-4 | Timeout entre bytes | no; la resincronización por `A5 5A` basta |
| PR-5 | Un ERROR por cada CRC malo o limitar la tasa | uno por CRC malo; con ruido raro no satura |
| PR-6 | Registrar «CRC-8 elegido, XOR descartado» en `criterios_diseno.md` §10 | sí, cuando apruebes (cierra el pendiente 6 de §10; la lista final de tipos sigue abierta) |

## 10. Orden de trabajo propuesto para T-04 (a refinar por Gemini en `MASTER_PLAN.md`)

Pasos revisables uno a uno, como se hizo en T-03:

1. `crc8.c/.h` + prueba contra el vector `0xF4` y las 5 tramas de §3.
2. `frame_send` (armar y enviar una trama): se prueba mandando un PONG fijo al arrancar.
3. Parser (máquina de estados) con PING → PONG.
4. ERROR 1 y 2 y la resincronización de PE5.
5. PE7 y PE8, `MODO_ECO` y `main.c`.

Criterio de aceptación: pruebas 2.1 a 2.13. Compila con XC8 2.40 sin errores nuevos.

## 11. Lo que no verifiqué

- El tiempo real del CRC bit a bit en el chip (es una estimación; se mide con el pin RC2 y el osciloscopio si importa).
- Que XC8 acepte `const uint8_t` en `tx_DAT(const char *, …)` sin avisos: se resuelve con un cast al llamar.
- La prueba 2.12 (error de framing) depende de cómo se provoque desde el PC; puede quedar como opcional.
