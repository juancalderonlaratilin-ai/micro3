/**
  EUSART1 Generated Driver File

  @Company
    Microchip Technology Inc.

  @File Name
    eusart1.c

  @Summary
    This is the generated driver implementation file for the EUSART1 driver using PIC10 / PIC12 / PIC16 / PIC18 MCUs

  @Description
    This source file provides APIs for EUSART1.
    Generation Information :
        Product Revision  :  PIC10 / PIC12 / PIC16 / PIC18 MCUs - 1.81.8
        Device            :  PIC16F18426
        Driver Version    :  2.1.1
    The generated drivers are tested against the following:
        Compiler          :  XC8 2.36 and above
        MPLAB 	          :  MPLAB X 6.00
*/

/*
    (c) 2018 Microchip Technology Inc. and its subsidiaries. 
    
    Subject to your compliance with these terms, you may use Microchip software and any 
    derivatives exclusively with Microchip products. It is your responsibility to comply with third party 
    license terms applicable to your use of third party software (including open source software) that 
    may accompany Microchip software.
    
    THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER 
    EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY 
    IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS 
    FOR A PARTICULAR PURPOSE.
    
    IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, 
    INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND 
    WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP 
    HAS BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO 
    THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL 
    CLAIMS IN ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT 
    OF FEES, IF ANY, THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS 
    SOFTWARE.
*/

/**
  Section: Included Files
*/
#include "eusart1.h"
#include <string.h> // [C-08] strlen(), que usa tx_string para contar los caracteres de la cadena

/**
  Section: Macro Declarations
*/

#define EUSART1_TX_BUFFER_SIZE 64
#define EUSART1_RX_BUFFER_SIZE 32

/* [C-09] Máscaras para dar la vuelta al índice de un buffer circular.
 * Los tamaños son potencia de 2 (64 y 32), así que SIZE-1 es un número con todos
 * sus bits bajos en 1 (63 = 0b00111111, 31 = 0b00011111).
 * Entonces  (índice + 1) & MASK  vuelve solo a 0 al pasar del final:
 *     63 + 1 = 64   y   64 & 63 = 0.
 * Es una sola operación. Con "% SIZE" el PIC haría una división por software,
 * y esto se ejecuta dentro de la interrupción, donde cada microsegundo cuenta. */
#define EUSART1_TX_MASK (EUSART1_TX_BUFFER_SIZE - 1)
#define EUSART1_RX_MASK (EUSART1_RX_BUFFER_SIZE - 1)

/* Si alguien cambia un tamaño por uno que NO sea potencia de 2, la máscara dejaría
 * de funcionar sin avisar. "SIZE & (SIZE-1)" vale 0 solo si SIZE es potencia de 2;
 * si no, la compilación se detiene aquí. */
#if (EUSART1_TX_BUFFER_SIZE & EUSART1_TX_MASK) || (EUSART1_RX_BUFFER_SIZE & EUSART1_RX_MASK)
#error "Los buffers de la EUSART1 deben tener un tamano potencia de 2"
#endif

/**
  Section: Global Variables
*/

