/**
 * @file osciloscopio.c
 * @brief Osciloscopio ECG con ADC + DAC + UART + FreeRTOS
 *
 * @mainpage Osciloscopio ECG Digital
 *
 * @section intro_sec Descripción General
 *
 * Esta aplicación implementa un osciloscopio digital básico utilizando
 * la placa ESP32-C6 y FreeRTOS.
 *
 * El sistema genera una señal ECG digital almacenada en un arreglo,
 * la convierte a analógica mediante el DAC y posteriormente la vuelve
 * a adquirir utilizando el ADC para visualizarla en el Serial Plotter
 * de VSCode.
 *
 * Además, se incorporan controles para modificar la velocidad de
 * reproducción de la señal ECG simulando:
 *
 * - Taquicardia (aumento de frecuencia)
 * - Bradicardia (disminución de frecuencia)
 * - Retorno a frecuencia normal
 *
 * @section functionalities Funcionalidades
 *
 * - Generación de señal ECG mediante DAC
 * - Lectura ADC por CH1
 * - Transmisión UART compatible con Serial Plotter
 * - Uso de FreeRTOS mediante tareas
 * - Control mediante interrupciones de teclas
 * - Control mediante comandos UART
 *
 * @section controls Controles
 *
 * | Acción | Método |
 * |--------|---------|
 * | Aumentar frecuencia | TEC1 o tecla 'T' |
 * | Disminuir frecuencia | TEC2 o tecla 'B' |
 * | Restaurar frecuencia | tecla 'R' |
 *
 * @section hardware Hardware utilizado
 *
 * - ESP32-C6
 * - DAC interno
 * - ADC CH1
 * - UART
 * - Teclas SWITCH_1 y SWITCH_2
 *
 * @author Francisco Gersstner
 * @date 2026
 */

/*==================[inclusions]=============================================*/

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "analog_io_mcu.h"
#include "uart_mcu.h"
#include "switch.h"

/*==================[macros and definitions]=================================*/

/**
 * @brief Tamaño de stack de la tarea principal
 */
#define TASK_STACK_SIZE     2048

/**
 * @brief Prioridad de la tarea principal
 */
#define TASK_PRIORITY       5

/**
 * @brief Delay por defecto entre muestras ECG
 */
#define DEFAULT_DELAY_MS   20

/**
 * @brief Incremento/decremento del delay
 */
#define STEP_DELAY_MS      1

/**
 * @brief Delay mínimo permitido
 */
#define MIN_DELAY_MS       1

/**
 * @brief Delay máximo permitido
 */
#define MAX_DELAY_MS       100

/*==================[ECG data]===============================================*/

/**
 * @brief Señal ECG digital
 *
 * Contiene muestras discretas de una señal electrocardiográfica.
 * Cada valor es enviado al DAC para generar una señal analógica.
 */
unsigned char ECG[] = {
17,17,17,17,17,17,17,17,17,17,17,18,18,18,17,17,17,17,17,17,17,18,18,18,18,18,18,18,17,17,16,16,16,16,17,17,18,18,18,17,17,17,17,
18,18,19,21,22,24,25,26,27,28,29,31,32,33,34,34,35,37,38,37,34,29,24,19,15,14,15,16,17,17,17,16,15,14,13,13,13,13,13,13,13,12,12,
10,6,2,3,15,43,88,145,199,237,252,242,211,167,117,70,35,16,14,22,32,38,37,32,27,24,24,26,27,28,28,27,28,28,30,31,31,31,32,33,34,36,
38,39,40,41,42,43,45,47,49,51,53,55,57,60,62,65,68,71,75,79,83,87,92,97,101,106,111,116,121,125,129,133,136,138,139,140,140,139,137,
133,129,123,117,109,101,92,84,77,70,64,58,52,47,42,39,36,34,31,30,28,27,26,25,25,25,25,25,25,25,25,24,24,24,24,25,25,25,25,25,25,25,
24,24,24,24,24,24,24,24,23,23,22,22,21,21,21,20,20,20,20,20,19,19,18,18,18,19,19,19,19,18,17,17,18,18,18,18,18,18,18,18,17,17,17,17,
17,17,17
};

/*==================[internal functions declaration]=========================*/

/**
 * @brief Tarea principal del sistema
 *
 * Genera la señal ECG, realiza la adquisición ADC y transmite
 * los datos por UART.
 *
 * @param pvParameter Parámetro no utilizado
 */
void TaskADC(void *pvParameter);

/**
 * @brief ISR de tecla 1
 *
 * Incrementa la frecuencia de reproducción del ECG.
 *
 * @param param Parámetro no utilizado
 */
void tecla1_isr(void *param);

/**
 * @brief ISR de tecla 2
 *
 * Disminuye la frecuencia de reproducción del ECG.
 *
 * @param param Parámetro no utilizado
 */
void tecla2_isr(void *param);

