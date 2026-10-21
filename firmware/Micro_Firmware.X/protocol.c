/* [T-04] protocol.c: protocolo de tramas de 9 bytes con CRC-8 sobre la UART.
 * Paso 5 de 5: arma y envía tramas (frame_send), las recibe con un parser (parser_feed), responde
 * PING -> PONG y avisa con ERROR 1 (CRC malo), ERROR 2 (TYPE desconocido), ERROR 3 (se perdieron
 * bytes por overflow del buffer de RX) y ERROR 4 (byte con error de hardware). */

#include "protocol.h"
#include "crc8.h"
#include "mcc_generated_files/eusart1.h" /* tx_DAT() y EUSART1_Read() */

/* [T-04] Cuenta las tramas que no se pudieron enviar porque el buffer de TX no tenía espacio
 * para los 9 bytes (evento PE9). Es "volatile" y global para verla en el depurador de MPLAB X.
 * shortcut: de 8 bits, da la vuelta tras 255; basta ver si es distinta de 0. */
volatile uint8_t protoTxDropped = 0;

/* [T-04] Estados del parser (la "máquina de estados"): en qué punto de la trama va.
 *   ESPERA_A5: busca el primer byte del encabezado (0xA5). Estado de reposo.
 *   ESPERA_5A: ya vio 0xA5; falta el 0x5A.
 *   CUERPO:    ya vio A5 5A; junta los 7 bytes que faltan (TYPE, SEQ, PAYLOAD y CRC). */
typedef enum { ESPERA_A5, ESPERA_5A, CUERPO } ParserState;

static ParserState parserState;           /* estado actual del parser */
static uint8_t rxFrame[PROTO_FRAME_LEN];  /* la trama que se está juntando, byte a byte */
static uint8_t rxIndex;                   /* en qué posición de rxFrame va el próximo byte */

/* [T-04] frame_send
 * Para qué sirve: armar una trama de 9 bytes, calcularle el CRC y encolarla para enviar.
 * Quién la llama: otras funciones de protocol.c, desde main (nunca la ISR).
 * Parámetros:     type    - tipo de trama (byte 2).
 *                 seq     - número de secuencia (byte 3).
 *                 payload - dirección de los 4 bytes de datos (bytes 4 a 7).
 * Devuelve:       true si la trama quedó encolada; false si no cupo (y se anota en
 *                 protoTxDropped). No espera: si no cabe, la trama se pierde.
 * Variables:      escribe protoTxDropped. */
static bool frame_send(uint8_t type, uint8_t seq, const uint8_t *payload)
{
    uint8_t frame[PROTO_FRAME_LEN]; /* la trama completa, armada aquí antes de enviarla */
    uint8_t i;                      /* índice para copiar los 4 bytes del payload */

    /* Bytes 0 y 1: el encabezado fijo A5 5A. */
    frame[0] = PROTO_SYNC1;
    frame[1] = PROTO_SYNC2;

    /* Bytes 2 y 3: el tipo y el número de secuencia. */
    frame[2] = type;
    frame[3] = seq;

    /* Bytes 4 a 7: se copian los 4 bytes de payload. */
    for (i = 0; i < PROTO_PAYLOAD_LEN; i++)
    {
        frame[4 + i] = payload[i];
    }

    /* Byte 8: el CRC se calcula sobre los bytes 2 a 7 (seis bytes: TYPE, SEQ y payload).
     * &frame[2] es la dirección del byte 2. El encabezado A5 5A NO entra en el CRC. */
    frame[8] = crc8(&frame[2], 6);

    /* Se encolan los 9 bytes de una vez con tx_DAT (todo o nada: nunca sale media trama).
     * tx_DAT pide "const char *", así que se hace un cast desde uint8_t *: son los mismos
     * bytes, solo cambia el nombre del tipo. */
    if (!tx_DAT((const char *)frame, PROTO_FRAME_LEN))
    {
        protoTxDropped++; /* no cupo: no se espera ni se reintenta, solo se cuenta */
        return false;
    }

    return true;
}

/* [T-04] send_error
 * Para qué sirve: enviar una trama ERROR al PC (TYPE 0x7F). El código va en el primer byte del
 *                 payload (P0) y los otros tres bytes van en 0.
 * Quién la llama: process_frame, desde main (nunca la ISR).
 * Parámetros:     code - código de error (1 = CRC inválido, 2 = TYPE desconocido).
 *                 seq  - SEQ de la trama que causó el error (decisión PR-3).
 * Devuelve:       nada.   Variables: ninguna propia (frame_send cuenta si no cupo). */