/* [C-09] BUFFER CIRCULAR CON CABEZA Y COLA (sin contador)
 *
 * Un buffer circular es un arreglo que se usa "en rueda": al llegar al final se
 * vuelve a la casilla 0. Ejemplo dibujado con 8 casillas (los nuestros tienen 64 y 32):
 *
 *   índice:    0     1     2     3     4     5     6     7
 *            +-----+-----+-----+-----+-----+-----+-----+-----+
 *   buffer:  |     |     | dat | dat | dat |     |     |     |
 *            +-----+-----+-----+-----+-----+-----+-----+-----+
 *                           ^                 ^
 *                          tail              head
 *
 *   head (cabeza): casilla donde se guardará el PRÓXIMO byte.
 *   tail (cola):   casilla del byte MÁS ANTIGUO que todavía no se ha sacado.
 *
 *   Guardar un byte:  buffer[head] = byte;   y después head avanza 1 (con vuelta).
 *   Sacar un byte:    byte = buffer[tail];   y después tail avanza 1 (con vuelta).
 *
 *   VACÍO:  head == tail
 *   LLENO:  ((head + 1) & MASK) == tail
 *
 * Se deja siempre UNA casilla sin usar. Así "lleno" y "vacío" se distinguen mirando
 * solo head y tail, sin un contador aparte. Por eso la capacidad útil es N - 1:
 * 63 bytes en TX y 31 bytes en RX.
 *
 * REGLA CLAVE: cada índice lo mueve UN SOLO lado. El otro lado solo lo lee.
 *
 *   Buffer | mueve head (cabeza)    | mueve tail (cola)
 *   -------+------------------------+--------------------------------
 *   TX     | main (al escribir)     | ISR de TX (al enviar por la UART)
 *   RX     | ISR de RX (al recibir) | main (al leer)
 *
 *   (ISR = Interrupt Service Routine: la función que el hardware ejecuta cuando
 *    se levanta una bandera de interrupción.)
 *
 * Por qué ya no hace falta un contador compartido (RxCount / TxBufferRemaining):
 * con contador, main y la ISR modificaban la MISMA variable, y MCC tenía que apagar
 * RC1IE/TX1IE un instante para que no se pisaran. Aquí cada uno escribe su propio
 * índice y solo lee el del otro. Los índices son uint8_t: un byte se lee o se
 * escribe en una sola instrucción del PIC, así que la ISR nunca deja un índice a medias.
 *
 * ORDEN al guardar: PRIMERO se escribe el byte en el arreglo y DESPUÉS se avanza
 * el índice. Si fuera al revés, el otro lado vería el índice adelantado y leería
 * una casilla que todavía no tiene el dato.
 *
 * "volatile" le dice al compilador que otra parte del programa (la ISR) puede
 * cambiar la variable en cualquier momento. Sin él, el compilador podría guardar
 * el valor en un registro y no volver a leerlo dentro de un ciclo de main. */

// ---- Buffer de transmisión (TX): main escribe, la ISR de TX envía ----
volatile uint8_t eusart1TxHead = 0; // [C-09] cabeza de TX: la mueve main (EUSART1_Write y tx_DAT)
volatile uint8_t eusart1TxTail = 0; // [C-09] cola de TX: la mueve la ISR de TX (EUSART1_Transmit_ISR)
volatile uint8_t eusart1TxBuffer[EUSART1_TX_BUFFER_SIZE]; // bytes esperando salir por la UART

// ---- Buffer de recepción (RX): la ISR de RX escribe, main lee ----
volatile uint8_t eusart1RxHead = 0; // [C-09] cabeza de RX: la mueve la ISR de RX (EUSART1_RxDataHandler)
volatile uint8_t eusart1RxTail = 0; // [C-09] cola de RX: la mueve main (EUSART1_Read)
volatile uint8_t eusart1RxBuffer[EUSART1_RX_BUFFER_SIZE]; // bytes recibidos que main aún no lee

// Estado de error de cada byte recibido (framing/overrun). Misma posición que eusart1RxBuffer.
volatile eusart1_status_t eusart1RxStatusBuffer[EUSART1_RX_BUFFER_SIZE];
// Estado del último byte que main sacó con EUSART1_Read.
volatile eusart1_status_t eusart1RxLastError;

/* [C-09] Aviso de "se perdió un byte": vale 1 si la ISR de RX descartó un byte porque
 * el buffer de RX estaba lleno. La ISR de RX lo sube a 1; main lo lee y lo borra con
 * EUSART1_get_overflow(). */
volatile uint8_t eusart1RxOverflow = 0;

/**
  Section: EUSART1 APIs
*/

/* [C-06] Aquí MCC declaraba dos punteros a función, EUSART1_TxDefaultInterruptHandler y
 * EUSART1_RxDefaultInterruptHandler. Se quitaron: interrupt_manager.c ahora llama
 * directo a EUSART1_Transmit_ISR() y EUSART1_Receive_ISR(), sin salto indirecto. */

void (*EUSART1_FramingErrorHandler)(void);
void (*EUSART1_OverrunErrorHandler)(void);
void (*EUSART1_ErrorHandler)(void);

