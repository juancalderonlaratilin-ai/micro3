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

/* [T-09] Tipos del ADC (decisión D-2; docs/instrucciones_claude_code_adc.md §3.4). */
#define PROTO_TYPE_ADC_STREAM 0x20 /* PC -> PIC: P0 = 0 detener, distinto de 0 iniciar el envío */
#define PROTO_TYPE_ADC_ACK    0xA0 /* PIC -> PC: respuesta a ADC_STREAM (0x20 | 0x80); P0 = estado */
#define PROTO_TYPE_DATA_IQ    0xA1 /* PIC -> PC, sin pedirla: I en P0:P1, Q en P2:P3 (big-endian) */

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

/* [T-09] protocol_stream_on
 * Para qué sirve: decir si el PC pidió el envío de DATA_IQ (con ADC_STREAM, P0 distinto de 0).
 *                 Al arrancar está apagado.
 * Quién la llama: main (nunca la ISR).
 * Parámetros:     ninguno.   Devuelve: true si el envío está encendido. */
bool protocol_stream_on(void);

/* [T-09] protocol_send_iq
 * Para qué sirve: enviar una trama DATA_IQ con un par (I, Q). Lleva su propio SEQ, que sube de
 *                 0 a 255 y vuelve a 0.
 * Quién la llama: main (nunca la ISR).
 * Parámetros:     i, q - los valores de cada canal (0 a 4095).
 * Devuelve:       true si la trama quedó encolada; false si no cupo en el buffer de TX. */
bool protocol_send_iq(uint16_t i, uint16_t q);

#endif
