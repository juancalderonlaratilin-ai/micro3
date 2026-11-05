/* [T-06] pwm.c: PWM6 (I) y PWM7 (Q) con Timer2, para reconstruir con un filtro pasa bajos las
 * señales que mide el ADC. Ver pwm.h.
 * Nombres de registros: PWM6CON/PWM6DCH/PWM6DCL, T2CON, T2CLKCON, T2PR, T2HLT y RxyPPS, como
 * los usa el código de MCC de esta familia. VERIFICAR en pic16f18426.h (XC8 2.40): si un nombre
 * no existe, la compilación falla en esa línea. */

#include <xc.h>     /* nombres de los registros del PIC */
#include "pwm.h"

/* [T-06] Copia en RAM del duty de cada canal (índice PWM_CANAL_I o PWM_CANAL_Q). Solo la
 * escribe el programa principal; la ISR no la toca. */
static uint16_t pwmDuty[2];

void pwm_init(void)
{
    /* 1. Apagar los dos PWM y el Timer2 mientras se configura.
     *    PWMxCON = control del PWM x: bit 7 EN (Enable, encendido), bit 4 POL (polaridad).
     *    T2CON = control del Timer2: bit 7 ON, bits 6:4 prescaler, bits 3:0 postscaler. */
    PWM6CON = 0x00;
    PWM7CON = 0x00;
    T2CON = 0x00;

    /* 2. Pines RA2 y RA4 como salidas digitales en 0. SYSTEM_Initialize() los dejó como
     *    entradas analógicas (TRISA = 0x3F, ANSELA = 0x37).
     *    LATAx = latch de salida: en 0 antes de volverlo salida, para no sacar un pulso falso.
     *    ANSAx = Analog Select (bit de ANSELA): 0 = digital.
     *    TRISAx = dirección (bit de TRISA): 0 = salida. */
    LATAbits.LATA2 = 0;
    LATAbits.LATA4 = 0;
    ANSELAbits.ANSA2 = 0;
    ANSELAbits.ANSA4 = 0;
    TRISAbits.TRISA2 = 0;
    TRISAbits.TRISA4 = 0;

    /* 3. RxyPPS = qué periférico maneja el pin (PPS). 0x0D = PWM6OUT, 0x0E = PWM7OUT
     *    (DS Tabla 16-3, notas §5). Desde aquí el pin lo mueve el PWM, no LATA. */
    RA2PPS = 0x0D;
    RA4PPS = 0x0E;

    /* 4. Timer2, la base de tiempo de los dos PWM (tras el reset CCPTMRS1 ya asigna PWM6 y PWM7
     *    al Timer2: no hace falta escribirlo, especificacion_pwm.md §3).
     *    T2CLKCON = 0x01: reloj Fosc/4 = 4 MHz. El valor de reset (0) es el pin externo y el
     *               timer no contaría (notas §9.3).
     *    T2HLT = 0x00: modo libre, cuenta mientras ON = 1 (es el valor de reset).
     *    T2PR = 255: periodo de 256 cuentas = 256 * 4 * 62,5 ns = 64 us -> 15,625 kHz, y
     *          4 * 256 = 1024 pasos de duty = 10 bits.
     *    T2TMR = 0: la cuenta empieza desde cero. */
    T2CLKCON = 0x01;
    T2HLT = 0x00;
    T2PR = 255;
    T2TMR = 0;

    /* 5. Duty inicial de los dos canales (la mitad de la escala) antes de encender. */
    pwm_set(PWM_CANAL_I, PWM_DUTY_INICIAL);
    pwm_set(PWM_CANAL_Q, PWM_DUTY_INICIAL);

    /* 6. Encender el Timer2: T2CON = 0x80 -> ON = 1, prescaler 1:1, postscaler 1:1. */
    T2CON = 0x80;

    /* 7. Encender los PWM: EN = 1, POL = 0 (salida normal: en alto durante el duty). */
    PWM6CON = 0x80;
    PWM7CON = 0x80;
}

bool pwm_set(uint8_t canal, uint16_t duty)
{
    uint8_t alto;   /* bits 9:2 del duty -> PWMxDCH */
    uint8_t bajo;   /* bits 1:0 del duty, puestos en los bits 7:6 -> PWMxDCL */

    /* Se revisa primero: con un valor inválido no se toca el hardware. */
    if (canal > PWM_CANAL_Q || duty > PWM_DUTY_MAX)
    {
        return false;
    }

    /* El duty de 10 bits se parte en dos registros (DS §24.4):
     *   PWMxDCH = los 8 bits altos (duty >> 2)
     *   PWMxDCL = los 2 bits bajos, en las posiciones 7 y 6 ((duty & 3) << 6). */
    alto = (uint8_t)(duty >> 2);
    bajo = (uint8_t)((duty & 0x03u) << 6);

    /* Los dos registros tienen doble buffer: el hardware los copia al empezar el siguiente
     * periodo (cuando T2TMR = T2PR). Si esa copia cae justo entre las dos escrituras, un solo
     * periodo de 64 us sale con un valor mezclado (error de 3 pasos como máximo); el filtro lo
     * borra. Por eso no hace falta apagar interrupciones ni esperar. */
    if (canal == PWM_CANAL_I)
    {
        PWM6DCH = alto;
        PWM6DCL = bajo;
    }
    else
    {
        PWM7DCH = alto;
        PWM7DCL = bajo;
    }

    pwmDuty[canal] = duty;   /* se guarda para pwm_get */
    return true;
}

uint16_t pwm_get(uint8_t canal)
{
    if (canal > PWM_CANAL_Q)
    {
        return 0xFFFFu;   /* canal que no existe */
    }
    return pwmDuty[canal];
}

/* [T-06] a_duty
 * Para qué sirve: pasar un código del ADC de 12 bits a un duty de 10 bits redondeando:
 *                 (código + 2) / 4, con la división hecha como desplazamiento (>> 2).
 *                 4095 daría (4097 >> 2) = 1024, que no existe: se recorta a 1023. */
static uint16_t a_duty(uint16_t codigo)
{
    uint16_t d = (uint16_t)((codigo + 2u) >> 2);
    if (d > PWM_DUTY_MAX)
    {
        d = PWM_DUTY_MAX;
    }
    return d;
}

void pwm_desde_adc(uint16_t i, uint16_t q)
{
    /* Los dos valores son siempre válidos (a_duty los deja en 0 a 1023), así que no hace falta
     * revisar lo que devuelve pwm_set. */
    (void)pwm_set(PWM_CANAL_I, a_duty(i));
    (void)pwm_set(PWM_CANAL_Q, a_duty(q));
}