void EUSART1_DefaultFramingErrorHandler(void);
void EUSART1_DefaultOverrunErrorHandler(void);
void EUSART1_DefaultErrorHandler(void);

void EUSART1_Initialize(void)
{
    // deshabilita las instrucciones antes de hacer un cambio de estado
    PIE3bits.RC1IE = 0;  // enable de interrupcion de recepcion
    PIE3bits.TX1IE = 0; // enable de interrupcion de transmision
    // [C-06] Aquí MCC llamaba a EUSART1_SetRxInterruptHandler / SetTxInterruptHandler para
    // guardar las ISR en los punteros. Ya no hay punteros: no se necesita nada aquí.

    // ABDOVF no_overflow; SCKP Non-Inverted; BRG16 16bit_generator; WUE disabled; ABDEN disabled; 
    BAUD1CON = 0x08;

    // SPEN enabled; RX9 8-bit; CREN enabled; ADDEN disabled; SREN disabled; 
    RC1STA = 0x90; // status de Recepcion

    // TX9 8-bit; TX9D 0; SENDB sync_break_complete; TXEN enabled; SYNC asynchronous; BRGH hi_speed; CSRC slave; 
    TX1STA = 0x24; // status de Transmision

    // SP1BRGL 34; 
    SP1BRGL = 0x22; // configuracion de bay rate

    // SP1BRGH 0; 
    SP1BRGH = 0x00;


    EUSART1_SetFramingErrorHandler(EUSART1_DefaultFramingErrorHandler);
    EUSART1_SetOverrunErrorHandler(EUSART1_DefaultOverrunErrorHandler);
    EUSART1_SetErrorHandler(EUSART1_DefaultErrorHandler);

    eusart1RxLastError.status = 0;

    // inicio de variables para funcionamiento del buffer circular
    // [C-09] Con head == tail el buffer está vacío: los cuatro índices en 0 dejan TX y RX
    // vacíos. Ya no existen TxBufferRemaining ni RxCount, así que no se inicializan.
    eusart1TxHead = 0;
    eusart1TxTail = 0;

    eusart1RxHead = 0;
    eusart1RxTail = 0;
    eusart1RxOverflow = 0; // [C-09] todavía no se ha perdido ningún byte

    // enable receive interrupt
    PIE3bits.RC1IE = 1;
}

/**
  [C-09] EUSART1_get_overflow
  Para qué sirve: saber si la ISR de RX descartó algún byte porque el buffer de RX
                  estaba lleno, y dejar la bandera lista para la próxima vez.
  Quién la llama: main.
  Parámetros:     ninguno.
  Devuelve:       true si hubo al menos un byte descartado desde la última llamada;
                  false si no.
  Variables:      lee y borra eusart1RxOverflow (la sube la ISR de RX).
*/
bool EUSART1_get_overflow(void)
{
    if (0 == eusart1RxOverflow) // nada se perdió: no hay nada que avisar ni que borrar
    {
        return false;
    }
    /* shortcut: si la ISR descarta otro byte justo entre el "if" de arriba y este borrado,
     * ese aviso se pierde (la bandera ya valía 1 y la borramos). En la Fase 1 basta saber
     * que hubo pérdida; si se necesita contar bytes perdidos, cambiar a un contador. */
    eusart1RxOverflow = 0;
    return true;
}

/**
  [C-09] EUSART1_is_tx_ready
  Para qué sirve: saber si el buffer de TX tiene espacio para un byte más. No espera.
  Quién la llama: main.
  Parámetros:     ninguno.
  Devuelve:       true si el buffer de TX NO está lleno; false si está lleno.
  Variables:      solo lee eusart1TxHead y eusart1TxTail (no modifica nada).
*/
bool EUSART1_is_tx_ready(void) // hay espacio en el buffer de envio?
{
    // [C-09] Lleno = la casilla que sigue a la cabeza es la cola. El "& MASK" da la vuelta
    // al llegar al final del arreglo. Si NO coinciden, todavía cabe un byte (true).
    return ((uint8_t)((eusart1TxHead + 1) & EUSART1_TX_MASK) != eusart1TxTail);
}

