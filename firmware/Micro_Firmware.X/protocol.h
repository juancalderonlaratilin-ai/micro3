/* [T-04] protocol.h: declaración del protocolo de tramas de 9 bytes con CRC-8.
 * Formato (docs/especificacion_protocolo.md §2):
 *   byte: 0      1      2      3     4..7     8
 *         A5     5A     TYPE   SEQ   PAYLOAD  CRC8
 * El protocolo va ENCIMA de la UART: eusart1.c mueve bytes; protocol.c los junta en tramas.
 * Todo corre en main; nada se llama desde la ISR. */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#define PROTO_SYNC1      0xA5 /* byte 0: primer byte de encabezado, marca el inicio de trama */
#define PROTO_SYNC2      0x5A /* byte 1: segundo byte de encabezado */
#define PROTO_FRAME_LEN  9    /* una trama mide siempre 9 bytes */
#define PROTO_PAYLOAD_LEN 4   /* el payload (datos útiles) mide siempre 4 bytes */

#define PROTO_TYPE_PING  0x01 /* petición del PC: "¿estás vivo?"; el PIC responde con PONG */
#define PROTO_TYPE_PONG  0x81 /* respuesta del PIC a un PING (TYPE de petición | 0x80) */
#define PROTO_TYPE_ERROR 0x7F /* el PIC avisa de un problema; P0 del payload = código de error */

#define PROTO_ERR_CRC    1    /* código de ERROR: el CRC de la trama no coincide */
#define PROTO_ERR_TYPE   2    /* código de ERROR: el TYPE no se reconoce (con CRC bueno) */
#define PROTO_ERR_OVERFLOW 3  /* código de ERROR: se perdieron bytes (buffer de RX lleno) */
#define PROTO_ERR_HARDWARE 4  /* código de ERROR: byte con error de hardware (framing/overrun) */

/* [T-04] protocol_init
 * Para qué sirve: dejar el protocolo listo al arrancar.
 * Quién la llama: main, una vez, con las interrupciones ya encendidas.
 * Parámetros:     ninguno.   Devuelve: nada. */
void protocol_init(void);

/* [T-04] protocol_task
 * Para qué sirve: leer los bytes que haya en el buffer de RX, juntarlos en tramas y atenderlas
 *                 (PING -> PONG). No espera nada: si no hay bytes, regresa enseguida.
 * Quién la llama: main, en cada vuelta del while(1) (nunca la ISR).
 * Parámetros:     ninguno.   Devuelve: nada. */
void protocol_task(void);

#endif
