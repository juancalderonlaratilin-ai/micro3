/**
  Generated Pin Manager File

  Company:
    Microchip Technology Inc.

  File Name:
    pin_manager.c

  Summary:
    This is the Pin Manager file generated using PIC10 / PIC12 / PIC16 / PIC18 MCUs

  Description:
    This header file provides implementations for pin APIs for all pins selected in the GUI.
    Generation Information :
        Product Revision  :  PIC10 / PIC12 / PIC16 / PIC18 MCUs - 1.81.8
        Device            :  PIC16F18426
        Driver Version    :  2.11
    The generated drivers are tested against the following:
        Compiler          :  XC8 2.36 and above
        MPLAB             :  MPLAB X 6.00

    Copyright (c) 2013 - 2015 released Microchip Technology Inc.  All rights reserved.
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

#include "pin_manager.h"





void PIN_MANAGER_Initialize(void)
{
    /**
    LATx registers
    */
    LATA = 0x00;
    LATC = 0x00;

    /**
    TRISx registers
    */
    /* [T-05 PRUEBA] ***** CÓDIGO DE PRUEBA: QUITAR ESTA PARTE PARA CONTINUAR CON LO DEMÁS *****
     * Sirve solo para probar el parpadeo de 1 Hz del LED de RA5 (pruebas 0.5 y 1.12).
     * Antes de seguir con las demás tareas hay que volver al valor de MCC: TRISA = 0x3F.
     *
     * TRISA = "Tri-State control del puerto A": un registro de 8 bits donde cada bit decide
     * la dirección del pin RA0..RA5 (RA5 es el pin 2 del chip de 14 pines).
     *   bit en 1 = el pin es ENTRADA   |   bit en 0 = el pin es SALIDA
     * MCC dejaba 0x3F (0b00111111): RA0..RA5 todos entradas.
     * Ahora 0x1F (0b00011111): el bit 5 pasa a 0, o sea RA5 es SALIDA (el LED); RA0..RA4 siguen
     * como antes. LATA5 ya vale 0 (LATA = 0x00 arriba), así que el pin arranca en nivel bajo. */
    TRISA = 0x1F;
    TRISC = 0x2F;

    /**
    ANSELx registers
    */
    ANSELC = 0x1F;
    /* [T-05 PRUEBA] ***** CÓDIGO DE PRUEBA: QUITAR ESTA PARTE PARA CONTINUAR CON LO DEMÁS *****
     * Antes de seguir con las demás tareas hay que volver al valor de MCC: ANSELA = 0x37.
     *
     * ANSELA = "Analog Select del puerto A": cada bit decide si el pin se usa como analógico.
     *   bit en 1 = analógico (el pin NO sirve como salida/entrada digital normal)
     *   bit en 0 = digital
     * Al reset los pines están en analógico, y MCC dejaba 0x37 (0b00110111), con ANSA5 = 1
     * (ANSA5 = bit 5 de ANSELA, el de RA5).
     * Ahora 0x17 (0b00010111): solo el bit 5 pasa a 0, o sea RA5 digital. Los demás no cambian.
     * Ojo: el LED prendería igual con ANSA5 = 1, porque ANSELA no afecta a las SALIDAS digitales
     * (datasheet §14.4.6, p.191). Solo hace que las LECTURAS del pin den siempre 0 y puede dar
     * resultados raros con instrucciones de leer-modificar-escribir sobre PORTA. Se baja el bit
     * porque la regla del proyecto y T-05 lo piden para todo pin digital. */
    ANSELA = 0x17;

    /**
    WPUx registers
    */
    WPUA = 0x00;
    WPUC = 0x00;

    /**
    ODx registers
    */
    ODCONA = 0x00;
    ODCONC = 0x00;

    /**
    SLRCONx registers
    */
    SLRCONA = 0x37;
    SLRCONC = 0x3F;

    /**
    INLVLx registers
    */
    INLVLA = 0x3F;
    INLVLC = 0x3F;





   
    
	
    RC4PPS = 0x0F;   //RC4->EUSART1:TX1;    
    RX1DTPPS = 0x15;   //RC5->EUSART1:RX1;    
}
  
void PIN_MANAGER_IOC(void)
{   
}

/**
 End of File
*/