/**
  [C-09] EUSART1_is_rx_ready
  Para qué sirve: saber si hay al menos un byte recibido esperando en el buffer de RX.
  Quién la llama: main.
  Parámetros:     ninguno.
  Devuelve:       true si hay algún byte por leer; false si el buffer está vacío.
  Variables:      solo lee eusart1RxHead y eusart1RxTail (no modifica nada).
*/
bool EUSART1_is_rx_ready(void) // hay algun dato en el buffer de repecion?
{
    // [C-09] Vacío = cabeza y cola en la misma casilla. Si son distintas, hay datos sin leer.
    // Antes se comparaba un contador (RxCount) contra 0; ahora basta comparar los dos índices.
    return (eusart1RxHead != eusart1RxTail);
}

/**
  [C-10] EUSART1_is_tx_done
  Para qué sirve: saber si TODO lo que se mandó a transmitir ya salió por el cable.
  Quién la llama: main.
  Parámetros:     ninguno.
  Devuelve:       true solo si el buffer de TX está vacío Y el registro de desplazamiento
                  terminó; false en cualquier otro caso.
  Variables:      solo lee eusart1TxHead, eusart1TxTail y el bit TRMT.
*/
bool EUSART1_is_tx_done(void) // ya salio el ultimo bit por cable
{
    // [C-10] Se exigen DOS cosas:
    //  1) eusart1TxHead == eusart1TxTail: no queda ningún byte esperando en el buffer.
    //  2) TX1STAbits.TRMT: bit de estado de TX1STA, "Transmit Shift Register Status".
    //     Vale 1 cuando el registro de desplazamiento (TSR, el que saca los bits uno a uno
    //     por el pin) está vacío. Vale 0 mientras se está enviando un byte.
    // Solo con TRMT no basta: TRMT mira únicamente el TSR. Con bytes todavía en el buffer
    // podría valer 1 un instante entre byte y byte y decir "terminó" sin que sea verdad.
    return (eusart1TxHead == eusart1TxTail) && TX1STAbits.TRMT;
}

eusart1_status_t EUSART1_get_last_status(void){
    return eusart1RxLastError; // el ultimo byte llego con error
}

/**
  [C-07] EUSART1_Read
  Para qué sirve: sacar UN byte del buffer de RX SIN esperar. Si no hay nada, avisa
                  con false y sigue; el programa nunca se queda colgado aquí.
  Quién la llama: main (nunca la ISR).
  Parámetros:     byte - dirección donde se deja el byte leído. Solo se escribe si hay dato.
  Devuelve:       true si había un byte y quedó en *byte; false si el buffer está vacío.
  Variables:      mueve eusart1RxTail (la cola de RX es de main); lee eusart1RxHead (la
                  mueve la ISR de RX); actualiza eusart1RxLastError.
*/
bool EUSART1_Read(uint8_t *byte)
{
    // Se copia la cola a una variable local: así se lee una sola vez y las líneas de
    // abajo usan siempre la misma casilla, aunque la ISR de RX llegue en medio.
    uint8_t tail = eusart1RxTail;

    // [C-07] Vacío = cabeza y cola iguales. Antes aquí había un while(0 == eusart1RxCount)
    // que esperaba para siempre si el PC no mandaba nada. Ahora se devuelve false y listo.
    if (eusart1RxHead == tail)
    {
        return false; // nada que leer: el llamador decide qué hacer (normalmente, seguir)
    }

    // Se guarda el estado de error (framing/overrun) de ESTE byte, como hacía MCC;
    // se consulta después con EUSART1_get_last_status().
    eusart1RxLastError = eusart1RxStatusBuffer[tail];

    // Se entrega el byte más antiguo, el de la cola.
    *byte = eusart1RxBuffer[tail];

    // [C-09] Recién ahora se avanza la cola (con vuelta al final por la máscara): la casilla
    // queda libre para la ISR de RX. Ya no se apaga RC1IE (Receive 1 Interrupt Enable,
    // el interruptor que permite que un byte recibido interrumpa) alrededor de esto, porque
    // no hay contador compartido: la cola solo la mueve main.
    eusart1RxTail = (uint8_t)((tail + 1) & EUSART1_RX_MASK);

    return true;
}

