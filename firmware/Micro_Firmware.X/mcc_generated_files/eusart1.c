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

#include "eusart1.h"
#include <string.h>

#define EUSART1_TX_BUFFER_SIZE 64
#define EUSART1_RX_BUFFER_SIZE 32

#define EUSART1_TX_MASK (EUSART1_TX_BUFFER_SIZE - 1)
#define EUSART1_RX_MASK (EUSART1_RX_BUFFER_SIZE - 1)

#if (EUSART1_TX_BUFFER_SIZE & EUSART1_TX_MASK) || (EUSART1_RX_BUFFER_SIZE & EUSART1_RX_MASK)
#error "Los buffers de la EUSART1 deben tener un tamano potencia de 2"
#endif

volatile uint8_t eusart1TxHead = 0;
volatile uint8_t eusart1TxTail = 0;
volatile uint8_t eusart1TxBuffer[EUSART1_TX_BUFFER_SIZE];

volatile uint8_t eusart1RxHead = 0;
volatile uint8_t eusart1RxTail = 0;
volatile uint8_t eusart1RxBuffer[EUSART1_RX_BUFFER_SIZE];

volatile eusart1_status_t eusart1RxStatusBuffer[EUSART1_RX_BUFFER_SIZE];
volatile eusart1_status_t eusart1RxLastError;

volatile uint8_t eusart1RxOverflow = 0;

void (*EUSART1_FramingErrorHandler)(void);
void (*EUSART1_OverrunErrorHandler)(void);
void (*EUSART1_ErrorHandler)(void);

void EUSART1_DefaultFramingErrorHandler(void);
void EUSART1_DefaultOverrunErrorHandler(void);
void EUSART1_DefaultErrorHandler(void);

// Configura la UART (115200 8N1), vacia los buffers y enciende la interrupcion de RX.
void EUSART1_Initialize(void)
{
    PIE3bits.RC1IE = 0;
    PIE3bits.TX1IE = 0;

    BAUD1CON = 0x08;

    RC1STA = 0x90;

    TX1STA = 0x24;

    SP1BRGL = 0x22;

    SP1BRGH = 0x00;


    EUSART1_SetFramingErrorHandler(EUSART1_DefaultFramingErrorHandler);
    EUSART1_SetOverrunErrorHandler(EUSART1_DefaultOverrunErrorHandler);
    EUSART1_SetErrorHandler(EUSART1_DefaultErrorHandler);

    eusart1RxLastError.status = 0;

    eusart1TxHead = 0;
    eusart1TxTail = 0;

    eusart1RxHead = 0;
    eusart1RxTail = 0;
    eusart1RxOverflow = 0;

    PIE3bits.RC1IE = 1;
}

// Devuelve true si se descarto algun byte de RX por buffer lleno, y borra el aviso.
bool EUSART1_get_overflow(void)
{
    if (0 == eusart1RxOverflow)
    {
        return false;
    }
    eusart1RxOverflow = 0;
    return true;
}

// true si el buffer de TX tiene espacio para un byte mas.
bool EUSART1_is_tx_ready(void)
{
    return ((uint8_t)((eusart1TxHead + 1) & EUSART1_TX_MASK) != eusart1TxTail);
}

// true si hay algun byte recibido sin leer.
bool EUSART1_is_rx_ready(void)
{
    return (eusart1RxHead != eusart1RxTail);
}

// true si el buffer de TX esta vacio y el ultimo bit ya salio por el cable.
bool EUSART1_is_tx_done(void)
{
    return (eusart1TxHead == eusart1TxTail) && TX1STAbits.TRMT;
}

// Estado de error (framing/overrun) del ultimo byte leido.
eusart1_status_t EUSART1_get_last_status(void){
    return eusart1RxLastError;
}

// Saca un byte del buffer de RX sin esperar; false si esta vacio.
bool EUSART1_Read(uint8_t *byte)
{
    uint8_t tail = eusart1RxTail;

    if (eusart1RxHead == tail)
    {
        return false;
    }

    eusart1RxLastError = eusart1RxStatusBuffer[tail];

    *byte = eusart1RxBuffer[tail];

    eusart1RxTail = (uint8_t)((tail + 1) & EUSART1_RX_MASK);

    return true;
}

// Encola un byte en el buffer de TX sin esperar; false si esta lleno.
bool EUSART1_Write(uint8_t txData)
{
    uint8_t head = eusart1TxHead;

    uint8_t next = (uint8_t)((head + 1) & EUSART1_TX_MASK);

    if (next == eusart1TxTail)
    {
        return false;
    }

    eusart1TxBuffer[head] = txData;
    eusart1TxHead = next;

    PIE3bits.TX1IE = 1;

    return true;
}

// Encola un bloque de size bytes, todo o nada; false si no cabe o DATA es NULL.
bool tx_DAT(const char *DATA, uint16_t size)
{
    uint8_t head = eusart1TxHead;
    uint8_t tail = eusart1TxTail;
    uint8_t freeSpace;
    uint16_t i;

    if (DATA == NULL)
    {
        return false;
    }

    if (size == 0)
    {
        return true;
    }

    freeSpace = (uint8_t)((tail - head - 1) & EUSART1_TX_MASK);

    if (size > freeSpace)
    {
        return false;
    }

    for (i = 0; i < size; i++)
    {
        eusart1TxBuffer[head] = (uint8_t)DATA[i];
        head = (uint8_t)((head + 1) & EUSART1_TX_MASK);
    }

    eusart1TxHead = head;

    PIE3bits.TX1IE = 1;

    return true;
}

// Encola una cadena terminada en '\0' (sin el '\0'); usa tx_DAT.
bool tx_string(const char *data)
{
    if (data == NULL)
    {
        return false;
    }

    return tx_DAT(data, (uint16_t)strlen(data));
}


// ISR de TX: pasa el siguiente byte del buffer a TX1REG o apaga TX1IE si no hay mas.
void EUSART1_Transmit_ISR(void)
{
    uint8_t tail = eusart1TxTail;

    if (eusart1TxHead != tail)
    {
        TX1REG = eusart1TxBuffer[tail];
        eusart1TxTail = (uint8_t)((tail + 1) & EUSART1_TX_MASK);
    }
    else
    {
        PIE3bits.TX1IE = 0;
    }
}

// ISR de RX: anota los errores del byte recibido y lo manda a guardar.
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

// Lee RC1REG y guarda el byte en el buffer de RX; si esta lleno lo descarta y sube el aviso.
void EUSART1_RxDataHandler(void){
    uint8_t data = RC1REG;

    uint8_t head = eusart1RxHead;
    uint8_t next = (uint8_t)((head + 1) & EUSART1_RX_MASK);

    if (next == eusart1RxTail)
    {
        eusart1RxOverflow = 1;
    }
    else
    {
        eusart1RxBuffer[head] = data;
        eusart1RxHead = next;
    }
}

void EUSART1_DefaultFramingErrorHandler(void){}

// Si hubo overrun, reinicia el receptor apagando y encendiendo CREN.
void EUSART1_DefaultOverrunErrorHandler(void){
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
