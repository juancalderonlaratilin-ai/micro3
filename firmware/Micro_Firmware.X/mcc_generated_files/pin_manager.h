/**
  @Generated Pin Manager Header File

  @Company:
    Microchip Technology Inc.

  @File Name:
    pin_manager.h

  @Summary:
    This is the Pin Manager file generated using PIC10 / PIC12 / PIC16 / PIC18 MCUs

  @Description
    This header file provides APIs for driver for .
    Generation Information :
        Product Revision  :  PIC10 / PIC12 / PIC16 / PIC18 MCUs - 1.81.8
        Device            :  PIC16F18426
        Driver Version    :  2.11
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

#ifndef PIN_MANAGER_H
#define PIN_MANAGER_H

/**
  Section: Included Files
*/

#include <xc.h>

#define INPUT   1
#define OUTPUT  0

#define HIGH    1
#define LOW     0

#define ANALOG      1
#define DIGITAL     0

#define PULL_UP_ENABLED      1
#define PULL_UP_DISABLED     0

// get/set CANAL_I aliases
#define CANAL_I_TRIS                 TRISCbits.TRISC0
#define CANAL_I_LAT                  LATCbits.LATC0
#define CANAL_I_PORT                 PORTCbits.RC0
#define CANAL_I_WPU                  WPUCbits.WPUC0
#define CANAL_I_OD                   ODCONCbits.ODCC0
#define CANAL_I_ANS                  ANSELCbits.ANSC0
#define CANAL_I_SetHigh()            do { LATCbits.LATC0 = 1; } while(0)
#define CANAL_I_SetLow()             do { LATCbits.LATC0 = 0; } while(0)
#define CANAL_I_Toggle()             do { LATCbits.LATC0 = ~LATCbits.LATC0; } while(0)
#define CANAL_I_GetValue()           PORTCbits.RC0
#define CANAL_I_SetDigitalInput()    do { TRISCbits.TRISC0 = 1; } while(0)
#define CANAL_I_SetDigitalOutput()   do { TRISCbits.TRISC0 = 0; } while(0)
#define CANAL_I_SetPullup()          do { WPUCbits.WPUC0 = 1; } while(0)
#define CANAL_I_ResetPullup()        do { WPUCbits.WPUC0 = 0; } while(0)
#define CANAL_I_SetPushPull()        do { ODCONCbits.ODCC0 = 0; } while(0)
#define CANAL_I_SetOpenDrain()       do { ODCONCbits.ODCC0 = 1; } while(0)
#define CANAL_I_SetAnalogMode()      do { ANSELCbits.ANSC0 = 1; } while(0)
#define CANAL_I_SetDigitalMode()     do { ANSELCbits.ANSC0 = 0; } while(0)

// get/set CANAL_Q aliases
#define CANAL_Q_TRIS                 TRISCbits.TRISC1
#define CANAL_Q_LAT                  LATCbits.LATC1
#define CANAL_Q_PORT                 PORTCbits.RC1
#define CANAL_Q_WPU                  WPUCbits.WPUC1
#define CANAL_Q_OD                   ODCONCbits.ODCC1
#define CANAL_Q_ANS                  ANSELCbits.ANSC1
#define CANAL_Q_SetHigh()            do { LATCbits.LATC1 = 1; } while(0)
#define CANAL_Q_SetLow()             do { LATCbits.LATC1 = 0; } while(0)
#define CANAL_Q_Toggle()             do { LATCbits.LATC1 = ~LATCbits.LATC1; } while(0)
#define CANAL_Q_GetValue()           PORTCbits.RC1
#define CANAL_Q_SetDigitalInput()    do { TRISCbits.TRISC1 = 1; } while(0)
#define CANAL_Q_SetDigitalOutput()   do { TRISCbits.TRISC1 = 0; } while(0)
#define CANAL_Q_SetPullup()          do { WPUCbits.WPUC1 = 1; } while(0)
#define CANAL_Q_ResetPullup()        do { WPUCbits.WPUC1 = 0; } while(0)
#define CANAL_Q_SetPushPull()        do { ODCONCbits.ODCC1 = 0; } while(0)
#define CANAL_Q_SetOpenDrain()       do { ODCONCbits.ODCC1 = 1; } while(0)
#define CANAL_Q_SetAnalogMode()      do { ANSELCbits.ANSC1 = 1; } while(0)
#define CANAL_Q_SetDigitalMode()     do { ANSELCbits.ANSC1 = 0; } while(0)

// get/set RC4 procedures
#define RC4_SetHigh()            do { LATCbits.LATC4 = 1; } while(0)
#define RC4_SetLow()             do { LATCbits.LATC4 = 0; } while(0)
#define RC4_Toggle()             do { LATCbits.LATC4 = ~LATCbits.LATC4; } while(0)
#define RC4_GetValue()              PORTCbits.RC4
#define RC4_SetDigitalInput()    do { TRISCbits.TRISC4 = 1; } while(0)
#define RC4_SetDigitalOutput()   do { TRISCbits.TRISC4 = 0; } while(0)
#define RC4_SetPullup()             do { WPUCbits.WPUC4 = 1; } while(0)
#define RC4_ResetPullup()           do { WPUCbits.WPUC4 = 0; } while(0)
#define RC4_SetAnalogMode()         do { ANSELCbits.ANSC4 = 1; } while(0)
#define RC4_SetDigitalMode()        do { ANSELCbits.ANSC4 = 0; } while(0)

// get/set RC5 procedures
#define RC5_SetHigh()            do { LATCbits.LATC5 = 1; } while(0)
#define RC5_SetLow()             do { LATCbits.LATC5 = 0; } while(0)
#define RC5_Toggle()             do { LATCbits.LATC5 = ~LATCbits.LATC5; } while(0)
#define RC5_GetValue()              PORTCbits.RC5
#define RC5_SetDigitalInput()    do { TRISCbits.TRISC5 = 1; } while(0)
#define RC5_SetDigitalOutput()   do { TRISCbits.TRISC5 = 0; } while(0)
#define RC5_SetPullup()             do { WPUCbits.WPUC5 = 1; } while(0)
#define RC5_ResetPullup()           do { WPUCbits.WPUC5 = 0; } while(0)
#define RC5_SetAnalogMode()         do { ANSELCbits.ANSC5 = 1; } while(0)
#define RC5_SetDigitalMode()        do { ANSELCbits.ANSC5 = 0; } while(0)

/**
   @Param
    none
   @Returns
    none
   @Description
    GPIO and peripheral I/O initialization
   @Example
    PIN_MANAGER_Initialize();
 */
void PIN_MANAGER_Initialize (void);

/**
 * @Param
    none
 * @Returns
    none
 * @Description
    Interrupt on Change Handling routine
 * @Example
    PIN_MANAGER_IOC();
 */
void PIN_MANAGER_IOC(void);



#endif // PIN_MANAGER_H
/**
 End of File
*/