/* [T-05 PRUEBA] ***** ARCHIVO DE PRUEBA: QUITAR ESTA PARTE PARA CONTINUAR CON LO DEMÁS *****
 * Para quitarlo, mira la lista "CÓMO QUITARLO" al inicio de heartbeat.h.
 *
 * Qué hace este archivo (pasos 2 y 3 de T-05): configurar Timer4 como base de tiempo
 * (Heartbeat_Initialize) y atender su interrupción para parpadear el LED (Heartbeat_ISR).
 * Se eligió Timer4 y no Timer2 porque Timer2 queda reservado para el PWM del auto-cero
 * (PWM6/PWM7, docs/especificacion_diseno_uart.md §3) y Timer0/Timer1 para disparar el ADC.
 * Timer4 es el mismo módulo que Timer2 ("lo que dice el datasheet de Timer2 vale para los
 * temporizadores pares", DS cap. 20, p.285), con registros T4CON, T4CLKCON, T4PR y T4TMR.
 *
 * Las cuentas (reloj del chip: Fosc = 16 MHz):
 *   Fosc/4 (reloj del temporizador) ........ 16 MHz / 4   = 4 000 000 Hz
 *   prescaler 1:128 ......................... 4 MHz / 128  =    31 250 Hz  (cada cuenta = 32 µs)
 *   T4PR = 124 (cuenta de 0 a 124 = 125) .... 31 250 / 125 =       250 Hz  (cada 4 ms)
 *   postscaler 1:5 (una interrupción cada 5) 250 / 5       =        50 Hz  (cada 20 ms)
 *   Heartbeat_ISR cuenta 25 interrupciones: 25 x 20 ms = 500 ms, y ahí invierte el LED
 *   (500 ms encendido + 500 ms apagado = parpadeo de 1 Hz).
 * Son números enteros exactos con el reloj nominal. La exactitud real depende del oscilador
 * interno (±2 % nominal, docs/criterios_diseno.md): la medirá la prueba 0.4.
 */

#include <xc.h>        // registros y bits del PIC16F18426 (pic16f18426.h)
#include "heartbeat.h"

/* [T-05 PRUEBA] Cuántas interrupciones de Timer4 (20 ms cada una) hay que contar antes de
 * invertir el LED: 25 x 20 ms = 500 ms. Para cambiar la velocidad del parpadeo se cambia
 * solo este número (por ejemplo 50 daría 1 s encendido y 1 s apagado, o sea 0,5 Hz). */
#define HEARTBEAT_TICKS_PER_TOGGLE 25

/* [T-05 PRUEBA] Interrupciones contadas desde la última vez que se invirtió el LED.
 * La escribe y la lee SOLO Heartbeat_ISR, y main nunca la toca: por eso no necesita "volatile"
 * ni protección contra interrupciones. "static" = privada de este archivo.
 * Un uint8_t llega hasta 255 y aquí solo se cuenta hasta 25, sobra. */
static uint8_t heartbeatTicks = 0;

