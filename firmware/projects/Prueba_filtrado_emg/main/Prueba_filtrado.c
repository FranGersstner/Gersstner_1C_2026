#include <stdio.h>
#include <stdint.h>
#include <stdlib.h> // Necesaria para la función abs()

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h" 

#include "analog_io_mcu.h"
#include "uart_mcu.h"

// Configuración de Periféricos (Drivers de la cátedra)
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

// Variable global para almacenar el nivel de contracción final (0 a 100%)
volatile uint8_t nivel_contraccion = 0;

/**
 * @brief Tarea de FreeRTOS enfocada SOLO en la adquisición y filtrado digital de EMG
 */
void vTaskEMGProcessing(void *pvParameters)
{
    uint16_t valor_adc_raw;
    float envolvente_filtrada = 0;
    
    // Coeficiente de filtrado (Alpha). Si ves que el porcentaje salta muy errático, bajalo a 0.01
    // Si ves que tarda mucho en reaccionar cuando apretás, subilo a 0.05
    const float alpha = 0.03; 

    while(1)
    {
        // Tomamos un bloque de 10 muestras a alta velocidad (~500Hz)
        for(int i = 0; i < 10; i++)
        {
            AnalogInputReadSingle(CH1, &valor_adc_raw);

            // 1. Restar el Offset DC (Tu línea base comprobada es ~1280)
            int16_t senal_centrada = (int16_t)valor_adc_raw - 1280;

            // 2. Rectificación (Valor Absoluto)
            uint16_t senal_rectificada = abs(senal_centrada);

            // 3. Filtrado Digital (Media Móvil Exponencial para la Envolvente)
            envolvente_filtrada = (alpha * senal_rectificada) + ((1.0 - alpha) * envolvente_filtrada);

            // Pequeña espera de 2 milisegundos entre muestras
            esp_rom_delay_us(2000); 
        }

        // --- MAPEO A PORCENTAJE (Calibrado a tu músculo) ---
        // Usamos el techo de 220.0 que calculamos antes
        float max_esperado = 220.0; 
        if (envolvente_filtrada > max_esperado) envolvente_filtrada = max_esperado;
        
        // Convertimos a un valor porcentual de 0 a 100
        nivel_contraccion = (uint8_t)((envolvente_filtrada / max_esperado) * 100.0);

        // Imprimimos solo el porcentaje para validarlo en el monitor
        printf("Fuerza Muscular: %d%%\n", nivel_contraccion);
        fflush(stdout);

        // Le damos un respiro de 10ms al sistema operativo
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}

void app_main(void)
{
    // Inicialización de la UART y el ADC
    UartInit(&uart_config);
    AnalogInputInit(&adc_config);

    // Creación de la ÚNICA tarea en FreeRTOS por el momento
    xTaskCreate(
        vTaskEMGProcessing,     // Función de la tarea
        "EMG_Filter_Task",      // Nombre
        4096,                   // Stack
        NULL,                   // Parámetros
        5,                      // Prioridad alta
        NULL                    // Handler
    );
}