# Especificaciones Requeridas para Pre-Informe de Laboratorio

**Contexto para el Asistente (Claude/Gemini):**
Actúa como un ingeniero electrónico experto. A continuación se listan los requerimientos obligatorios solicitados por el profesor para el diseño, análisis y documentación de un sistema basado en microcontrolador (enfocado en ADC y comunicación USART). Por favor, ayúdame a desarrollar cada uno de estos puntos para mi pre-informe.

## 1. Documentación de Especificaciones del Microcontrolador
*   **Requisito:** Crear un documento de especificaciones detallando los valores y configuraciones del USART y el ADC.
*   **Acción requerida:** Listar las hojas de datos (datasheet) y extraer los valores límite, configuraciones de registros necesarios y tasas de baudios/frecuencias de muestreo basándonos en el microcontrolador específico que vamos a usar (ej. STM32, PIC, ATmega, etc.).

## 2. Análisis de Tiempos y Diagramas
*   **Requisito:** Generar diagramas de tiempo detallados.
*   **Elementos a incluir:**
    *   Tiempo de conversión del ADC.
    *   Tiempo de lectura del ADC.
    *   Tiempo empleado en la conversión de los datos (ya sea a formato decimal o ASCII).
    *   Tiempo del armado de la trama de datos para enviar por USART.

## 3. Diseño de Hardware (OBLIGATORIO)
*   **Cumplimiento:** El diseño de hardware final debe cumplir estrictamente con las especificaciones calculadas.
*   **Análisis de Impedancias:**
    *   Calcular y justificar la resistencia de manejo de entrada del circuito de *Sample and Hold* (Muestreo y Retención).
    *   Tomar en cuenta y documentar la resistencia de salida del amplificador operacional (OpAmp) que se está utilizando en el acondicionamiento de señal.
*   **Requisito de Fuente:** Independientemente de la fuente de alimentación principal utilizada, se debe dejar esa fuente física accesible en un pin específico del circuito.

## 4. Análisis de Datos y LabVIEW
*   **Adquisición:** Tomar 10,000 muestras utilizando LabVIEW.
*   **Análisis Estadístico:**
    *   Generar un histograma del periodo de las muestras.
    *   Calcular la desviación estándar del histograma del periodo.

## 5. Procesamiento de Señal y Requisitos Adicionales
*   **Modulación y Frecuencia:** "Modular" (procesar/acondicionar) los resultados del ADC para poder observar claramente la componente de 10 Hz.
*   **Auto-zero:** Definir y calcular cuánta precisión necesitamos en la técnica de *auto-zero* (autocero) para cancelar el voltaje de offset del sistema. *(2026-11-04: pasa a Macro por el cambio de enfoque del profesor; en Micro 3 el PWM reconstruye las señales muestreadas, `docs/especificacion_pwm.md`.)*
*   **Meta a corto plazo:** Tener el programa (código) terminado y funcionando para la próxima clase, junto con el pin de la fuente física listo.

---
**Instrucciones para generar el contenido:**
*Por favor, [Asistente], comienza ayudándome a redactar la sección 1 y 2. Te proporcionaré el modelo de mi microcontrolador y el OpAmp que estoy usando en el siguiente mensaje.*
```eof

Este archivo estructura todos los mensajes de "imagen.png" en requerimientos claros. Solo tienes que copiar el contenido del archivo `.md` y pegarlo en tu próxima herramienta de IA. ¡Éxitos con tu proyecto y el análisis en LabVIEW!