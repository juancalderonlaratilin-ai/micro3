/* [T-09] adc_iq.h: lectura de los canales I (RC0) y Q (RC1) con el ADC, a 1 kHz.
 * El Timer0 dispara por hardware la conversión de I cada 1 ms; la interrupción del ADC
 * (adc_iq_isr) guarda I, arranca Q y guarda Q. Cada par (una sola muestra de I y una de Q, sin
 * promediar) queda listo para que main lo lea con adc_iq_get: 1000 pares por segundo.
 * Detalle: docs/instrucciones_claude_code_adc.md §3.3. */

#ifndef ADC_IQ_H
#define ADC_IQ_H

#include <stdint.h>
#include <stdbool.h>

/* [T-09] Sin promedio (cambio del 2026-11-03, reemplaza la decisión D-1 de promediar 5):
 * cada par que entrega adc_iq_get es UNA sola conversión de I y UNA de Q. Con muestreo a
 * 1 kHz salen 1000 pares por segundo (adc_ascii.c manda 1 de cada ADC_PARES_POR_LINEA). */

/* [T-09] Pin de prueba RC2 (pata 8, el pin libre; decisión D-4). Qué sale por RC2:
 *   0 = nada: RC2 no se toca.
 *   1 = pulso de la ISR: RC2 sube mientras corre adc_iq_isr() (dos pulsos por ms: fin de I y
 *       fin de Q). Sirve para medir la duración de la ISR (pruebas 4.5 y 4.13).
 *   2 = salida del Timer0 (TMR0_out, por PPS): onda cuadrada de 500 Hz. TMR0_out cambia de
 *       nivel cada vez que el Timer0 llega a su período (1 ms), así que CADA FLANCO (de subida
 *       y de bajada) es un disparo del ADC. Es la señal del hardware, sin software en medio:
 *       sirve para medir el período de muestreo y su jitter (prueba 4.5).
 * Ojo, en la PCB nueva RC2 es EN_AZ_PIC: deja JP_AZEN en 1-2 (reposo) y mide en la pata 3 de
 * JP_AZEN (o en la pata 8 del PIC). Con JP_AZEN en 2-3 esta señal conmutaría U4. */
#define ADC_PIN_PRUEBA 2

#if (ADC_PIN_PRUEBA < 0) || (ADC_PIN_PRUEBA > 2)
#error "ADC_PIN_PRUEBA debe ser 0, 1 o 2"
#endif

/* [T-09] Pares que se perdieron porque main no alcanzó a leer el anterior antes de que la ISR
 * tuviera listo el siguiente. Es "volatile" (lo cambia la ISR) y global para verlo en el
 * depurador de MPLAB X. Debe quedar en 0 (prueba 4.14). */
extern volatile uint16_t adcParesPerdidos;

/* [T-09] adc_iq_init
 * Para qué sirve: dejar el ADC leyendo el canal I (RC0) en vez de RA0, y arrancar el Timer0
 *                 desde cero (corrige C-12).
 * Quién la llama: main, una vez, después de SYSTEM_Initialize() y ANTES de encender PEIE/GIE.
 * Parámetros:     ninguno.   Devuelve: nada. */
void adc_iq_init(void);

/* [T-09] adc_iq_isr
 * Para qué sirve: atender el fin de cada conversión (I y luego Q) y dejar listo el par.
 * Quién la llama: SOLO interrupt_manager.c, cuando ADIE = 1 y ADIF = 1 (C-11).
 * Parámetros:     ninguno.   Devuelve: nada. */
void adc_iq_isr(void);

/* [T-09] adc_iq_get
 * Para qué sirve: entregar a main el último par leído (una muestra de cada canal), si hay
 *                 uno nuevo.
 * Quién la llama: main, en cada vuelta del while(1) (nunca la ISR).
 * Parámetros:     i, q - dónde dejar la muestra de cada canal (0 a 4095).
 * Devuelve:       true si había un par nuevo (y lo deja en *i y *q); false si no (no toca
 *                 *i ni *q). No espera nada. */
bool adc_iq_get(uint16_t *i, uint16_t *q);

#endif
