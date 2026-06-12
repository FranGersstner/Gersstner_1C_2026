#include <stdio.h>
#include <stdint.h>
#include <stdlib.h> 

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h" 

#include "analog_io_mcu.h"
#include "uart_mcu.h"
#include "neopixel_stripe.h"
#include "servo_sg90.h"

// --- CONFIGURACIÓN DE HARDWARE ---
#define PIN_NEOPIXEL GPIO_18 
#define PIN_SERVO GPIO_3     // Pin actualizado para el servomotor SG90
#define CANTIDAD_LEDS 8

static analog_input_config_t adc_config = {
    .input = CH1,
    .mode = ADC_SINGLE,
    .func_p = NULL,
    .param_p = NULL
};

static serial_config_t uart_config = {
    .port = UART_PC,
    .baud_rate = 115200,
    .func_p = NULL,
    .param_p = NULL
};

// Variable global compartida
volatile uint8_t nivel_contraccion = 0;
neopixel_color_t colores_leds[CANTIDAD_LEDS];


/**
 * @brief Tarea 1: Adquisición y filtrado digital de EMG
 */
void vTaskEMGProcessing(void *pvParameters)
{
    uint16_t valor_adc_raw;
    float envolvente_filtrada = 0;
    const float alpha = 0.03; 

    while(1)
    {
        for(int i = 0; i < 10; i++)
        {
            AnalogInputReadSingle(CH1, &valor_adc_raw);
            int16_t senal_centrada = (int16_t)valor_adc_raw - 1280;
            uint16_t senal_rectificada = abs(senal_centrada);
            envolvente_filtrada = (alpha * senal_rectificada) + ((1.0 - alpha) * envolvente_filtrada);
            esp_rom_delay_us(2000); 
        }

        float max_esperado = 220.0; 
        if (envolvente_filtrada > max_esperado) envolvente_filtrada = max_esperado;
        
        nivel_contraccion = (uint8_t)((envolvente_filtrada / max_esperado) * 100.0);
        
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}

/**
 * @brief Tarea 2: Control de Actuadores (NeoPixel + Servo)
 */
void vTaskActuatorsControl(void *pvParameters)
{
    uint8_t leds_a_encender = 0;
    int8_t angulo_servo = -90; // Empezamos en la posición mínima

    while(1)
    {
        // --- 1. PROCESAMIENTO NEOPIXEL ---
        leds_a_encender = (nivel_contraccion * CANTIDAD_LEDS) / 100;
        for(int i = 0; i < CANTIDAD_LEDS; i++) 
        {
            if(i < leds_a_encender) colores_leds[i] = NeoPixelRgb2Color(0, 255, 0); 
            else colores_leds[i] = NeoPixelRgb2Color(0, 0, 0);   
        }
        NeoPixelSetArray(colores_leds);

        // --- 2. PROCESAMIENTO SERVO SG90 ---
        // Fórmula: mapeamos de [0 a 100] hacia [-90 a 90]
        angulo_servo = (int8_t)(((nivel_contraccion * 180) / 100) - 90);
        
        // Movemos físicamente el servo
        ServoMove(SERVO_0, angulo_servo);

        // --- 3. MONITOR SERIAL ---
        printf("Fuerza: %d%% | Angulo: %d grad\n", nivel_contraccion, angulo_servo);
        fflush(stdout);

        // Refresco general a 20Hz
        vTaskDelay(pdMS_TO_TICKS(50)); 
    }
}

void app_main(void)
{
    UartInit(&uart_config);
    AnalogInputInit(&adc_config);

    // Inicializamos LEDs
    NeoPixelInit(PIN_NEOPIXEL, CANTIDAD_LEDS, colores_leds);
    NeoPixelAllOff();

    // Inicializamos Servo en el GPIO 3
    ServoInit(SERVO_0, PIN_SERVO);
    // Lo mandamos a la posición de reposo absoluto (-90 grados) antes de arrancar
    ServoMove(SERVO_0, -90); 

    // Creación de las tareas concurrentes
    xTaskCreate(vTaskEMGProcessing, "EMG_Filter", 4096, NULL, 5, NULL);
    xTaskCreate(vTaskActuatorsControl, "Actuators_Task", 2048, NULL, 4, NULL);
}