/**
 * @brief Callback UART
 *
 * Procesa comandos recibidos desde la PC.
 *
 * @param param Parámetro no utilizado
 */
void FuncUart(void *param);

/*==================[internal data definition]===============================*/

/**
 * @brief Variable donde se almacena el valor ADC leído
 */
uint16_t valor_adc = 0;

/**
 * @brief Índice actual del arreglo ECG
 */
uint16_t ecg_index = 0;

/**
 * @brief Delay actual entre muestras ECG
 *
 * Variable compartida entre ISR y tarea principal.
 */
volatile uint32_t delay_ecg = DEFAULT_DELAY_MS;

/**
 * @brief Buffer utilizado para transmisión UART
 */
char uart_buffer[32];

/**
 * @brief Configuración del ADC
 */
static analog_input_config_t adc_config = {
    .input = CH1,
    .mode = ADC_SINGLE,
    .func_p = NULL,
    .param_p = NULL
};

/**
 * @brief Configuración UART
 */
static serial_config_t uart_config = {
    .port = UART_PC,
    .baud_rate = 115200,
    .func_p = FuncUart,
    .param_p = NULL
};

/*==================[external functions definition]==========================*/

/**
 * @brief Función principal
 *
 * Inicializa:
 * - UART
 * - ADC
 * - DAC
 * - Teclas
 * - Interrupciones
 * - Tarea FreeRTOS
 */
void app_main(void)
{
    /* Inicialización UART */
    UartInit(&uart_config);

    /* Inicialización ADC */
    AnalogInputInit(&adc_config);

    /* Inicialización DAC */
    AnalogOutputInit();

    /* Inicialización de teclas */
    SwitchesInit();

    /* Activación de interrupciones */
    SwitchActivInt(SWITCH_1, tecla1_isr, NULL);
    SwitchActivInt(SWITCH_2, tecla2_isr, NULL);

    /* Creación de tarea principal */
    xTaskCreate(
        TaskADC,
        "ADC_Task",
        TASK_STACK_SIZE,
        NULL,
        TASK_PRIORITY,
        NULL
    );
}

/*==================[interrupts definition]==================================*/

/**
 * @brief ISR tecla 1
 *
 * Simula taquicardia disminuyendo el delay.
 */
void tecla1_isr(void *param)
{
    if(delay_ecg > MIN_DELAY_MS)
    {
        delay_ecg -= STEP_DELAY_MS;
    }
}

/**
 * @brief ISR tecla 2
 *
 * Simula bradicardia aumentando el delay.
 */
void tecla2_isr(void *param)
{
    if(delay_ecg < MAX_DELAY_MS)
    {
        delay_ecg += STEP_DELAY_MS;
    }
}

/**
 * @brief Callback UART
 *
 * Procesa comandos:
 *
 * - T -> aumenta frecuencia
 * - B -> disminuye frecuencia
 * - R -> restaura frecuencia
 */
void FuncUart(void *param)
{
    uint8_t dato;

    /* Lectura del byte recibido */
    UartReadByte(UART_PC, &dato);

    switch(dato)
    {
        case 'T':
        case 't':

            if(delay_ecg > MIN_DELAY_MS)
            {
                delay_ecg -= STEP_DELAY_MS;
            }

            break;

        case 'B':
        case 'b':

            if(delay_ecg < MAX_DELAY_MS)
            {
                delay_ecg += STEP_DELAY_MS;
            }

            break;

        case 'R':
        case 'r':

            delay_ecg = DEFAULT_DELAY_MS;

            break;
    }
}

/*==================[tasks definition]=======================================*/

/**
 * @brief Tarea principal del osciloscopio
 *
 * Funciones realizadas:
 *
 * 1. Envía muestras ECG al DAC
 * 2. Lee señal analógica desde CH1
 * 3. Envía datos por UART
 * 4. Actualiza índice ECG
 * 5. Controla velocidad de reproducción
 */
void TaskADC(void *pvParameter)
{
    while(true)
    {
        /* Enviar muestra ECG al DAC */
        AnalogOutputWrite(ECG[ecg_index]);

        /* Leer señal desde ADC */
        AnalogInputReadSingle(CH1, &valor_adc);

        /* Formato compatible con Serial Plotter */
        sprintf(uart_buffer, ">ECG:%d,Delay:%ld\r\n", valor_adc, delay_ecg);

        /* Enviar datos por UART */
        UartSendString(UART_PC, uart_buffer);

        /* Incrementar índice */
        ecg_index++;

        /* Reiniciar índice al finalizar el arreglo */
        if(ecg_index >= (sizeof(ECG) / sizeof(ECG[0])))
        {
            ecg_index = 0;
        }

        /* Delay variable según frecuencia seleccionada */
        vTaskDelay(pdMS_TO_TICKS(delay_ecg));
    }
}

/*==================[end of file]============================================*/