/**
  [C-08] EUSART1_Write
  Para qué sirve: poner UN byte en el buffer de TX SIN esperar. La ISR de TX lo envía
                  por la UART cuando el hardware esté libre.
  Quién la llama: main (nunca la ISR).
  Parámetros:     txData - el byte a enviar.
  Devuelve:       true si el byte quedó encolado; false si el buffer de TX estaba lleno
                  (el byte NO se encola; el llamador decide si reintenta o lo descarta).
  Variables:      mueve eusart1TxHead (la cabeza de TX es de main); lee eusart1TxTail (la
                  mueve la ISR de TX); escribe en eusart1TxBuffer.
*/
bool EUSART1_Write(uint8_t txData)
{
    // La cabeza actual: casilla donde se guardará este byte. Se lee una sola vez.
    uint8_t head = eusart1TxHead;

    // [C-08] Casilla que seguiría a la cabeza, con vuelta al final por la máscara.
    // Se usa "& MASK" y no "% SIZE" porque SIZE es potencia de 2: una sola instrucción
    // en vez de una división por software.
    uint8_t next = (uint8_t)((head + 1) & EUSART1_TX_MASK);

    // Lleno si la casilla siguiente a la cabeza es la cola. Antes aquí había un
    // while(0 == eusart1TxBufferRemaining) que colgaba el programa con el buffer lleno.
    // Leer la cola aquí es seguro: es un byte, y si la ISR de TX la mueve justo ahora,
    // solo aparece más espacio libre, nunca menos.
    if (next == eusart1TxTail)
    {
        return false; // buffer lleno: no se bloquea, se avisa al llamador
    }

    // [C-09] PRIMERO se escribe el byte en la casilla de la cabeza y DESPUÉS se avanza la
    // cabeza. Así la ISR de TX nunca ve la cabeza adelantada con el dato sin escribir.
    eusart1TxBuffer[head] = txData;
    eusart1TxHead = next;

    // PIE3bits.TX1IE = Transmit 1 Interrupt Enable (bit TX1IE del registro PIE3, Peripheral
    // Interrupt Enable 3): interruptor que permite que "TX1REG vacío" (bandera TX1IF)
    // interrumpa. Al encenderlo, si TX1REG está vacío la ISR de TX se dispara sola y manda
    // el primer byte; si ya estaba encendido no pasa nada. Aquí se eliminó la rama de MCC
    // que escribía directo en TX1REG: ahora TODO pasa por el buffer, así el orden se respeta.
    PIE3bits.TX1IE = 1;

    return true;
}

