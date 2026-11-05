# Prompt para Claude Code: T-06 (PWM de reconstrucción) sobre el envío ASCII

> **Ya ejecutado (2026-11-04):** Claude Code revisó y compiló T-06 con XC8 2.40 (0 errores; ver `docs/estado.md`). Se conserva como registro.

Copiar desde la línea de abajo y pegarlo en Claude Code, abierto en la carpeta `micro3`.

---

Tarea: **T-06**, un PWM por canal que reproduce las señales muestreadas. Está aprobada por la persona el 2026-11-04 por un cambio de enfoque del profesor: el auto-cero pasa a Macro. Se monta sobre el envío ASCII de T-09.

**Lee primero, en este orden:**
1. `docs/especificacion_pwm.md`, §1 a §5. Lo que hay que hacer está en §5.
2. `docs/especificacion_protocolo.md`, §1 y §4. Ahí están la línea `2048,2051\r\n` y `adc_ascii_par`.
3. `docs/notas_pic16f18426.md`: §5 (códigos PPS), §9.1 (PWM6/PWM7) y §9.3 (Timer2).
4. `docs/instrucciones_claude_code_uart.md` §2, con el estilo de los comentarios.

**Situación:** el chat de Claude ya dejó escrita una versión en `firmware/Micro_Firmware.X/`: `pwm.c/.h`, `main.c` y `adc_ascii.c/.h` con `adc_ascii_par`, más `pwm.c` registrado en `nbproject/`. Esa versión **no se ha compilado con XC8**. Solo se revisó con gcc y cabeceras simuladas. Tu trabajo es dejarla correcta y compilada; no la reescribas si ya cumple.

**Pasos:**
1. **Revisar contra la especificación.** Compara cada archivo con `especificacion_pwm.md` §5: orden y valores de §5.3, funciones de §5.2 y §5.4, y `main` de §5.5. Compara también `adc_ascii.c/.h` con `especificacion_protocolo.md` §4. Si falta un archivo, créalo según la especificación. Anota cada diferencia.
2. **Verificar los nombres.** Comprueba en el `pic16f18426.h` de XC8 2.40 (`...\xc8\v2.40\pic\include\proc\`) cada registro y bit que use el código. Son estos: `PWM6CON`, `PWM6DCH`, `PWM6DCL`, `PWM7CON`, `PWM7DCH`, `PWM7DCL`, `T2CON`, `T2CLKCON`, `T2HLT`, `T2PR`, `T2TMR`, `RA2PPS`, `RA4PPS`, `RC2PPS`, `LATAbits.LATA2/LATA4`, `ANSELAbits.ANSA2/ANSA4` y `TRISAbits.TRISA2/TRISA4`. Confirma además que tras el reset `CCPTMRS1` asigna PWM6 y PWM7 al Timer2. Anota en `docs/notas_pic16f18426.md` §9.1 lo que verifiques, con la fuente (`.h` o página del datasheet). **No escribas ningún nombre de memoria.**
3. **Limpiar.** Comprueba que en `firmware/` y en `nbproject/` no quede ninguna referencia a `crc8`, `protocol`, `adc_prueba`, `MODO_ECO`, `MODO_PRUEBA_ADC` ni `adc_ascii_task`. Comprueba también que `pwm.c`, `pwm.h`, `adc_ascii.c` y `adc_iq.c` estén registrados en `configurations.xml` y `Makefile-default.mk`.
4. **Compilar con XC8 2.40.** Usa `make` del proyecto o `xc8-cc -mcpu=16F18426` con todas las fuentes, y deja la salida fuera de la carpeta del proyecto. Corrige los errores. Reporta los avisos (los `(520)` de funciones de MCC sin usar ya se conocen) y el uso de memoria de programa y de datos.
5. **Formato.** Deja los archivos del firmware en ISO-8859-1 con CRLF, como el resto. Usa comentarios en español, línea por línea, con la etiqueta `[T-06]`.

**Reglas:**
- No toques `eusart1.*`, `interrupt_manager.*`, `adc_iq.*`, `adcc.*`, `tmr0.*`, `pin_manager.*`, `mcc.*` ni `device_config.*`. No pulses *Generate* en MCC.
- No cambies decisiones de diseño:
  - pines (RA2 = PWM de I, RA4 = PWM de Q, RC2 = salida del Timer0);
  - Timer2 con `T2PR` = 255 a Fosc/4;
  - `duty = (código + 2) >> 2`;
  - línea `I,Q\r\n` cada 2 pares, a 115200.
- Sin esperas bloqueantes: nada de `while` que espere ni `__delay_ms`. El PWM no usa interrupciones.
- Si algo de la especificación contradice el datasheet o el `.h`, **detente**. Anótalo en "Preguntas para el planificador" de `MASTER_PLAN.md` y avísame.

**Al terminar:**
- Deja T-06 en `MASTER_PLAN.md` como `implementada, sin probar`. Agrega «compilada con XC8 2.40» si compiló.
- Actualiza `docs/estado.md`: estado del código, siguiente paso y una fila en el registro.
- Dame una tabla con las diferencias que encontraste y lo que cambiaste.
- Dime qué probar: Módulo 3 (3.1 a 3.8) y Módulo 2 (2.1 a 2.4) de `docs/protocolos_prueba.md`, con el filtro de `especificacion_pwm.md` §6.
- No hagas commit. Lo hago yo.
