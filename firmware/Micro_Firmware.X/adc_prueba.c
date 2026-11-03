/* [PRUEBA ADC] adc_prueba.c: prueba TEMPORAL; manda "I:xxxx,Q:xxxx\r\n" cada
 * PRUEBA_PARES_POR_LINEA pares (50 = 20 líneas por segundo) para el Serial Plotter.
 * Usa los pares que entrega adc_iq_get() (T-09): una sola muestra de I y una de Q, sin
 * promediar. La copia con ADIE apagado un instante la hace adc_iq_get(). */

#include "adc_prueba.h"
#include "adc_iq.h"                       /* adc_iq_get() */
#include "mcc_generated_files/eusart1.h"  /* tx_string() */

/* [PRUEBA ADC] Revisión al compilar (#error detiene la compilación con el mensaje):
 * "pares" es de 8 bits, así que PRUEBA_PARES_POR_LINEA debe ir de 1 a 255. */
#if (PRUEBA_PARES_POR_LINEA < 1) || (PRUEBA_PARES_POR_LINEA > 255)
#error "PRUEBA_PARES_POR_LINEA debe ir de 1 a 255"
#endif

volatile uint16_t adcPruebaLineasPerdidas = 0; /* declarado en adc_prueba.h */

static uint8_t pares = 0; /* cuántos pares han llegado desde la última línea */

/* [PRUEBA ADC] a_decimal
 * Para qué sirve: escribir un número de 0 a 9999 en decimal, sin ceros a la izquierda, sin
 *                 printf ni división: para cada cifra se resta 1000, 100, 10 o 1 las veces que
 *                 quepa, y esas veces son la cifra.
 * Quién la llama: adc_prueba_task, desde main.
 * Parámetros:     v   - el número (I o Q van de 0 a 4095, así que siempre cabe).
 *                 dst - dónde escribir los caracteres (hasta 4; NO se agrega '\0').
 * Devuelve:       cuántos caracteres escribió (1 a 4). */
static uint8_t a_decimal(uint16_t v, char *dst)
{
    static const uint16_t potencia[4] = {1000, 100, 10, 1}; /* valor de cada posición */
    uint8_t k;            /* qué posición se está calculando (millares, centenas, ...) */
    uint8_t cifra;        /* la cifra de esa posición, de 0 a 9 */
    uint8_t n = 0;        /* cuántos caracteres van escritos */

    for (k = 0; k < 4; k++)
    {
        /* Se resta la potencia mientras quepa: a lo más 9 vueltas, no es una espera. */
        for (cifra = 0; v >= potencia[k]; cifra++)
        {
            v -= potencia[k];
        }

        /* Se omiten los ceros de la izquierda; la última cifra se escribe siempre (para el 0). */
        if (cifra != 0 || n != 0 || k == 3)
        {
            dst[n] = (char)('0' + cifra); /* '0' + 7 = '7' en ASCII */
            n++;
        }
    }

    return n;
}

void adc_prueba_task(void)
{
    uint16_t i;     /* muestra del canal I (0 a 4095) */
    uint16_t q;     /* muestra del canal Q */
    char linea[16]; /* "I:4095,Q:4095\r\n" son 15 caracteres + el '\0' final */
    uint8_t n;      /* cuántos caracteres lleva la línea */

    /* Si la ISR no tiene un par nuevo, se regresa enseguida (no se espera). */
    if (!adc_iq_get(&i, &q))
    {
        return;
    }

    /* Se cuentan los pares hasta juntar PRUEBA_PARES_POR_LINEA; solo entonces se manda una
     * línea con el último par. */
    pares++;
    if (pares < PRUEBA_PARES_POR_LINEA)
    {
        return;
    }
    pares = 0;

    /* Se arma "I:" + I + ",Q:" + Q + "\r\n" carácter por carácter. */
    n = 0;
    linea[n++] = 'I';
    linea[n++] = ':';
    n += a_decimal(i, &linea[n]);
    linea[n++] = ',';
    linea[n++] = 'Q';
    linea[n++] = ':';
    n += a_decimal(q, &linea[n]);
    linea[n++] = '\r'; /* retorno de carro: la terminal vuelve al inicio de la línea */
    linea[n++] = '\n'; /* salto de línea */
    linea[n] = '\0';   /* fin de cadena, para que tx_string sepa dónde termina */

    /* tx_string solo encola los bytes y regresa; si no caben (devuelve false), no espera ni
     * reintenta: la línea se descarta y solo se cuenta. A 115200 baudios 15 bytes tardan
     * ~1,3 ms y hay 50 ms entre líneas, así que no debería pasar. */
    if (!tx_string(linea))
    {
        adcPruebaLineasPerdidas++;
    }
}
