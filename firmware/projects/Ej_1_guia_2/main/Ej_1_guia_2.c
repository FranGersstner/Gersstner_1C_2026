/*! @mainpage Medidor de distancia por ultrasonido
 *
 * @section genDesc General Description
 *
 * El sistema mide distancia con un sensor HC-SR04 y:
 * - Muestra el valor en LCD
 * - Indica rangos con LEDs
 * - Permite iniciar/detener (TEC1)
 * - Permite HOLD (TEC2)
 *
 * @section hardConn Hardware Connection
 *
 * | Peripheral | ESP32 |
 * |------------|--------|
 * | ECHO       | GPIO_3 |
 * | TRIGGER    | GPIO_2 |
 * | TEC1       | GPIO_X |
 * | TEC2       | GPIO_X |
 *
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gpio_mcu.h"
#include "led.h"
#include "switch.h"
#include "ultrasonic.h"
#include "lcd.h"
#include <stdbool.h>

/*==================[macros and definitions]=================================*/

#define REFRESH_TIME 1000 / portTICK_PERIOD_MS

/*==================[internal data definition]===============================*/

typedef enum{
    IDLE,
    MEDIR,
    HOLD
} estado_t;

static estado_t estado = IDLE;
static uint16_t distancia = 0;

/*==================[internal functions declaration]=========================*/

void actualizar_leds(uint16_t d);
void mostrar_lcd(uint16_t d, bool hold);

/*==================[external functions definition]==========================*/

void app_main(void){

    /* Inicialización */
    LedsInit();
    SwitchesInit();
    LcdInit();
    UltrasonicInit(GPIO_2, GPIO_3);

    while(true){

        /* ===== Lectura de teclas ===== */

        if(SwitchesRead() & SWITCH_1){   // TEC1
            vTaskDelay(200 / portTICK_PERIOD_MS);

            if(estado == IDLE)
                estado = MEDIR;
            else
                estado = IDLE;
        }

        if(SwitchesRead() & SWITCH_2){   // TEC2
            vTaskDelay(200 / portTICK_PERIOD_MS);

            if(estado == MEDIR)
                estado = HOLD;
            else if(estado == HOLD)
                estado = MEDIR;
        }

        /* ===== Máquina de estados ===== */

        switch(estado){

            case IDLE:
                actualizar_leds(0);
                mostrar_lcd(0, false);
                break;

            case MEDIR:
                distancia = UltrasonicReadDistance();

                actualizar_leds(distancia);
                mostrar_lcd(distancia, false);

                vTaskDelay(REFRESH_TIME);
                break;

            case HOLD:
                actualizar_leds(distancia);
                mostrar_lcd(distancia, true);
                break;
        }
    }
}

/*==================[internal functions definition]==========================*/

void actualizar_leds(uint16_t d){

    if(d < 10){
        LedOff(LED_1);
        LedOff(LED_2);
        LedOff(LED_3);
    }
    else if(d < 20){
        LedOn(LED_1);
        LedOff(LED_2);
        LedOff(LED_3);
    }
    else if(d < 30){
        LedOn(LED_1);
        LedOn(LED_2);
        LedOff(LED_3);
    }
    else{
        LedOn(LED_1);
        LedOn(LED_2);
        LedOn(LED_3);
    }
}

/*------------------------------------------------*/

void mostrar_lcd(uint16_t d, bool hold){

    char buffer[20];

    LcdClear();

    if(hold){
        LcdWriteString("HOLD");
    }else{
        sprintf(buffer, "Dist: %d cm", d);
        LcdWriteString(buffer);
    }
}

/*==================[end of file]============================================*/