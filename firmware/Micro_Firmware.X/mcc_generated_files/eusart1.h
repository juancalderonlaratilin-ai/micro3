/**
  EUSART1 Generated Driver API Header File

  @Company
    Microchip Technology Inc.

  @File Name
    eusart1.h

  @Summary
    This is the generated header file for the EUSART1 driver using PIC10 / PIC12 / PIC16 / PIC18 MCUs

  @Description
    This header file provides APIs for driver for EUSART1.
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

#ifndef EUSART1_H
#define EUSART1_H

/**
  Section: Included Files
*/

#include <xc.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus  // Provide C++ Compatibility

    extern "C" {

#endif


/**
  Section: Macro Declarations
*/

#define EUSART1_DataReady  (EUSART1_is_rx_ready())

/**
  Section: Data Type Definitions
*/

typedef union {
    struct {
        unsigned perr : 1;
        unsigned ferr : 1;
        unsigned oerr : 1;
        unsigned reserved : 5;
    };
    uint8_t status;
}eusart1_status_t;

/* [C-09] Aquí MCC declaraba como "extern" los contadores eusart1TxBufferRemaining y
 * eusart1RxCount. Ya no existen: cada buffer usa solo cabeza y cola, que son
 * privadas de eusart1.c (los demás archivos usan las funciones de abajo). */

/* [C-06] Aquí MCC declaraba como "extern" los punteros EUSART1_TxDefaultInterruptHandler
 * y EUSART1_RxDefaultInterruptHandler. Ya no existen: interrupt_manager.c llama directo
 * a EUSART1_Transmit_ISR() y EUSART1_Receive_ISR(), declaradas más abajo. */

/**
  Section: EUSART1 APIs
*/

/**
  @Summary
    Initialization routine that takes inputs from the EUSART1 GUI.

  @Description
    This routine initializes the EUSART1 driver.
    This routine must be called before any other EUSART1 routine is called.

  @Preconditions
    None

  @Param
    None

  @Returns
    None

  @Comment
    
*/
void EUSART1_Initialize(void);

/**
  [C-09] EUSART1_is_tx_ready
  ¿Cabe al menos un byte más en el buffer de TX? No espera. Se llama solo desde main.

  @Returns
    true  si el buffer de TX NO está lleno (EUSART1_Write aceptará el byte)
    false si está lleno
*/
bool EUSART1_is_tx_ready(void);

/**
  [C-09] EUSART1_is_rx_ready
  ¿Hay al menos un byte recibido esperando en el buffer de RX? No espera.
  Se llama solo desde main.

  @Returns
    true  si hay algún byte por leer con EUSART1_Read
    false si el buffer de RX está vacío
*/
bool EUSART1_is_rx_ready(void);

/**
  [C-10] EUSART1_is_tx_done
  ¿Ya salió TODO por el cable? Se llama solo desde main.

  @Returns
    true  solo si el buffer de TX está vacío Y el registro de desplazamiento
          terminó (TRMT = 1)
    false si queda algún byte en el buffer o se está enviando uno
*/
bool EUSART1_is_tx_done(void);

/**
  @Summary
    Gets the error status of the last read byte.

  @Description
    This routine gets the error status of the last read byte.

  @Preconditions
    EUSART1_Initialize() function should have been called
    before calling this function. The returned value is only
    updated after a read is called.

  @Param
    None

  @Returns
    the status of the last read byte

  @Example
	<code>
    void main(void)
    {
        volatile uint8_t rxData;
        volatile eusart1_status_t rxStatus;
        
        // Initialize the device
        SYSTEM_Initialize();
        
        // Enable the Global Interrupts
        INTERRUPT_GlobalInterruptEnable();
        
        while(1)
        {
            // Logic to echo received data
            // [C-07] Read ya no devuelve el byte: lo deja en rxData y devuelve true/false
            if(EUSART1_Read(&rxData))
            {
                rxStatus = EUSART1_get_last_status();
                if(rxStatus.ferr){
                    LED_0_SetHigh();
                }
            }
        }
    }
    </code>
 */
eusart1_status_t EUSART1_get_last_status(void);

/**
  [C-07] EUSART1_Read
  Saca UN byte del buffer de RX SIN esperar. Se llama solo desde main.
  El estado de error de ese byte queda disponible en EUSART1_get_last_status().

  @Param
    byte - dirección donde se deja el byte leído (solo se escribe si hay dato)

  @Returns
    true  si había un byte y quedó en *byte
    false si el buffer de RX está vacío (*byte no se toca)
*/
bool EUSART1_Read(uint8_t *byte);

/**
  [C-08] EUSART1_Write
  Pone UN byte en el buffer de TX SIN esperar; la ISR de TX lo envía por la UART.
  Se llama solo desde main.

  @Param
    txData - byte a enviar

  @Returns
    true  si el byte quedó encolado
    false si el buffer de TX estaba lleno (el byte NO se encola)
*/
bool EUSART1_Write(uint8_t txData);

/**
  [C-08] tx_DAT  (firma pedida en clase)
  Encola un bloque de "size" bytes en el buffer de TX SIN esperar. TODO O NADA:
  si no caben los size bytes, no se encola ninguno. Se llama solo desde main.

  @Param
    DATA - dirección del primer byte a enviar
    size - cuántos bytes enviar

  @Returns
    true  si el bloque quedó encolado (o si size es 0)
    false si DATA es NULL o no hay espacio para el bloque completo
*/
bool tx_DAT(const char *DATA, uint16_t size);

/**
  [C-08] tx_string  (firma pedida en clase)
  Encola una cadena terminada en '\0' (el '\0' no se envía). Usa tx_DAT.
  Se llama solo desde main.

  @Param
    data - dirección del primer carácter de la cadena

  @Returns
    true  si la cadena completa quedó encolada
    false si data es NULL o no cabe
*/
bool tx_string(const char *data);

/**
  @Summary
    Maintains the driver's transmitter state machine and implements its ISR.

  @Description
    This routine is used to maintain the driver's internal transmitter state
    machine.This interrupt service routine is called when the state of the
    transmitter needs to be maintained in a non polled manner.

  @Preconditions
    EUSART1_Initialize() function should have been called
    for the ISR to execute correctly.

  @Param
    None

  @Returns
    None
*/
void EUSART1_Transmit_ISR(void);

/**
  @Summary
    Maintains the driver's receiver state machine and implements its ISR

  @Description
    This routine is used to maintain the driver's internal receiver state
    machine.This interrupt service routine is called when the state of the
    receiver needs to be maintained in a non polled manner.

  @Preconditions
    EUSART1_Initialize() function should have been called
    for the ISR to execute correctly.

  @Param
    None

  @Returns
    None
*/
void EUSART1_Receive_ISR(void);

/**
  @Summary
    Maintains the driver's receiver state machine

  @Description
    This routine is called by the receive state routine and is used to maintain 
    the driver's internal receiver state machine. It should be called by a custom
    ISR to maintain normal behavior

  @Preconditions
    EUSART1_Initialize() function should have been called
    for the ISR to execute correctly.

  @Param
    None

  @Returns
    None
*/
void EUSART1_RxDataHandler(void);

/**
  @Summary
    Set EUSART1 Framing Error Handler

  @Description
    This API sets the function to be called upon EUSART1 framing error

  @Preconditions
    Initialize  the EUSART1 before calling this API

  @Param
    Address of function to be set as framing error handler

  @Returns
    None
*/
void EUSART1_SetFramingErrorHandler(void (* interruptHandler)(void));

/**
  @Summary
    Set EUSART1 Overrun Error Handler

  @Description
    This API sets the function to be called upon EUSART1 overrun error

  @Preconditions
    Initialize  the EUSART1 module before calling this API

  @Param
    Address of function to be set as overrun error handler

  @Returns
    None
*/
void EUSART1_SetOverrunErrorHandler(void (* interruptHandler)(void));

/**
  @Summary
    Set EUSART1 Error Handler

  @Description
    This API sets the function to be called upon EUSART1 error

  @Preconditions
    Initialize  the EUSART1 module before calling this API

  @Param
    Address of function to be set as error handler

  @Returns
    None
*/
void EUSART1_SetErrorHandler(void (* interruptHandler)(void));

/**
  [C-09] EUSART1_get_overflow
  Dice si la ISR de RX descartó algún byte porque el buffer de RX estaba lleno
  y borra el aviso. Se llama solo desde main.

  @Returns
    true  si hubo al menos un byte descartado desde la última llamada
    false si no se perdió nada
*/
bool EUSART1_get_overflow(void);

#ifdef __cplusplus  // Provide C++ Compatibility

    }

#endif

#endif  // EUSART1_H
/**
 End of File
*/
