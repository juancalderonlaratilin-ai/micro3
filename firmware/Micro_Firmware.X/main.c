/**
  Generated Main Source File

  Company:
    Microchip Technology Inc.

  File Name:
    main.c

  Summary:
    This is the main file generated using PIC10 / PIC12 / PIC16 / PIC18 MCUs

  Description:
    This header file provides implementations for driver APIs for all modules selected in the GUI.
    Generation Information :
        Product Revision  :  PIC10 / PIC12 / PIC16 / PIC18 MCUs - 1.81.8
        Device            :  PIC16F18426
        Driver Version    :  2.00
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

#include "mcc_generated_files/mcc.h"
/* [T-09] Declara adc_iq_init() y adc_iq_get(), el muestreo de I y Q a 1 kHz. */
#include "adc_iq.h"
/* [T-09] Declara adc_ascii_task(): manda I y Q al PC como texto "I:2048,Q:2051\r\n". */
#include "adc_ascii.h"

/*
                         Main application
 */
/**
  [T-09] main
  Para qué sirve: muestrear I (RC0) y Q (RC1) a 1 kHz y mandarlos al PC por la UART como
                  líneas de texto "I:xxxx,Q:xxxx\r\n" (500 por segundo, ADC_PARES_POR_LINEA en
                  adc_ascii.h), sin ninguna espera bloqueante (ni while que espere ni
                  __delay_ms). El PC (herramientas/interfaz_micro3.py, el Serial Plotter del
                  Arduino IDE o LabVIEW) solo lee líneas; el PIC no necesita ningún comando.
*/
void main(void)
{
    // initialize the device
    SYSTEM_Initialize();

    // [T-09] Pone el ADC en el canal I (MCC lo dejó en RA0) y arranca el Timer0 desde cero (C-12).
    // Va ANTES de encender las interrupciones, para que la primera que llegue ya sea del canal I.
    adc_iq_init();

    // [T-03] Interrupciones: sin esto, la ISR nunca se ejecuta y nada se envía.
    // PEIE = Peripheral Interrupt Enable (bit de INTCON): deja pasar las interrupciones de los
    // periféricos (aquí, la UART y el ADC). GIE = Global Interrupt Enable (bit de INTCON):
    // interruptor general. Se encienden en este orden y AL FINAL de la configuración, para que
    // ninguna interrupción llegue con la UART o el ADC a medio configurar.
    INTERRUPT_PeripheralInterruptEnable();
    INTERRUPT_GlobalInterruptEnable();

    // [T-03] Mensaje de arranque. tx_string solo mete los caracteres en el buffer de TX y
    // regresa enseguida; la ISR de TX los va enviando mientras main sigue. Si no cupiera
    // (no pasa: son 14 bytes y caben 63), devolvería false y no se enviaría nada.
    // El PC lo ignora porque no tiene la forma "I:...,Q:...".
    tx_string("Micro3 listo\r\n");

    // Bucle infinito del programa. Cada vuelta es muy corta porque nada espera.
    while (1)
    {
        // [T-09] Si la ISR del ADC dejó un par nuevo, lo cuenta y cada ADC_PARES_POR_LINEA
        // pares manda una línea "I:...,Q:...". Si no hay par nuevo, regresa al instante.
        // Lo que llegue por RX se queda en el buffer sin leer: el PIC no recibe comandos.
        adc_ascii_task();
    }
}
/**
 End of File
*/
