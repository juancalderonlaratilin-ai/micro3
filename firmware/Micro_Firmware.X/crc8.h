/* [T-04] crc8.h: declaración del cálculo de CRC-8 que usa el protocolo de tramas.
 * Un .h dice QUÉ funciones existen; el .c (crc8.c) dice CÓMO se hacen. Quien incluya este
 * archivo (protocol.c) puede llamar a crc8() sin ver su código. */

#ifndef CRC8_H
#define CRC8_H

#include <stdint.h>

/* [T-04] crc8
 * Para qué sirve: calcular el CRC-8 (polinomio 0x07, valor inicial 0x00, sin reflejo,
 *                 sin XOR final) de un bloque de bytes. Es el CRC-8 "clásico" (el de SMBus).
 * Quién la llama: main, a través de protocol.c (nunca la ISR).
 * Parámetros:     data - dirección del primer byte a procesar.
 *                 len  - cuántos bytes procesar (en el protocolo son 6: TYPE, SEQ y PAYLOAD).
 * Devuelve:       el CRC de 8 bits. Con "123456789" (9 caracteres ASCII) da 0xF4.
 * Variables:      ninguna global: solo usa variables locales. */
uint8_t crc8(const uint8_t *data, uint8_t len);

#endif
