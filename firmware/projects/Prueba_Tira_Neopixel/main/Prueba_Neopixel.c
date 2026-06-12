#include <stdio.h>
#include <stdint.h>
#include <stdlib.h> 

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h" 

#include "analog_io_mcu.h"
#include "uart_mcu.h"
#include "neopixel_stripe.h"

// --- CONFIGURACIÓN DE HARDWARE ---
#define PIN_NEOPIXEL GPIO_18 // Pin actualizado a tu hardware
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

// Variable global compartida entre tareas
volatile uint8_t nivel_contraccion = 0;

// Arreglo de memoria para los colores de la tira
neopixel_color_t colores_leds[CANTIDAD_LEDS];


/**
 * @brief Tarea 1: Adquisición y filtrado de EMG
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

        printf("Fuerza Muscular: %d%%\n", nivel_contraccion);
        
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}

/**
 * @brief Tarea 2: Control Visual - Barra de intensidad NeoPixel
 */
void vTaskNeoPixelControl(void *pvParameters)
{
    uint8_t leds_a_encender = 0;

    while(1)
    {
        // 1. Mapeo: 100% de contracción equivale a los 8 LEDs prendidos
        leds_a_encender = (nivel_contraccion * CANTIDAD_LEDS) / 100;

        // 2. Modificamos el arreglo en memoria
        for(int i = 0; i < CANTIDAD_LEDS; i++) 
        {
            if(i < leds_a_encender) 
            {
                // Encendemos en Verde (R=0, G=255, B=0)
                colores_leds[i] = NeoPixelRgb2Color(0, 255, 0); 
            } 
            else 
            {
                // Apagamos los LEDs restantes
                colores_leds[i] = NeoPixelRgb2Color(0, 0, 0);   
            }
        }
        
        // 3. Enviamos el arreglo completo a la tira
        NeoPixelSetArray(colores_leds);

        // Refresco a 20Hz
        vTaskDelay(pdMS_TO_TICKS(50)); 
    }
}

void app_main(void)
{
    UartInit(&uart_config);
    AnalogInputInit(&adc_config);

    // Inicializamos el driver NeoPixel en el GPIO 18
    NeoPixelInit(PIN_NEOPIXEL, CANTIDAD_LEDS, colores_leds);
    
    // Apagamos todo al iniciar
    NeoPixelAllOff();

    // Creación de las tareas concurrentes
    xTaskCreate(vTaskEMGProcessing, "EMG_Filter", 4096, NULL, 5, NULL);
    xTaskCreate(vTaskNeoPixelControl, "NeoPixel_Task", 2048, NULL, 4, NULL);
}