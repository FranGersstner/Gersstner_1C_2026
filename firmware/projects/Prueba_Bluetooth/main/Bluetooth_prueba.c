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

// --- CONFIGURACIÓN DE HARDWARE ---
#define PIN_NEOPIXEL GPIO_18
#define PIN_SERVO GPIO_3    
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

volatile uint8_t nivel_contraccion = 0;
neopixel_color_t colores_leds[CANTIDAD_LEDS];

void read_data(uint8_t * data, uint8_t length){
    // Callback vacío
}

/**
 * @brief Tarea 1: Adquisición EMG y transmisión Bluetooth
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

        // 1. Tomamos las muestras del EMG y las preparamos para el GRÁFICO (*G)
        for(int i = 0; i < 10; i++)
        {
            AnalogInputReadSingle(CH1, &valor_adc_raw);
            int16_t senal_centrada = (int16_t)valor_adc_raw - 1280;
            uint16_t senal_rectificada = abs(senal_centrada);
            envolvente_filtrada = (alpha * senal_rectificada) + ((1.0 - alpha) * envolvente_filtrada);
            
            sprintf(chunk_aux, "*G%d*", senal_centrada);
            strcat(msg_bluetooth, chunk_aux);

            esp_rom_delay_us(2000);
        }

        // 2. Calculamos el porcentaje de fuerza
        float max_esperado = 220.0;
        if (envolvente_filtrada > max_esperado) envolvente_filtrada = max_esperado;
        uint8_t porcentaje_calculado = (uint8_t)((envolvente_filtrada / max_esperado) * 100.0);

        if (porcentaje_calculado < 35) {
            nivel_contraccion = 0;
        } else {
            nivel_contraccion = porcentaje_calculado;
        }
        
        // 3. Enviamos los datos si hay conexión
        if(BleStatus() == BLE_CONNECTED)
        {
            // Le pegamos al final del mensaje el porcentaje de fuerza (*P)
            sprintf(chunk_aux, "*P%d*", nivel_contraccion);
            strcat(msg_bluetooth, chunk_aux);
            
            BleSendString(msg_bluetooth);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * @brief Tarea 2: Control de Actuadores (LEDs y Servo)
 */
void vTaskActuatorsControl(void *pvParameters)
{
    uint8_t leds_a_encender = 0;
    int8_t angulo_servo = 90;

    while(1)
    {
        uint8_t copia_nivel = nivel_contraccion;

        // --- 1. NEOPIXEL CON LOS COLORES QUE PEDISTE ---
        leds_a_encender = (copia_nivel * CANTIDAD_LEDS) / 100;
        for(int i = 0; i < CANTIDAD_LEDS; i++)
        {
            if(i < leds_a_encender) {
                if (i < 3) {
                    colores_leds[i] = NeoPixelRgb2Color(255, 0, 0); // 3 Rojos
                } else if (i < 6) {
                    colores_leds[i] = NeoPixelRgb2Color(0, 255, 0); // 3 Verdes
                } else {
                    colores_leds[i] = NeoPixelRgb2Color(0, 0, 255); // 2 Azules
                }
            } else {
                colores_leds[i] = NeoPixelRgb2Color(0, 0, 0); // Apagado
            }
        }
        NeoPixelSetArray(colores_leds);

        // --- 2. SERVO SG90 ---
        angulo_servo = (int8_t)(90 - ((copia_nivel * 180) / 100));
        ServoMove(SERVO_0, angulo_servo);

        // --- 3. MONITOR SERIAL ---
        printf("Fuerza: %d%% | Angulo: %d grad\n", copia_nivel, angulo_servo);
        fflush(stdout);

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void app_main(void)
{
    UartInit(&uart_config);
    AnalogInputInit(&adc_config);

    NeoPixelInit(PIN_NEOPIXEL, CANTIDAD_LEDS, colores_leds);
    NeoPixelAllOff();

    ServoInit(SERVO_0, PIN_SERVO);
    ServoMove(SERVO_0, 90); 

    ble_config_t ble_configuration = {
        "A_EMG_Biofeedback", 
        read_data          
    };
    BleInit(&ble_configuration);

    xTaskCreate(vTaskEMGProcessing, "EMG_and_BLE", 4096, NULL, 5, NULL);
    xTaskCreate(vTaskActuatorsControl, "Actuators_Task", 2048, NULL, 4, NULL);
}