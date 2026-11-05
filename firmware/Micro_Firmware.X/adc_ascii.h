/* [T-09] adc_ascii.h: envío de I y Q al PC en texto (ASCII), una línea por par:
 *   "2048,2051\r\n"   (primero I, después Q, separados por una coma)
 * Es el único formato de envío del proyecto (decisión del 2026-11-04). Números separados
 * por coma es un formato que entiende el Serial Plotter del Arduino IDE (dos curvas);
 * herramientas/interfaz_micro3.py lee las mismas líneas.
 * Los pares los junta la ISR del ADC (adc_iq.c): aquí solo se leen y se convierten a texto,
 * así no hay una segunda ISR ni hace falta tocar interrupt_manager.c. */

#ifndef ADC_ASCII_H
#define ADC_ASCII_H

#include <stdint.h>

/* [T-09] Cada cuántos pares (I, Q) se manda una línea. El ADC muestrea a 1 kHz, así que
 * 2 pares = 2 ms = 500 líneas por segundo (fs = 500 Hz en el PC: alcanza para medir SINAD a
 * 5 Hz y 20 Hz con sus armónicos 2 a 5). Va de 1 a 255 (el contador es de 8 bits); adc_ascii.c
 * lo revisa al compilar.
 * Uso de la línea: "4095,4095\r\n" son 11 bytes = 110 bits = 0,96 ms a 114 286 baudios,
 * así que cada 2 ms ocupan el 48 % de la línea. Con 1 (cada par) caben apenas: 96 % de la
 * línea, sin margen. Con 50 serían 20 líneas/s: solo sirve para ver señales de menos de 10 Hz. */
#define ADC_PARES_POR_LINEA 2

/* [T-09] Líneas descartadas porque no cupieron en el buffer de TX (tx_string devolvió false).
 * "volatile" y global para verla en el depurador de MPLAB X. Debe quedar en 0. */
extern volatile uint16_t adcLineasPerdidas;

/* [T-09] adc_ascii_par
 * Para qué sirve: contar el par y, cada ADC_PARES_POR_LINEA pares, mandar por la UART una
 *                 línea "I,Q" en decimal. No espera nada.
 * Quién la llama: main, cada vez que adc_iq_get entrega un par nuevo (nunca la ISR).
 * Parámetros:     i, q - el par del ADC (0 a 4095).   Devuelve: nada. */
void adc_ascii_par(uint16_t i, uint16_t q);

#endif
