# Instrucciones para Claude Code

@MASTER_PLAN.md

## Tu rol: desarrollador principal de código

El planificador es Gemini. Tú implementas. Los roles y el flujo están en "Roles y flujo de trabajo" de `MASTER_PLAN.md`.

- Implementa solo las tareas de "Tareas" (en `MASTER_PLAN.md`) con estado `aprobada`, de una en una. **Ahora:** T-09 (ADC de I y Q y envío en texto ASCII, detalle en `docs/especificacion_protocolo.md` y `docs/especificacion_adc.md`) está implementada; falta que la persona la pruebe en el chip. T-06 (un PWM por canal que reproduce las muestras, con filtro pasa bajos: `docs/especificacion_pwm.md`) también está implementada, sin probar. El auto-cero pasa a Macro (cambio de enfoque del profesor, 2026-11-04).
- Si una tarea contradice la especificación (`docs/`) o el enunciado, **detente**: no lo resuelvas por tu cuenta. Anota la duda en "Preguntas para el planificador" y avisa a la persona.
- No cambies decisiones de diseño (pines, baud, formato de las líneas ASCII, reloj). Si crees que hay que cambiarlas, propón el cambio en "Cambios propuestos" y no lo apliques.
- Escribe código en `firmware/`. Puedes editar `docs/estado.md`, `docs/notas_pic16f18426.md` y el estado de las filas de `docs/revision_codigo_mcc.md`. No reescribas el plan de Gemini.
- El código lo lee una persona que está aprendiendo: **comentarios en español, línea por línea, qué hace y por qué** (formato en `docs/instrucciones_claude_code_uart.md` §2).
- MCC: la persona puede usarlo. Una vez que el código generado se edita a mano, **no se vuelve a pulsar *Generate*** (reescribe los archivos).
- Al terminar una tarea: márcala `implementada, sin probar`, actualiza `docs/estado.md` y dile a la persona qué compilar y qué pruebas de `docs/protocolos_prueba.md` ejecutar. Nunca marques una tarea como `probada` sin el resultado de la persona.
- Commits: uno por tarea, con el ID en el mensaje (por ejemplo `T-03: UART sin esperas`). Hazlos solo cuando la persona lo pida.

## Preinforme

Antes de escribir o revisar cualquier preinforme, lee entero `docs/especificaciones_preinforme.md`: requisitos de la clase, valores del UART y el ADC, bloques, diagramas de tiempo, cumplimiento y estructura. Lo que no se midió va como «calculado» o «previsto», nunca como resultado.
