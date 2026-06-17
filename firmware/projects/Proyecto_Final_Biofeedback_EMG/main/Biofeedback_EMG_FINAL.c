/**
 * @file emg_biofeedback.c
 * @brief Sistema de adquisición, procesamiento y biofeedback de señales EMG
 *
 * @mainpage Biofeedback EMG Digital
 *
 * @section intro_sec Descripción General
 *
 * Esta aplicación implementa un sistema de biofeedback EMG (Electromiografía)
 * utilizando la placa ESP32-C6 y FreeRTOS.
 *
 * El sistema adquiere la señal analógica de un sensor muscular, la procesa digitalmente
 * (centrado, rectificado y filtrado pasa-bajos) para obtener la envolvente y calcular
 * un porcentaje de fuerza de contracción. 
 * * Este nivel de fuerza se transmite por Bluetooth a una aplicación móvil 
 * ("Bluetooth Electronics") y se refleja localmente de forma visual y mecánica.
 *
 * @section functionalities Funcionalidades
 *
 * - Adquisición de señal EMG analógica mediante ADC (CH1).
 * - Procesamiento digital de señales (Filtro promediador exponencial).
 * - Transmisión BLE (Gráfico continuo *G* y nivel de fuerza *P*).
 * - Control de actuador visual (Tira NeoPixel dividida en colores por umbrales).
 * - Control de actuador mecánico (Servo SG90 con lógica angular invertida).
 * - Tareas concurrentes mediante FreeRTOS.
 *
 * @section hardware Hardware utilizado
 *
 * - ESP32-C6
 * - Sensor EMG analógico conectado al CH1
 * - Tira NeoPixel (8 LEDs) en GPIO_18
 * - Servomotor SG90 en GPIO_3
 *
 * @author Francisco Gersstner
 * @date 17/06/2026
 */

/*==================[inclusions]=============================================*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h> 

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"

#include "analog_io_mcu.h"
#include "uart_mcu.h"
#include "neopixel_stripe.h"
#include "servo_sg90.h"
#include "ble_mcu.h"

/*==================[macros and definitions]=================================*/

/**
 * @brief Pin GPIO utilizado para controlar la tira de LEDs NeoPixel
 */
#define PIN_NEOPIXEL GPIO_18

/**
 * @brief Pin GPIO utilizado para enviar la señal PWM al servomotor SG90
 */
#define PIN_SERVO GPIO_3    

/**
 * @brief Cantidad total de LEDs que posee la tira NeoPixel conectada
 */
#define CANTIDAD_LEDS 8

/*==================[internal functions declaration]=========================*/

/**
 * @brief Callback de recepción Bluetooth
 *
 * Se ejecuta automáticamente al recibir datos desde el celular.
 *
 * @param data Puntero al arreglo de bytes recibidos
 * @param length Cantidad de bytes recibidos
 */
void read_data(uint8_t * data, uint8_t length);

/**
 * @brief Tarea de adquisición y transmisión de datos
 *
 * Captura las muestras de EMG, calcula el nivel de fuerza
 * y transmite los paquetes por Bluetooth.
 *
 * @param pvParameters Parámetro no utilizado
 */
void vTaskEMGProcessing(void *pvParameters);

/**
 * @brief Tarea de control de actuadores físicos
 *
 * Actualiza el estado de los LEDs NeoPixel y la posición 
 * del Servomotor basándose en el nivel de contracción actual.
 *
 * @param pvParameters Parámetro no utilizado
 */
void vTaskActuatorsControl(void *pvParameters);

/*==================[internal data definition]===============================*/

/**
 * @brief Porcentaje actual de fuerza de contracción (0 a 100)
 *
 * Variable compartida entre la tarea de procesamiento EMG y la tarea de actuadores.
 */
volatile uint8_t nivel_contraccion = 0;

/**
 * @brief Arreglo de colores para actualizar el estado de la tira NeoPixel
 */
neopixel_color_t colores_leds[CANTIDAD_LEDS];

/**
 * @brief Configuración del ADC para la lectura de la señal muscular
 */
static analog_input_config_t adc_config = {
    .input = CH1,
    .mode = ADC_SINGLE,
    .func_p = NULL,
    .param_p = NULL
};

/**
 * @brief Configuración UART para depuración mediante monitor serial
 */
static serial_config_t uart_config = {
    .port = UART_PC,
    .baud_rate = 115200,
    .func_p = NULL,
    .param_p = NULL
};

/*==================[external functions definition]==========================*/

/**
 * @brief Función principal
 *
 * Inicializa:
 * - UART
 * - ADC
 * - NeoPixel
 * - Servomotor
 * - Módulo Bluetooth (BLE)
 * - Tareas FreeRTOS
 */
void app_main(void)
{
    /* Inicialización de interfaces comunes */
    UartInit(&uart_config);
    AnalogInputInit(&adc_config);

    /* Inicialización de actuador visual (NeoPixel) */
    NeoPixelInit(PIN_NEOPIXEL, CANTIDAD_LEDS, colores_leds);
    NeoPixelAllOff();

    /* Inicialización de actuador mecánico (Servo) */
    ServoInit(SERVO_0, PIN_SERVO);
    ServoMove(SERVO_0, 90); 

    /* Inicialización y configuración de Bluetooth */
    ble_config_t ble_configuration = {
        "A_EMG_Biofeedback", 
        read_data          
    };
    BleInit(&ble_configuration);

    /* Creación de tareas del sistema */
    xTaskCreate(vTaskEMGProcessing, "EMG_and_BLE", 4096, NULL, 5, NULL);
    xTaskCreate(vTaskActuatorsControl, "Actuators_Task", 2048, NULL, 4, NULL);
}

