/* [T-06] pwm.h: un PWM por canal que reproduce la señal muestreada (Micro 3).
 * Cada par nuevo del ADC (1 kHz) se copia al duty de un PWM:
 *   I (RC0) -> PWM6 en RA2 (pata 11)      Q (RC1) -> PWM7 en RA4 (pata 3)
 * Con un filtro pasa bajos en cada salida, el promedio del PWM vuelve a ser la señal
 * analógica: V = VDD * duty / 1024. Como el ADC también usa VDD de referencia, la salida
 * filtrada repite la entrada (ganancia 1), con pasos de 3,7 V / 1024 = 3,6 mV.
 * Base de tiempo: Timer2 a Fosc/4, T2PR = 255, sin prescaler -> 64 us = 15,625 kHz, 10 bits.
 * Detalle y cuentas: docs/especificacion_pwm.md. */

#ifndef PWM_H
#define PWM_H

#include <stdint.h>
#include <stdbool.h>

/* [T-06] Número de canal para pwm_set y pwm_get. */
#define PWM_CANAL_I 0   /* PWM6, RA2 */
#define PWM_CANAL_Q 1   /* PWM7, RA4 */

/* [T-06] Duty máximo de 10 bits: 4 * (T2PR + 1) - 1 = 1023 (99,9 %). 0 = siempre en bajo. */
#define PWM_DUTY_MAX 1023u

/* [T-06] Duty al arrancar, antes del primer par del ADC: 512 = 50 % = 1,85 V (la mitad de la
 * escala, igual que el reposo de I y Q). */
#define PWM_DUTY_INICIAL 512u

/* [T-06] pwm_init
 * Para qué sirve: configurar Timer2, PWM6 y PWM7 y sacar sus salidas por RA2 y RA4.
 * Quién la llama: main, una vez, después de SYSTEM_Initialize() (que deja RA2 y RA4 como
 *                 entradas) y antes de encender las interrupciones.
 * Parámetros:     ninguno.   Devuelve: nada. */
void pwm_init(void);

/* [T-06] pwm_set
 * Para qué sirve: cambiar el duty de un canal. El hardware lo toma al empezar el siguiente
 *                 periodo de 64 us (doble buffer): no hay que esperar nada.
 * Parámetros:     canal - PWM_CANAL_I o PWM_CANAL_Q.   duty - de 0 a PWM_DUTY_MAX.
 * Devuelve:       true si lo aplicó; false si el canal o el duty no son válidos (no toca nada). */
bool pwm_set(uint8_t canal, uint16_t duty);

/* [T-06] pwm_get
 * Para qué sirve: leer el último duty aplicado a un canal (copia en RAM, no lee el hardware).
 * Devuelve:       el duty (0 a 1023), o 0xFFFF si el canal no existe. */
uint16_t pwm_get(uint8_t canal);

/* [T-06] pwm_desde_adc
 * Para qué sirve: pasar un par del ADC (12 bits, 0 a 4095) a los dos PWM (10 bits). Redondea
 *                 (código + 2) / 4, así el error es de medio paso como máximo.
 * Quién la llama: main, cada vez que adc_iq_get entrega un par nuevo (1000 veces por segundo).
 * Parámetros:     i, q - códigos del ADC.   Devuelve: nada. */
void pwm_desde_adc(uint16_t i, uint16_t q);

#endif
