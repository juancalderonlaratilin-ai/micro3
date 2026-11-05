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
/* [T-09] Declara adc_ascii_par(): manda I y Q al PC como texto "2048,2051\r\n". */
#include "adc_ascii.h"
/* [T-06] Declara pwm_init() y pwm_desde_adc(): un PWM por canal que repite la señal medida. */
#include "pwm.h"

/*
                         Main application
 */
/**
  [T-09, T-06] main
  Para qué sirve: muestrear I (RC0) y Q (RC1) a 1 kHz y, con cada par:
                  1. copiarlo a un PWM por canal (I -> PWM6 en RA2, Q -> PWM7 en RA4); con un
                     filtro pasa bajos en cada pin se vuelve a ver la señal analógica;
                  2. mandarlo al PC por la UART como texto "xxxx,xxxx\r\n" (I,Q; 1 de cada 2
                     pares = 500 líneas por segundo, ADC_PARES_POR_LINEA en adc_ascii.h).
                  Sin ninguna espera bloqueante (ni while que espere ni __delay_ms). El PC solo
                  lee líneas; el PIC no necesita ningún comando.
*/
void main(void)
{
    uint16_t i; // [T-09] muestra del canal I que entrega adc_iq_get (0 a 4095)
    uint16_t q; // [T-09] muestra del canal Q

    // initialize the device
    SYSTEM_Initialize();

    // [T-09] Pone el ADC en el canal I (MCC lo dejó en RA0) y arranca el Timer0 desde cero (C-12).
    // Va ANTES de encender las interrupciones, para que la primera que llegue ya sea del canal I.
    adc_iq_init();

    // [T-06] Timer2 + PWM6 (RA2) y PWM7 (RA4), con el duty en la mitad (1,85 V) hasta que
    // llegue el primer par. El PWM corre solo en hardware: no usa interrupciones.
    pwm_init();

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
    // El PC lo ignora porque no tiene la forma "número,número".
    tx_string("Micro3 listo\r\n");

    // Bucle infinito del programa. Cada vuelta es muy corta porque nada espera.
    while (1)
    {
        // [T-09] adc_iq_get devuelve true solo si la ISR del ADC dejó un par nuevo (cada 1 ms).
        // Si no hay par, regresa al instante y el bucle sigue.
        if (adc_iq_get(&i, &q))
        {
            // [T-06] Primero el PWM, para que la salida analógica cambie lo antes posible
            // después de la muestra (unos µs). El hardware lo aplica en el siguiente periodo
            // de 64 us.
            pwm_desde_adc(i, q);

            // [T-09] Después el texto: cada ADC_PARES_POR_LINEA pares arma y encola la línea.
            adc_ascii_par(i, q);
        }
        // Lo que llegue por RX se queda en el buffer sin leer: el PIC no recibe comandos.
    }
}
/**
 End of File
*/
