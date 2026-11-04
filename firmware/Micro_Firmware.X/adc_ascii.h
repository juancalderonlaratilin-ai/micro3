/* [T-09] adc_ascii.h: envío de I y Q al PC en texto (ASCII), una línea por par:
 *   "I:2048,Q:2051\r\n"
 * Es el único formato de envío del proyecto (decisión del 2026-11-04). "nombre:valor"
 * separados por coma es el formato que entiende el Serial Plotter del Arduino IDE; herramientas/interfaz_micro3.py lee las mismas líneas.
 * Los pares los junta la ISR del ADC (adc_iq.c): aquí solo se leen y se convierten a texto,
 * así no hay una segunda ISR ni hace falta tocar interrupt_manager.c. */

#ifndef ADC_ASCII_H
#define ADC_ASCII_H

#include <stdint.h>

/* [T-09] Cada cuántos pares (I, Q) se manda una línea. El ADC muestrea a 1 kHz, así que
 * 2 pares = 2 ms = 500 líneas por segundo (fs = 500 Hz en el PC: alcanza para medir SINAD a
 * 5 Hz y 20 Hz con sus armónicos 2 a 5). Va de 1 a 255 (el contador es de 8 bits); adc_ascii.c
 * lo revisa al compilar.
 * Uso de la línea: "I:4095,Q:4095\r\n" son 15 bytes = 150 bits = 1,31 ms a 114 286 baudios,
 * así que caben cada 2 ms (66 % de la línea). Con 1 (cada par) NO caben: 1,31 ms > 1 ms y se
 * perderían líneas. Con 50 serían 20 líneas/s: solo sirve para ver señales de menos de 10 Hz. */
#define ADC_PARES_POR_LINEA 2

/* [T-09] Líneas descartadas porque no cupieron en el buffer de TX (tx_string devolvió false).
 * "volatile" y global para verla en el depurador de MPLAB X. Debe quedar en 0. */
extern volatile uint16_t adcLineasPerdidas;

/* [T-09] adc_ascii_task
 * Para qué sirve: cada ADC_PARES_POR_LINEA pares mandar por la UART una línea con el último I
 *                 y Q, en decimal. No espera nada: si no hay par nuevo, regresa enseguida.
 * Quién la llama: main, en cada vuelta del while(1) (nunca la ISR). Antes, main debe haber
 *                 llamado a adc_iq_init().
 * Parámetros:     ninguno.   Devuelve: nada. */
void adc_ascii_task(void);

#endif
