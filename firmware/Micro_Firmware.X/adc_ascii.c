/* [T-09] adc_ascii.c: manda "xxxx,xxxx\r\n" (I, coma, Q) cada ADC_PARES_POR_LINEA pares (2 = 500 líneas
 * por segundo). main le pasa cada par que entrega adc_iq_get(): una sola muestra de I y una de
 * Q, sin promediar. */

#include "adc_ascii.h"
#include "mcc_generated_files/eusart1.h"  /* tx_string() */

/* [T-09] Revisión al compilar (#error detiene la compilación con el mensaje):
 * "pares" es de 8 bits, así que ADC_PARES_POR_LINEA debe ir de 1 a 255. */
#if (ADC_PARES_POR_LINEA < 1) || (ADC_PARES_POR_LINEA > 255)
#error "ADC_PARES_POR_LINEA debe ir de 1 a 255"
#endif

volatile uint16_t adcLineasPerdidas = 0; /* declarado en adc_ascii.h */

static uint8_t pares = 0; /* cuántos pares han llegado desde la última línea */

/* [T-09] a_decimal
 * Para qué sirve: escribir un número de 0 a 9999 en decimal, sin ceros a la izquierda, sin
 *                 printf ni división: para cada cifra se resta 1000, 100, 10 o 1 las veces que
 *                 quepa, y esas veces son la cifra.
 * Quién la llama: adc_ascii_par, desde main.
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

void adc_ascii_par(uint16_t i, uint16_t q)
{
    char linea[12]; /* "4095,4095\r\n" son 11 caracteres + el '\0' final */
    uint8_t n;      /* cuántos caracteres lleva la línea */

    /* Se cuentan los pares hasta juntar ADC_PARES_POR_LINEA; solo entonces se manda una
     * línea con el último par. */
    pares++;
    if (pares < ADC_PARES_POR_LINEA)
    {
        return;
    }
    pares = 0;

    /* Se arma I + "," + Q + "\r\n" carácter por carácter: primero I, después Q. */
    n = 0;
    n += a_decimal(i, &linea[n]);
    linea[n++] = ','; /* separa I de Q */
    n += a_decimal(q, &linea[n]);
    linea[n++] = '\r'; /* retorno de carro: la terminal vuelve al inicio de la línea */
    linea[n++] = '\n'; /* salto de línea */
    linea[n] = '\0';   /* fin de cadena, para que tx_string sepa dónde termina */

    /* tx_string solo encola los bytes y regresa; si no caben (devuelve false), no espera ni
     * reintenta: la línea se descarta y solo se cuenta. A 115200 baudios 11 bytes tardan
     * ~0,96 ms y hay 2 ms entre líneas (con ADC_PARES_POR_LINEA = 2), así que no debería
     * pasar; adcLineasPerdidas debe quedar en 0. */
    if (!tx_string(linea))
    {
        adcLineasPerdidas++;
    }
}
