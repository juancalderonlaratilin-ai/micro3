/* [PRUEBA ADC] adc_prueba.h: prueba TEMPORAL para ver I y Q en el Serial Plotter del Arduino IDE
 * (por ejemplo, una señal de un generador en RC0). No es T-09 completa ni usa el protocolo de
 * tramas: manda líneas de texto "I:2048,Q:2051\r\n" ("nombre:valor" separados por coma es el
 * formato que entiende el Serial Plotter). Se activa con MODO_PRUEBA_ADC = 1 en main.c.
 * Los pares los junta la ISR de T-09 (adc_iq.c): aquí solo se leen y se convierten a texto,
 * así no hay una segunda ISR del ADC ni hace falta tocar interrupt_manager.c. */

#ifndef ADC_PRUEBA_H
#define ADC_PRUEBA_H

#include <stdint.h>

/* [PRUEBA ADC] Cada cuántos pares (I, Q) se manda una línea. El ADC muestrea a 1 kHz, así que
 * 2 pares = 2 ms = 500 líneas por segundo (fs = 500 Hz, para medir SINAD a 5 Hz y 20 Hz con
 * herramientas/interfaz_micro3.py). Va de 1 a 255 (el contador es de 8 bits); adc_prueba.c lo
 * revisa al compilar.
 * Uso de la línea: una línea "I:4095,Q:4095\r\n" son 15 bytes = 1,31 ms a 115200 baudios, así
 * que caben cada 2 ms (66 % de la línea). Con 1 (cada par) NO caben: se perderían líneas.
 * Antes era 50 (20 líneas/s): solo servía para ver señales de menos de 10 Hz. */
#define PRUEBA_PARES_POR_LINEA 2

/* [PRUEBA ADC] Líneas descartadas porque no cupieron en el buffer de TX (tx_string devolvió
 * false). "volatile" y global para verla en el depurador. Debe quedar en 0. */
extern volatile uint16_t adcPruebaLineasPerdidas;

/* [PRUEBA ADC] adc_prueba_task
 * Para qué sirve: cada PRUEBA_PARES_POR_LINEA pares mandar por la UART una línea con el último
 *                 I y Q, en decimal. No espera nada: si no hay par nuevo, regresa enseguida.
 * Quién la llama: main, en cada vuelta del while(1) (nunca la ISR). Antes, main debe haber
 *                 llamado a adc_iq_init().
 * Parámetros:     ninguno.   Devuelve: nada. */
void adc_prueba_task(void);

#endif
