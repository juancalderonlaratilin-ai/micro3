/* [T-09] adc_iq.c: muestreo de I (RC0) y Q (RC1) a 1 kHz, disparado por el Timer0.
 * Flujo de cada milisegundo (docs/instrucciones_claude_code_adc.md §3.2):
 *   1. El Timer0 llega a su período (1 ms) y arranca, por hardware, la conversión del canal
 *      que esté en ADPCH (I). No hace falta software para el disparo: no hay jitter.
 *   2. Al terminar I, el ADC sube ADIF y entra adc_iq_isr(): guarda I, cambia a Q y arranca Q.
 *   3. Al terminar Q, entra otra vez adc_iq_isr(): guarda Q, deja ADPCH en I para el próximo
 *      tic y suma el par. Cada ADC_N_PROMEDIO pares deja las sumas listas para main.
 * Nombres de registros y bits verificados en pic16f18426.h (XC8 2.35). */

#include <xc.h>                         /* nombres de los registros del PIC (ADPCH, ADRESH, ...) */
#include "adc_iq.h"
#include "mcc_generated_files/adcc.h"  /* CANAL_I y CANAL_Q: los números de canal que puso MCC */

/* [T-09] En qué conversión va el par actual. Los nombres no pueden ser CANAL_I/CANAL_Q porque
 * ya los usa MCC en adcc.h para los números de canal del ADC.
 *   ESTADO_I: el ADC está convirtiendo (o va a convertir) I. Estado de reposo entre tics.
 *   ESTADO_Q: I ya se guardó y el ADC está convirtiendo Q. */
typedef enum { ESTADO_I, ESTADO_Q } AdcEstado;

volatile uint16_t adcParesPerdidos = 0; /* declarado en adc_iq.h */

/* Variables que solo usa la ISR (no hace falta protegerlas, pero son volatile por claridad:
 * viven entre una interrupción y la siguiente). */
static volatile AdcEstado adcEstado;  /* qué canal se está convirtiendo */
static volatile uint16_t adcI;        /* último I leído, esperando a su Q */
static volatile uint16_t sumaI;       /* suma de los I del grupo en curso */
static volatile uint16_t sumaQ;       /* suma de los Q del grupo en curso */
static volatile uint8_t  cuentaPares; /* cuántos pares lleva el grupo en curso */

/* Variables compartidas con main: la ISR las escribe y adc_iq_get las lee con ADIE apagado. */
static volatile uint16_t salidaI;     /* suma de I del último grupo completo */
static volatile uint16_t salidaQ;     /* suma de Q del último grupo completo */
static volatile bool     listo;       /* true = hay un grupo completo que main no ha leído */

void adc_iq_init(void)
{
    /* 1. T0EN = Timer0 Enable (bit de T0CON0). TMR0_Initialize() ya lo encendió dentro de
     *    SYSTEM_Initialize(): se apaga para que no lleguen disparos mientras se configura. */
    T0CON0bits.T0EN = 0;

    /* 2. ADPCH = ADC Positive Channel (el canal que se va a convertir). ADCC_Initialize() lo dejó
     *    en 0x00 = ANA0 = RA0, que es ICSPDAT del programador SNAP (C-12). Se elige I (ANC0, RC0)
     *    con el nombre que puso MCC en adcc.h, no con el número a mano. */
    ADPCH = CANAL_I;

    /* 3. ADIF = ADC Interrupt Flag (bit de PIR1): la bandera que sube el ADC al terminar. Puede
     *    haber quedado arriba por una conversión de RA0 hecha antes de esta función: se borra
     *    para que la primera interrupción sea de verdad la del canal I. */
    PIR1bits.ADIF = 0;

    /* Estado inicial: esperando I, grupo vacío, nada listo y contadores en cero. */
    adcEstado = ESTADO_I;
    adcI = 0;
    sumaI = 0;
    sumaQ = 0;
    cuentaPares = 0;
    salidaI = 0;
    salidaQ = 0;
    listo = false;
    adcParesPerdidos = 0;

#if ADC_PIN_PRUEBA
    /* 4. RC2 como salida digital en 0 (pin de prueba, D-4). PIN_MANAGER_Initialize() lo dejó
     *    como entrada analógica (ANSELC = 0x1F, TRISC = 0x2F).
     *    LATC2 = latch de salida de RC2: se pone en 0 ANTES de volverlo salida, para que no
     *            salga un pulso falso.
     *    ANSC2 = Analog Select de RC2 (bit de ANSELC): 0 = digital.
     *    TRISC2 = dirección de RC2 (bit de TRISC): 0 = salida. */
    LATCbits.LATC2 = 0;
    ANSELCbits.ANSC2 = 0;
    TRISCbits.TRISC2 = 0;
#endif

    /* 5. TMR0L = cuenta del Timer0 (en modo 8 bits va de 0 a TMR0H = 249). Se pone en 0 para que
     *    el primer disparo llegue 1 ms completo después, y se vuelve a encender el timer. */
    TMR0L = 0;
    T0CON0bits.T0EN = 1;
}