/**
  [C-08] tx_DAT  (firma pedida en clase)
  Para qué sirve: encolar un bloque de "size" bytes en el buffer de TX SIN esperar.
                  Es TODO O NADA: si el bloque completo no cabe, no se encola ni un byte
                  (así nunca sale media trama).
  Quién la llama: main (nunca la ISR), directamente o a través de tx_string.
  Parámetros:     DATA - dirección del primer byte a enviar.
                  size - cuántos bytes enviar.
  Devuelve:       true  si el bloque quedó encolado (o si size es 0: no hay nada que hacer);
                  false si DATA es NULL o si no hay espacio para los size bytes.
  Variables:      mueve eusart1TxHead UNA sola vez, al final; lee eusart1TxTail; escribe
                  en eusart1TxBuffer.
*/
bool tx_DAT(const char *DATA, uint16_t size)
{
    // Se leen la cabeza (propia) y la cola (de la ISR) una sola vez cada una.
    uint8_t head = eusart1TxHead;
    uint8_t tail = eusart1TxTail;
    uint8_t freeSpace; // casillas libres en el buffer de TX
    uint16_t i;        // cuántos bytes se han copiado

    // Un puntero nulo no apunta a ningún dato: se rechaza en vez de leer una dirección inválida.
    if (DATA == NULL)
    {
        return false;
    }

    // Bloque vacío: no hay nada que enviar y tampoco hay error.
    if (size == 0)
    {
        return true;
    }

    // Espacio libre = casillas entre la cabeza y la cola, menos 1 (la que siempre queda sin
    // usar). El "& MASK" corrige el resultado cuando la resta da negativo porque la cabeza
    // ya dio la vuelta: por ejemplo cola 0 y cabeza 0 (vacío): (0 - 0 - 1) & 63 = 63 libres.
    // Leer la cola aquí es seguro: es un byte, y si la ISR de TX la mueve justo ahora, el
    // espacio libre solo aumenta, nunca disminuye.
    freeSpace = (uint8_t)((tail - head - 1) & EUSART1_TX_MASK);

    // TODO O NADA: si el bloque no cabe completo, se rechaza sin escribir nada.
    if (size > freeSpace)
    {
        return false;
    }

    // Se copian los bytes a partir de la cabeza, dando la vuelta con la máscara. Se avanza
    // la variable local "head", NO eusart1TxHead: la ISR de TX no debe ver el bloque a medias.
    for (i = 0; i < size; i++)
    {
        eusart1TxBuffer[head] = (uint8_t)DATA[i];
        head = (uint8_t)((head + 1) & EUSART1_TX_MASK);
    }

    // [C-09] Recién ahora, con todos los bytes ya escritos, se avanza la cabeza real UNA sola
    // vez. Así la ISR de TX ve el bloque completo de golpe.
    eusart1TxHead = head;

    // Se enciende TX1IE (Transmit 1 Interrupt Enable) para que la ISR de TX empiece a
    // sacar los bytes; si ya estaba encendido, no pasa nada.
    PIE3bits.TX1IE = 1;

    return true;
}

/**
  [C-08] tx_string  (firma pedida en clase)
  Para qué sirve: encolar una cadena de texto terminada en '\0' (por ejemplo "Hola\r\n").
  Quién la llama: main (nunca la ISR).
  Parámetros:     data - dirección del primer carácter de la cadena.
  Devuelve:       lo mismo que tx_DAT: true si la cadena completa quedó encolada; false si
                  data es NULL o no cabe.
  Variables:      ninguna propia (todo lo hace tx_DAT).
*/
bool tx_string(const char *data)
{
    // Sin cadena no hay nada que medir: strlen(NULL) leería una dirección inválida.
    if (data == NULL)
    {
        return false;
    }

    // strlen cuenta los caracteres ANTES del '\0' final; el '\0' no se envía (es solo la
    // marca de fin en memoria, no parte del mensaje). Todo lo demás lo hace tx_DAT.
    return tx_DAT(data, (uint16_t)strlen(data));
}


/**
  [C-09] EUSART1_Transmit_ISR
  Para qué sirve: pasar el siguiente byte del buffer de TX al registro de transmisión
                  (TX1REG) cada vez que la UART queda libre; si no hay nada que enviar,
                  apagar el aviso.
  Quién la llama: INTERRUPT_InterruptManager (interrupt_manager.c), SOLO cuando
                  TX1IE y TX1IF valen 1. Nunca desde main.
  Parámetros:     ninguno.
  Devuelve:       nada.
  Variables:      lee eusart1TxHead (la mueve main); mueve eusart1TxTail (la cola de TX es
                  de la ISR); lee eusart1TxBuffer.
*/
void EUSART1_Transmit_ISR(void)
{
    // Se lee la cola una sola vez. TX1REG = Transmit 1 Register: el registro de la UART 1
    // donde se deja el byte que se va a enviar. Escribirlo inicia la transmisión.
    uint8_t tail = eusart1TxTail;

    // [C-09] Hay algo que enviar si la cabeza (la mueve main) no alcanzó a la cola.
    // Antes se comparaba un contador (TxBufferRemaining); ahora basta comparar los índices.
    if (eusart1TxHead != tail)
    {
        // Primero se copia el byte a TX1REG y DESPUÉS se avanza la cola: mientras no se
        // avance, esa casilla sigue "ocupada" y main no puede pisarla.
        TX1REG = eusart1TxBuffer[tail];
        eusart1TxTail = (uint8_t)((tail + 1) & EUSART1_TX_MASK);
    }
    else
    {
        // Nada que enviar: se apaga TX1IE (Transmit 1 Interrupt Enable).
        // POR QUÉ: la bandera TX1IF (Transmit 1 Interrupt Flag) vale 1 siempre que TX1REG
        // esté vacío, aunque TX1IE valga 0. Con TX1IE encendido y el buffer vacío, TX1IF
        // seguiría en 1 y esta ISR se repetiría sin parar, sin dejar tiempo a main.
        // EUSART1_Write / tx_DAT lo vuelven a encender cuando hay datos nuevos.
        PIE3bits.TX1IE = 0;
    }
}

