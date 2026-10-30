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
/* [T-04] Declara protocol_init() y protocol_task(), el protocolo de tramas con CRC-8. */
#include "protocol.h"
/* [T-09] Declara adc_iq_init() y adc_iq_get(), el muestreo de I y Q a 1 kHz. */
#include "adc_iq.h"
/* [PRUEBA ADC] Declara adc_prueba_task(): prueba temporal que manda "I:...,Q:..." en texto. */
#include "adc_prueba.h"

/* [PRUEBA ADC] MODO_PRUEBA_ADC: 1 = prueba TEMPORAL del ADC: el bucle solo manda líneas de
 * texto "I:2048,Q:2051" (20 por segundo, PRUEBA_PARES_POR_LINEA en adc_prueba.h) para verlas
 * en el Serial Plotter del Arduino IDE (no atiende el protocolo ni el eco). 0 = todo queda como antes (MODO_ECO decide). Se cambia aquí y se recompila. */
#define MODO_PRUEBA_ADC 1

/* [T-04] MODO_ECO: 1 = el programa devuelve cada byte tal cual llega (eco crudo, para repetir
 * las pruebas 1.5, 1.6 y 1.7 del Módulo 1). 0 = el programa atiende el protocolo de tramas
 * (Módulo 2). Se cambia aquí y se vuelve a compilar. */
#define MODO_ECO 0

/* [T-03] Cuenta los bytes que el eco no pudo devolver porque el buffer de TX estaba lleno.
 * Es "volatile" y global para poder verla en el depurador de MPLAB X (ventana Variables).
 * shortcut: es de 8 bits y da la vuelta tras 255; en la Fase 1 basta ver si es distinta
 * de 0. Si hace falta un conteo exacto, cambiar a uint16_t. */
volatile uint8_t echoBytesLost = 0;

/* [T-09] Cuenta las tramas DATA_IQ que no cupieron en el buffer de TX. "volatile" y global para
 * verla en el depurador. Debe quedar en 0 (prueba 4.14). */
volatile uint16_t adcTramasPerdidas = 0;

/*
                         Main application
 */
/**
  [T-03] main  (programa de eco para probar la UART)
  Para qué sirve: devolver al PC cada byte que llega por la UART, sin perder ni cambiar
                  ninguno, y sin ninguna espera bloqueante (ni while que espere ni
                  __delay_ms). Sirve para las pruebas 1.5, 1.6, 1.7, 1.9, 1.13 y 1.14.
*/
void main(void)
{
#if MODO_PRUEBA_ADC
    // [PRUEBA ADC] No hace falta ninguna variable aquí: adc_prueba_task tiene las suyas.
#elif MODO_ECO
    uint8_t b; // el byte que se acaba de leer de la UART
#else
    uint16_t i; // [T-09] promedio del canal I que entrega adc_iq_get (0 a 4095)
    uint16_t q; // [T-09] promedio del canal Q
#endif

    // initialize the device
    SYSTEM_Initialize();

    // [T-09] Pone el ADC en el canal I (MCC lo dejó en RA0) y arranca el Timer0 desde cero (C-12).
    // Va ANTES de encender las interrupciones, para que la primera que llegue ya sea del canal I.
    adc_iq_init();

    // [T-03] Interrupciones: sin esto, la ISR nunca se ejecuta y nada se envía ni se recibe.
    // PEIE = Peripheral Interrupt Enable (bit de INTCON): deja pasar las interrupciones de los
    // periféricos (aquí, la UART). GIE = Global Interrupt Enable (bit de INTCON): interruptor
    // general. Se encienden en este orden y AL FINAL de la configuración, para que ninguna
    // interrupción llegue con la UART a medio configurar.
    INTERRUPT_PeripheralInterruptEnable();
    INTERRUPT_GlobalInterruptEnable();

    // [T-03] Mensaje de arranque. tx_string solo mete los caracteres en el buffer de TX y
    // regresa enseguida; la ISR de TX los va enviando mientras main sigue. Si no cupiera
    // (no pasa: son 14 bytes y caben 63), devolvería false y no se enviaría nada.
    tx_string("Micro3 listo\r\n");

    /* [T-04] Deja el protocolo en reposo, esperando el primer byte de una trama. */
    protocol_init();

    // Bucle infinito del programa. Cada vuelta es muy corta porque nada espera.
    while (1)
    {
#if MODO_PRUEBA_ADC
        // [PRUEBA ADC] Solo la prueba del ADC: cada 50 ms una línea "I:...,Q:...". Si no hay
        // par nuevo, regresa al instante. Los bytes que lleguen por RX se quedan sin leer.
        adc_prueba_task();
#elif MODO_ECO
        // [T-03] EUSART1_Read devuelve true solo si había un byte esperando en el buffer de RX
        // (y lo deja en b). Si no hay nada devuelve false al instante y el bucle sigue.
        if (EUSART1_Read(&b))
        {
            // EUSART1_Write devuelve false si el buffer de TX está lleno. En la Fase 1 NO se
            // reintenta (reintentar sería esperar): el byte se pierde y solo se cuenta.
            // En la práctica no pasa: el eco sale a la misma velocidad a la que llega.
            if (!EUSART1_Write(b))
            {
                echoBytesLost++;
            }
        }
        // [T-09] En este modo el ADC sigue interrumpiendo (prueba 4.10), pero no se lee ni se
        // envía nada; por eso adcParesPerdidos sube y no significa nada.
#else
        // [T-04] Atiende los bytes que lleguen: los junta en tramas y responde (PING -> PONG).
        protocol_task();

        // [T-09] adc_iq_get devuelve true solo si la ISR dejó listo un par nuevo (cada
        // ADC_N_PROMEDIO ms). Se lee SIEMPRE, aunque el envío esté apagado, para que
        // adcParesPerdidos solo cuente pérdidas reales. Si no hay par, regresa al instante.
        if (adc_iq_get(&i, &q))
        {
            // Solo se envía si el PC lo pidió con ADC_STREAM (al arrancar está apagado).
            if (protocol_stream_on())
            {
                // false = la trama no cupo en el buffer de TX: no se reintenta, solo se cuenta.
                if (!protocol_send_iq(i, q))
                {
                    adcTramasPerdidas++;
                }
            }
        }
#endif
    }
}
/**
 End of File
*/