static void send_error(uint8_t code, uint8_t seq)
{
    uint8_t payload[PROTO_PAYLOAD_LEN]; /* P0 = código; P1 a P3 = 0 */

    payload[0] = code;
    payload[1] = 0;
    payload[2] = 0;
    payload[3] = 0;
    frame_send(PROTO_TYPE_ERROR, seq, payload);
}

/* [T-04] handle_ping
 * Para qué sirve: responder a un PING con un PONG que lleve el mismo SEQ y el mismo payload
 *                 (evento PE10).
 * Quién la llama: process_frame, desde main (nunca la ISR).
 * Parámetros:     frame - la trama PING completa y ya verificada (9 bytes).
 * Devuelve:       nada.   Variables: ninguna propia. */
static void handle_ping(const uint8_t *frame)
{
    /* frame[3] es el SEQ del PING y &frame[4] apunta a su payload: el PONG los copia tal cual,
     * y así el PC sabe a qué petición corresponde la respuesta. */
    frame_send(PROTO_TYPE_PONG, frame[3], &frame[4]);
}

/* [T-04] process_frame
 * Para qué sirve: revisar el CRC de una trama completa (evento PE3) y, si es buena, hacer lo
 *                 que pide su TYPE (despacho).
 * Quién la llama: parser_feed, cuando junta los 9 bytes.
 * Parámetros:     frame - la trama completa (9 bytes).
 * Devuelve:       true si el CRC era MALO (hay que resincronizar); false en cualquier otro
 *                 caso (CRC bueno, tanto si el TYPE se conoce como si no).
 * Variables:      ninguna propia. */
static bool process_frame(const uint8_t *frame)
{
    /* El receptor recalcula el CRC de los bytes 2 a 7 que le llegaron y lo compara con el que
     * venía en el byte 8. Si no coinciden, la trama se dañó en el camino. */
    if (crc8(&frame[2], 6) != frame[8])
    {
        /* Evento PE5: se avisa con ERROR 1 (uno por cada CRC malo, decisión PR-5). Como la
         * trama llegó completa, se copia su SEQ en el ERROR (decisión PR-3). */
        send_error(PROTO_ERR_CRC, frame[3]);
        return true; /* quien llamó debe resincronizar */
    }

    /* CRC correcto (evento PE4): se decide qué hacer según el TYPE (byte 2). */
    if (frame[2] == PROTO_TYPE_PING)
    {
        handle_ping(frame);
    }
    else
    {
        /* Evento PE6: el CRC es bueno pero el PIC no conoce este TYPE: ERROR 2. */
        send_error(PROTO_ERR_TYPE, frame[3]);
    }

    return false; /* el CRC era bueno: no hace falta resincronizar */
}

/* [T-04] parser_feed
 * Para qué sirve: recibir UN byte y avanzar la máquina de estados que junta la trama.
 * Quién la llama: protocol_task, desde main (nunca la ISR).
 * Parámetros:     b - el byte que acaba de llegar por la UART.
 * Devuelve:       true si con este byte se completó una trama con CRC MALO (rxFrame todavía
 *                 la contiene y hay que resincronizar); false en cualquier otro caso.
 * Variables:      parserState, rxFrame y rxIndex (privadas de este archivo). */
static bool parser_feed(uint8_t b)
{
    switch (parserState)
    {
    case ESPERA_A5:
        /* En reposo solo importa el 0xA5. Cualquier otro byte es basura y se descarta en
         * silencio (no genera ERROR). */
        if (b == PROTO_SYNC1)
        {
            rxFrame[0] = b;
            parserState = ESPERA_5A;
        }
        break;

    case ESPERA_5A:
        if (b == PROTO_SYNC2)
        {
            /* Encabezado completo (evento PE2): empieza a juntar el cuerpo en la posición 2. */
            rxFrame[1] = b;
            rxIndex = 2;
            parserState = CUERPO;
        }
        else if (b != PROTO_SYNC1)
        {
            /* No era 5A ni otro A5: el A5 anterior era basura; se vuelve a reposo. Si era otro
             * A5 (caso "A5 A5 5A"), el estado no cambia: ese nuevo A5 puede ser el verdadero. */
            parserState = ESPERA_A5;
        }
        break;

    case CUERPO:
    default:
        /* Se guarda el byte en la siguiente posición de la trama. Dentro del cuerpo NO se
         * interpreta nada, ni siquiera un A5 5A: el largo es fijo, así que un payload con
         * A5 5A no confunde al parser. */
        rxFrame[rxIndex] = b;
        rxIndex++;
        if (rxIndex == PROTO_FRAME_LEN)
        {
            /* Trama completa (evento PE3): se vuelve a reposo ANTES de procesarla, para que
             * el parser quede listo para la siguiente pase lo que pase. */
            parserState = ESPERA_A5;
            return process_frame(rxFrame);
        }
        break;
    }

    return false; /* la trama sigue incompleta o no hubo CRC malo */
}