/*==================[interrupts definition]==================================*/

/**
 * @brief Callback de recepción de datos por Bluetooth
 * * Actualmente vacío ya que el sistema opera como transmisor unidireccional.
 */
void read_data(uint8_t * data, uint8_t length)
{
    /* Aquí se procesarían los comandos enviados desde la App móvil */
}

/*==================[tasks definition]=======================================*/

/**
 * @brief Tarea principal de procesamiento EMG
 *
 * Funciones realizadas:
 * * 1. Toma 10 muestras analógicas del CH1.
 * 2. Centra, rectifica y filtra la señal (promedio exponencial).
 * 3. Escala la envolvente a un porcentaje (0-100%) con zona muerta.
 * 4. Transmite los datos a la App móvil vía BLE.
 */
void vTaskEMGProcessing(void *pvParameters)
{
    uint16_t valor_adc_raw;
    float envolvente_filtrada = 0;
    const float alpha = 0.01; 

    char msg_bluetooth[256];
    char chunk_aux[32];

    while(1)
    {
        msg_bluetooth[0] = '\0';

        /* 1. Tomamos las muestras del EMG y las preparamos para el GRÁFICO (*G) */
        for(int i = 0; i < 10; i++)
        {
            AnalogInputReadSingle(CH1, &valor_adc_raw);
            
            /* Eliminación de offset y rectificación */
            int16_t senal_centrada = (int16_t)valor_adc_raw - 1280;
            uint16_t senal_rectificada = abs(senal_centrada);
            
            /* Filtrado digital pasa-bajos */
            envolvente_filtrada = (alpha * senal_rectificada) + ((1.0 - alpha) * envolvente_filtrada);
            
            /* Armado de paquete para el gráfico */
            sprintf(chunk_aux, "*G%d*", senal_centrada);
            strcat(msg_bluetooth, chunk_aux);

            esp_rom_delay_us(2000);
        }

        /* 2. Calculamos el porcentaje de fuerza */
        float max_esperado = 220.0;
        if (envolvente_filtrada > max_esperado) envolvente_filtrada = max_esperado;
        uint8_t porcentaje_calculado = (uint8_t)((envolvente_filtrada / max_esperado) * 100.0);

        /* Aplicación de zona muerta para rechazo de ruido */
        if (porcentaje_calculado < 35) {
            nivel_contraccion = 0;
        } else {
            nivel_contraccion = porcentaje_calculado;
        }
        
        /* 3. Enviamos los datos si hay conexión */
        if(BleStatus() == BLE_CONNECTED)
        {
            /* Se anexa el nivel de fuerza para el indicador/barra (*P) */
            sprintf(chunk_aux, "*P%d*", nivel_contraccion);
            strcat(msg_bluetooth, chunk_aux);
            
            BleSendString(msg_bluetooth);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * @brief Tarea de actualización de actuadores
 *
 * Funciones realizadas:
 * * 1. Lee el porcentaje de contracción global.
 * 2. Enciende LEDs secuencialmente por tramos (3 rojos, 3 verdes, 2 azules).
 * 3. Mueve el servo en proporción inversa a la fuerza.
 */
void vTaskActuatorsControl(void *pvParameters)
{
    uint8_t leds_a_encender = 0;
    int8_t angulo_servo = 90;

    while(1)
    {
        uint8_t copia_nivel = nivel_contraccion;

        /* --- 1. NEOPIXEL CON COLORES SEGMENTADOS --- */
        leds_a_encender = (copia_nivel * CANTIDAD_LEDS) / 100;
        for(int i = 0; i < CANTIDAD_LEDS; i++)
        {
            if(i < leds_a_encender) {
                if (i < 3) {
                    colores_leds[i] = NeoPixelRgb2Color(255, 0, 0); /* 3 Rojos */
                } else if (i < 6) {
                    colores_leds[i] = NeoPixelRgb2Color(0, 255, 0); /* 3 Verdes */
                } else {
                    colores_leds[i] = NeoPixelRgb2Color(0, 0, 255); /* 2 Azules */
                }
            } else {
                colores_leds[i] = NeoPixelRgb2Color(0, 0, 0); /* Apagado */
            }
        }
        NeoPixelSetArray(colores_leds);

        /* --- 2. SERVO SG90 --- */
        /* Matemáticamente invertido: Fuerza máxima = 0 grados, Reposo = 90 grados */
        angulo_servo = (int8_t)(90 - ((copia_nivel * 180) / 100));
        ServoMove(SERVO_0, angulo_servo);

        /* --- 3. MONITOR SERIAL DE RESPALDO --- */
        printf("Fuerza: %d%% | Angulo: %d grad\n", copia_nivel, angulo_servo);
        fflush(stdout);

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/*==================[end of file]============================================*/