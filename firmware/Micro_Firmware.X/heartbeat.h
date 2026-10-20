/* [T-05 PRUEBA] ***** ARCHIVO DE PRUEBA: QUITAR ESTA PARTE PARA CONTINUAR CON LO DEMÁS *****
 *
 * heartbeat.h / heartbeat.c: LED de estado en RA5 que parpadea a 1 Hz, movido por una
 * interrupción de Timer4, sin esperas. Existe solo para probar el reloj y comprobar que la UART
 * no bloquea el programa (pruebas 0.5 y 1.12 de docs/protocolos_prueba.md).
 *
 * CÓMO QUITARLO (todo está marcado con "[T-05 PRUEBA]"; búscalo con ese texto):
 *   1. Sacar heartbeat.c y heartbeat.h del proyecto en MPLAB X (clic derecho > Remove From
 *      Project) y borrar los dos archivos.
 *   2. main.c: borrar el #include "heartbeat.h" y la llamada a Heartbeat_Initialize().
 *   3. interrupt_manager.c: borrar el #include de heartbeat.h y la rama de TMR4 con
 *      Heartbeat_ISR().
 *   4. pin_manager.c: volver a TRISA = 0x3F y ANSELA = 0x37 (los valores de MCC).
 */

#ifndef HEARTBEAT_H
#define HEARTBEAT_H

/* Configura Timer4 para que interrumpa cada 20 ms y habilita su interrupción.
 * Se llama UNA vez desde main, antes de encender PEIE y GIE.
 * Esta interrupción necesita que interrupt_manager.c la atienda con Heartbeat_ISR() (ya lo
 * hace desde el paso 3 de T-05); si no, la bandera TMR4IF se quedaría en 1 y la ISR se
 * repetiría sin parar. */
void Heartbeat_Initialize(void);

/* Atiende una interrupción de Timer4: cada 25 interrupciones (500 ms) invierte el LED de RA5.
 * La llama SOLO interrupt_manager.c, cuando TMR4IE y TMR4IF valen 1. Nunca desde main. */
void Heartbeat_ISR(void);

#endif  // HEARTBEAT_H