void adc_iq_isr(void)
{
    uint16_t resultado; /* el número que acaba de convertir el ADC (0 a 4095) */

#if ADC_PIN_PRUEBA
    LATCbits.LATC2 = 1; /* RC2 sube: empieza la ISR (se mide con el osciloscopio) */
#endif

    /* Se baja ADIF primero: si no, al salir de la interrupción volvería a entrar sin fin. */
    PIR1bits.ADIF = 0;

    /* ADRESH/ADRESL = ADC Result High/Low: el resultado de 12 bits partido en dos registros
     * de 8 (justificado a la derecha: ADRESH tiene los 4 bits altos). Se juntan como MCC. */
    resultado = (uint16_t)((ADRESH << 8) + ADRESL);

    if (adcEstado == ESTADO_I)
    {
        /* Terminó I (la arrancó el Timer0). Se guarda y se arranca Q enseguida. */
        adcI = resultado;
        ADPCH = CANAL_Q; /* el próximo canal es Q (ANC1, RC1) */
        /* ADGO = ADC Go (bit de ADCON0): 1 = empezar. Como ADACQ > 0, el hardware hace primero
         * la adquisición (10 µs) y después la conversión: no hay que esperar por software. */
        ADCON0bits.ADGO = 1;
        adcEstado = ESTADO_Q;
    }
    else
    {
        /* Terminó Q. Se deja ADPCH en I para que el próximo disparo del Timer0 convierta I. */
        ADPCH = CANAL_I;
        adcEstado = ESTADO_I;

        /* Se suma el par al grupo en curso. 16 × 4095 cabe en 16 bits (ver #error en adc_iq.h). */
        sumaI += adcI;
        sumaQ += resultado;
        cuentaPares++;

        if (cuentaPares >= ADC_N_PROMEDIO)
        {
            /* Grupo completo. Si main todavía no leyó el anterior, ese se pierde: se cuenta. */
            if (listo)
            {
                adcParesPerdidos++;
            }

            /* Se pasan las sumas a la salida y se avisa a main. La división del promedio la
             * hace main (adc_iq_get): en la ISR no se divide, para que sea corta. */
            salidaI = sumaI;
            salidaQ = sumaQ;
            listo = true;

            /* Se empieza un grupo nuevo. */
            sumaI = 0;
            sumaQ = 0;
            cuentaPares = 0;
        }
    }

#if ADC_PIN_PRUEBA
    LATCbits.LATC2 = 0; /* RC2 baja: termina la ISR */
#endif
}

bool adc_iq_get(uint16_t *i, uint16_t *q)
{
    uint16_t si; /* copia de la suma de I */
    uint16_t sq; /* copia de la suma de Q */

    /* Nada nuevo: se regresa enseguida, sin esperar. Leer un bool es una sola instrucción,
     * así que no hace falta apagar la interrupción para esto. */
    if (!listo)
    {
        return false;
    }

    /* Las sumas son de 16 bits y el PIC las lee en dos pasos de 8. Si la ISR las cambiara entre
     * los dos pasos, se leería medio valor viejo y medio nuevo. Por eso se apaga SOLO ADIE
     * (ADC Interrupt Enable, bit de PIE1) mientras se copian: GIE sigue encendido y la UART se
     * sigue atendiendo. Si el ADC termina justo ahora, ADIF queda arriba y la ISR corre apenas
     * se vuelve a encender ADIE: no se pierde nada. */
    PIE1bits.ADIE = 0;
    si = salidaI;
    sq = salidaQ;
    listo = false;
    PIE1bits.ADIE = 1;

    /* Promedio con redondeo al entero más cercano: (suma + N/2) / N. Se hace aquí, en main,
     * porque la división es lenta en el PIC16 y no debe ir en la ISR. */
    *i = (uint16_t)((si + (ADC_N_PROMEDIO / 2)) / ADC_N_PROMEDIO);
    *q = (uint16_t)((sq + (ADC_N_PROMEDIO / 2)) / ADC_N_PROMEDIO);

    return true;
}
