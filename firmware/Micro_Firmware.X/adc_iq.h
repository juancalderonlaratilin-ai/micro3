/* [T-09] adc_iq.h: lectura de los canales I (RC0) y Q (RC1) con el ADC, a 1 kHz.
 * El Timer0 dispara por hardware la conversión de I cada 1 ms; la interrupción del ADC
 * (adc_iq_isr) guarda I, arranca Q, guarda Q y acumula el par. Cada ADC_N_PROMEDIO pares deja
 * las sumas listas para que main las lea con adc_iq_get y las promedie.
 * Detalle: docs/instrucciones_claude_code_adc.md §3.3. */

#ifndef ADC_IQ_H
#define ADC_IQ_H

#include <stdint.h>
#include <stdbool.h>

/* [T-09] Cuántos pares (I, Q) se suman antes de entregar un promedio a main (decisión D-1).
 * Con 5: muestreo a 1 kHz y 200 promedios por segundo (200 tramas/s, ~16 % de la línea).
 * Para el histograma del profesor (prueba 4.3) se compila un rato con 1. */
#define ADC_N_PROMEDIO 5

/* [T-09] El acumulador es de 16 bits: 16 × 4095 = 65 520 cabe, 17 × 4095 = 69 615 no.
 * Con 0 el promedio dividiría por cero. #error detiene la compilación con este mensaje. */
#if (ADC_N_PROMEDIO < 1) || (ADC_N_PROMEDIO > 16)
#error "ADC_N_PROMEDIO debe estar entre 1 y 16 (acumulador de 16 bits)"
#endif

/* [T-09] Pin de prueba (decisión D-4): 1 = RC2 sube mientras corre adc_iq_isr(), para medir
 * con el osciloscopio el período de muestreo y la duración de la ISR (pruebas 4.5 y 4.13).
 * 0 = RC2 no se toca. */
#define ADC_PIN_PRUEBA 1

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
 * Para qué sirve: atender el fin de cada conversión (I y luego Q) y acumular el par.
 * Quién la llama: SOLO interrupt_manager.c, cuando ADIE = 1 y ADIF = 1 (C-11).
 * Parámetros:     ninguno.   Devuelve: nada. */
void adc_iq_isr(void);

/* [T-09] adc_iq_get
 * Para qué sirve: entregar a main el último par promediado, si hay uno nuevo.
 * Quién la llama: main, en cada vuelta del while(1) (nunca la ISR).
 * Parámetros:     i, q - dónde dejar el promedio de cada canal (0 a 4095).
 * Devuelve:       true si había un par nuevo (y lo deja en *i y *q); false si no (no toca
 *                 *i ni *q). No espera nada. */
bool adc_iq_get(uint16_t *i, uint16_t *q);

#endif