void EUSART1_Receive_ISR(void)
{
    
    eusart1RxStatusBuffer[eusart1RxHead].status = 0;

    if(RC1STAbits.FERR){
        eusart1RxStatusBuffer[eusart1RxHead].ferr = 1;
        EUSART1_FramingErrorHandler();
    }

    if(RC1STAbits.OERR){
        eusart1RxStatusBuffer[eusart1RxHead].oerr = 1;
        EUSART1_OverrunErrorHandler();
    }
    
    if(eusart1RxStatusBuffer[eusart1RxHead].status){
        EUSART1_ErrorHandler();
    } else {
        EUSART1_RxDataHandler();
    }
}

/**
  [C-09] EUSART1_RxDataHandler
  Para qué sirve: sacar el byte recibido de la UART y guardarlo en el buffer de RX; si el
                  buffer está lleno, descartar el byte y avisar con eusart1RxOverflow.
  Quién la llama: EUSART1_Receive_ISR, y EUSART1_DefaultErrorHandler cuando el byte llegó
                  con error. Siempre dentro de la interrupción; nunca desde main.
  Parámetros:     ninguno.
  Devuelve:       nada.
  Variables:      mueve eusart1RxHead (la cabeza de RX es de la ISR); lee eusart1RxTail
                  (la mueve main); escribe eusart1RxBuffer y eusart1RxOverflow.
*/
void EUSART1_RxDataHandler(void){
    // RC1REG = Receive 1 Register: registro de la UART 1 del que se saca el byte recibido.
    // Se lee SIEMPRE, haya o no sitio en el buffer: leerlo quita el byte de la FIFO del
    // hardware y baja la bandera RC1IF (Receive 1 Interrupt Flag). Si no se leyera,
    // RC1IF seguiría en 1 y esta ISR se repetiría sin parar.
    uint8_t data = RC1REG;

    // Se leen la cabeza (propia) y la casilla siguiente, con vuelta al final por la máscara.
    uint8_t head = eusart1RxHead;
    uint8_t next = (uint8_t)((head + 1) & EUSART1_RX_MASK);

    // [C-09] Lleno = la casilla siguiente a la cabeza es la cola (main aún no leyó esos datos).
    if (next == eusart1RxTail)
    {
        // No se pisa un dato sin leer: el byte nuevo se descarta y se deja constancia.
        // main lo consulta con EUSART1_get_overflow().
        eusart1RxOverflow = 1;
    }
    else
    {
        // PRIMERO se guarda el byte y DESPUÉS se avanza la cabeza, para que main nunca vea
        // la cabeza adelantada con la casilla sin escribir.
        eusart1RxBuffer[head] = data;
        eusart1RxHead = next;
    }
}

void EUSART1_DefaultFramingErrorHandler(void){}

void EUSART1_DefaultOverrunErrorHandler(void){
    // EUSART1 error - restart

    RC1STAbits.CREN = 0;
    RC1STAbits.CREN = 1;

}

void EUSART1_DefaultErrorHandler(void){
    EUSART1_RxDataHandler();
}

void EUSART1_SetFramingErrorHandler(void (* interruptHandler)(void)){
    EUSART1_FramingErrorHandler = interruptHandler;
}

void EUSART1_SetOverrunErrorHandler(void (* interruptHandler)(void)){
    EUSART1_OverrunErrorHandler = interruptHandler;
}

void EUSART1_SetErrorHandler(void (* interruptHandler)(void)){
    EUSART1_ErrorHandler = interruptHandler;
}

/**
  End of File
*/