/**
  [T-05 PRUEBA] Heartbeat_Initialize
  Para qué sirve: dejar Timer4 contando solo y avisando por interrupción cada 20 ms.
  Quién la llama: main, una sola vez, después de SYSTEM_Initialize() y antes de encender PEIE y GIE.
  Parámetros:     ninguno.
  Devuelve:       nada.
  Registros que toca: PIE4, PIR4, T4CON, T4CLKCON, T4PR, T4TMR (nombres comprobados en
                  pic16f18426.h; función de cada uno en los comentarios de abajo).
*/
void Heartbeat_Initialize(void)
{
    // PIE4bits.TMR4IE = bit TMR4IE del registro PIE4 (Peripheral Interrupt Enable 4):
    // interruptor que deja que Timer4 interrumpa. Se apaga mientras se configura, para que
    // no llegue un aviso con el temporizador a medio configurar.
    PIE4bits.TMR4IE = 0;

    // T4CON = Timer4 Control. Bit 7 ON = 0: el temporizador queda apagado y con sus contadores
    // en cero mientras se configura. (Los demás bits se escriben al final, en un solo paso.)
    T4CON = 0x00;

    // T4CLKCON = Timer4 Clock Control. Campo CS (Clock Select), bits 3:0 = 0001 = Fosc/4
    // (tabla 20-3, DS p.305). OJO: CS vale 0000 al reset y 0000 es el PIN externo T4CKIPPS, no
    // el reloj del chip: sin escribir este registro el temporizador no cuenta nada.
    T4CLKCON = 0x01;

    // T4PR = Timer4 Period Register. El contador T4TMR sube hasta T4PR y a la cuenta siguiente
    // vuelve a 0, así que el período es T4PR + 1 = 125 cuentas (DS §20.1, p.286).
    T4PR = 124;

    // T4TMR = Timer4 Timer Register (el contador). Escribir T4CON NO lo pone en cero (DS
    // §20.1), así que se pone a mano para empezar siempre desde 0.
    T4TMR = 0;

    // PIR4bits.TMR4IF = bit TMR4IF del registro PIR4 (Peripheral Interrupt Request 4): bandera
    // que el hardware sube cuando Timer4 cumple su período con el postscaler. NO se baja sola:
    // hay que bajarla por software. Se baja ahora para no arrancar con un aviso viejo.
    PIR4bits.TMR4IF = 0;

    // Se enciende el aviso de Timer4. La interrupción solo llegará cuando main encienda
    // también PEIE (Peripheral Interrupt Enable) y GIE (Global Interrupt Enable).
    PIE4bits.TMR4IE = 1;

    // T4CON = 0xF4 = 0b 1 111 0100:
    //   bit 7   ON      = 1    enciende el temporizador. Con T4HLT en su valor de reset (0, modo
    //                          MODE = 00000 "compuerta por software") cuenta libre mientras ON = 1.
    //   bits 6:4 CKPS   = 111  prescaler 1:128
    //   bits 3:0 OUTPS  = 0100 postscaler 1:5: la bandera TMR4IF sube una de cada 5 veces que el
    //                          contador llega a T4PR (valores de la tabla del registro TxCON)
    // Se escribe todo en una sola instrucción y ON = 1 va al final, cuando ya está todo listo.
    T4CON = 0xF4;
}

/**
  [T-05 PRUEBA] Heartbeat_ISR
  Para qué sirve: atender cada interrupción de Timer4 (una cada 20 ms) y, cada 25 de ellas
                  (500 ms), invertir el LED de RA5. Resultado: parpadeo de 1 Hz.
  Quién la llama: INTERRUPT_InterruptManager (interrupt_manager.c), SOLO cuando TMR4IE y
                  TMR4IF valen 1. Nunca desde main.
  Parámetros:     ninguno.
  Devuelve:       nada.
  Variables:      heartbeatTicks (privada). Registros: PIR4 (bandera) y LATA (nivel de RA5).
  Es corta a propósito: corre dentro de la interrupción única del chip, y la UART necesita que
  la ISR de RX se atienda en menos de unos 174 µs (docs/especificacion_diseno_uart.md §6).
*/
void Heartbeat_ISR(void)
{
    // Se baja la bandera TMR4IF lo primero. El hardware NO la baja solo (DS, registro PIR4:
    // "must be cleared in software"). Si no se bajara, esta ISR se repetiría sin parar.
    // Bajarla al inicio (y no al final) evita perder una interrupción que llegue mientras
    // se ejecuta el resto de la función.
    PIR4bits.TMR4IF = 0;

    // Una interrupción más (cada una son 20 ms).
    heartbeatTicks++;

    // Cuando se completan las 25 (500 ms) se reinicia la cuenta y se cambia el LED.
    if (heartbeatTicks >= HEARTBEAT_TICKS_PER_TOGGLE)
    {
        heartbeatTicks = 0;

        // LATA = "Latch A": registro donde cada bit guarda el nivel que el puerto A pone en su
        // pin. LATAbits.LATA5 es el bit de RA5. "~" invierte el bit (0 pasa a 1 y 1 pasa a 0):
        // si el LED estaba apagado se enciende y al revés. Se usa LATA y no PORTA porque leer
        // el latch da el nivel que se ESCRIBIÓ, sin depender de lo que mida el pin.
        LATAbits.LATA5 = ~LATAbits.LATA5;
    }
}
