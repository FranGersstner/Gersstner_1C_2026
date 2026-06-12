#include <stdio.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h" // Necesaria para el delay en microsegundos

#include "analog_io_mcu.h"
#include "uart_mcu.h"

uint16_t valor_adc;

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

void app_main(void)
{
    UartInit(&uart_config);
    AnalogInputInit(&adc_config);

    while(1)
    {
        for(int i = 0; i < 8; i++)
        {
            AnalogInputReadSingle(CH1, &valor_adc);
            
            // Probamos mandándole una etiqueta "EMG:" antes del número
            // y forzamos el retorno de carro (\r\n) que aman los plotters
            printf("EMG:%d\r\n", valor_adc);
            
            // ESTA LÍNEA ES CLAVE: Fuerza al micro a escupir el dato por el cable YA
            fflush(stdout); 
            
            esp_rom_delay_us(1500); 
        }
        vTaskDelay(1); 
    }
}