/* [T-04] resync
 * Para qué sirve: recuperarse de una trama con CRC malo. El primer byte (A5) de esa trama era
 *                 un falso encabezado o la trama estaba dañada; los otros 8 bytes pueden
 *                 contener el inicio de una trama verdadera. Por eso se descarta SOLO el
 *                 primer byte y los otros 8 se vuelven a pasar por el parser (evento PE5).
 * Quién la llama: protocol_task, justo después de que parser_feed devuelva true.
 * Parámetros:     ninguno.   Devuelve: nada.
 * Variables:      lee rxFrame (y el parser lo vuelve a escribir al procesar). */
static void resync(void)
{
    uint8_t pending[PROTO_FRAME_LEN - 1]; /* copia de los bytes 1 a 8 de la trama mala */
    uint8_t i;

    /* Se copian ANTES de volver a alimentar el parser, porque el parser escribe sobre rxFrame
     * y se pisaría los bytes que todavía falta leer. */
    for (i = 0; i < PROTO_FRAME_LEN - 1; i++)
    {
        pending[i] = rxFrame[1 + i];
    }

    /* Se pasan por el parser. El resultado se ignora: 8 bytes no alcanzan para completar una
     * trama nueva (hacen falta 9), así que aquí no puede aparecer otro CRC malo. Si había un
     * encabezado verdadero entre ellos, el parser queda a medias y los siguientes bytes de la
     * UART terminan la trama. */
    for (i = 0; i < PROTO_FRAME_LEN - 1; i++)
    {
        parser_feed(pending[i]);
    }
}

/* [T-04] parser_reset
 * Para qué sirve: dejar el parser en reposo, esperando el primer 0xA5, y tirar la trama a medias.
 * Quién la llama: protocol_init (al arrancar) y protocol_task (cuando se pierden bytes o llega
 *                 un byte con error de hardware: la trama en curso ya no es confiable).
 * Parámetros:     ninguno.   Devuelve: nada.
 * Variables:      escribe parserState y rxIndex. */
static void parser_reset(void)
{
    parserState = ESPERA_A5;
    rxIndex = 0;
}

void protocol_init(void)
{
    /* Estado de arranque: en reposo, esperando el primer 0xA5. */
    parser_reset();
}

void protocol_task(void)
{
    uint8_t b;     /* el byte que se acaba de leer del buffer de RX */
    uint8_t count; /* cuántos bytes se han leído en esta vuelta */
    eusart1_status_t status; /* errores de hardware (ferr, oerr) del byte recién leído */

    /* Evento PE7: EUSART1_get_overflow devuelve true si la ISR de RX descartó bytes porque el
     * buffer de RX estaba lleno, y borra el aviso. Con bytes perdidos la trama en curso ya no es
     * confiable: se avisa con ERROR 3 (SEQ 0: no hay una trama completa de donde copiarlo, PR-3)
     * y el parser empieza de cero. */
    if (EUSART1_get_overflow())
    {
        send_error(PROTO_ERR_OVERFLOW, 0);
        parser_reset();
    }

    /* Se leen TODOS los bytes disponibles, pero con tope de 32 (el tamaño del buffer de RX)
     * para que una vuelta de main nunca se alargue de más. EUSART1_Read devuelve false al
     * instante cuando el buffer está vacío, así que este "for" NO espera nada. */
    for (count = 0; count < 32; count++)
    {
        if (!EUSART1_Read(&b))
        {
            break; /* no hay más bytes: se sale y main sigue con lo suyo */
        }

        /* Evento PE8: EUSART1_get_last_status da los errores de hardware del byte que acaba de
         * leer EUSART1_Read. ferr = framing error (bit de stop mal), oerr = overrun error (el
         * hardware perdió un byte). Con cualquiera de los dos el byte no es confiable: se avisa
         * con ERROR 4 (SEQ 0), el parser empieza de cero y ese byte no se usa. */
        status = EUSART1_get_last_status();
        if (status.ferr || status.oerr)
        {
            send_error(PROTO_ERR_HARDWARE, 0);
            parser_reset();
            continue; /* pasa al siguiente byte sin alimentar el parser con este */
        }

        if (parser_feed(b))
        {
            resync(); /* la trama que acaba de completarse tenía el CRC malo */
        }
    }
}
