/* [T-04] crc8.c: cálculo del CRC-8 bit a bit (sin tabla). Decisión PR-1 de
 * docs/especificacion_protocolo.md: es la forma más corta y fácil de leer línea a línea.
 *
 * Idea: un CRC es el RESTO de dividir el mensaje (como un número enorme) entre un polinomio.
 * Aquí el polinomio es 0x07 (x^8 + x^2 + x + 1). Al final queda 1 byte que cambia si cambia
 * cualquier bit del mensaje, así que sirve para detectar tramas dañadas. */

#include "crc8.h"

#define CRC8_POLY 0x07 /* polinomio; el bit x^8 está implícito, por eso solo caben x^2+x+1 = 0b111 */

uint8_t crc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0x00; /* valor inicial del CRC (la especificación fija 0x00) */
    uint8_t i;          /* cuántos bytes del bloque ya se procesaron */
    uint8_t bit;        /* cuántos de los 8 bits del byte actual ya se procesaron */

    for (i = 0; i < len; i++)
    {
        /* Se mezcla el byte nuevo con el CRC acumulado usando XOR (^): donde los bits son
         * iguales da 0 y donde son distintos da 1. */
        crc ^= data[i];

        /* Se procesa el byte bit por bit, 8 veces, empezando por el bit 7 (el más alto). */
        for (bit = 0; bit < 8; bit++)
        {
            if (crc & 0x80) /* 0x80 = 0b10000000: ¿el bit 7 vale 1? */
            {
                /* Sí: se desplaza todo 1 lugar a la izquierda (el bit 7 se sale) y se aplica
                 * el polinomio con XOR. Es un "paso de la división". El cast a uint8_t evita
                 * que C trabaje con más de 8 bits en la cuenta intermedia. */
                crc = (uint8_t)((crc << 1) ^ CRC8_POLY);
            }
            else
            {
                /* No: solo se desplaza 1 lugar a la izquierda; no hay nada que restar. */
                crc = (uint8_t)(crc << 1);
            }
        }
    }

    return crc;
}
