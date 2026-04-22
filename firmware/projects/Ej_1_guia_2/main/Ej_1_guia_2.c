/*! @mainpage Medidor de distancia por ultrasonido
 *
 * @section genDesc General Description
 *
 * El sistema mide distancia utilizando un sensor ultrasónico HC-SR04.
 * La distancia obtenida se utiliza para:
 *
 * - Mostrar el valor en un display de 3 dígitos (LCD ITSE0803)
 * - Indicar el rango de distancia mediante LEDs
 * - Permitir iniciar/detener la medición (TEC1)
 * - Permitir congelar el valor medido (HOLD con TEC2)
 *
 * El sistema está implementado utilizando FreeRTOS, separando la lectura
 * de teclas y la lógica de medición en tareas independientes.
 *
 * @section funcionamiento Funcionamiento
 *
 * Máquina de estados:
 *
 * - IDLE: sistema detenido, display apagado
 * - MEDIR: mide cada 1 segundo
 * - HOLD: mantiene el último valor medido
 *
 * @section hardConn Hardware Connection
 *
 * | EDU-ESP | PERIFÉRICO |
 * |---------|------------|
 * | GPIO_3  | ECHO       |
 * | GPIO_2  | TRIGGER    |
 * | +5V     | +5V        |
 * | GND     | GND        |
 *
 * @section ledsControl Control de LEDs
 *
 * - d < 10 cm      → todos apagados
 * - 10 ≤ d < 20 cm → LED_1
 * - 20 ≤ d ≤ 30 cm → LED_1 + LED_2
 * - d > 30 cm      → LED_1 + LED_2 + LED_3
 *
 * @section tasks Tareas FreeRTOS
 *
 * - TareaTeclas: lectura de botones y cambio de estado
 * - TareaMedicion: medición, display y LEDs
 *
 * @author Gersstner Francisco
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gpio_mcu.h"
#include "led.h"
#include "switch.h"
#include "hc_sr04.h"
#include "lcditse0803.h"
#include <stdbool.h>

/*==================[macros and definitions]=================================*/

#define LOOP_DELAY_MS 50
#define REFRESH_TICKS (1000 / LOOP_DELAY_MS)

#define PRIORIDAD_1 5
#define PRIORIDAD_2 4

#define MEMORIA_DISPONIBLE_1 1024
#define MEMORIA_DISPONIBLE_2 2048

/*==================[internal data definition]===============================*/

typedef enum{
    IDLE,
    MEDIR,
    HOLD
} estado_t;

static volatile estado_t estado = IDLE;
static volatile uint16_t distancia = 0;

/*==================[internal functions declaration]=========================*/

/**
 * @brief Actualiza el estado de los LEDs según la distancia
 * @param d Distancia en cm
 */
void actualizar_leds(uint16_t d);

/**
 * @brief Tarea de lectura de teclas
 * @param pvParameter No utilizado
 */
void TareaTeclas(void *pvParameter);

/**
 * @brief Tarea de medición y control del sistema
 * @param pvParameter No utilizado
 */
void TareaMedicion(void *pvParameter);

/*==================[external functions definition]==========================*/

/**
 * @brief Función principal
 *
 * Inicializa periféricos y crea tareas FreeRTOS
 */
void app_main(void){

    LedsInit();
    SwitchesInit();
    LcdItsE0803Init();
    HcSr04Init(GPIO_3, GPIO_2);

    xTaskCreate(TareaTeclas, "Teclas", MEMORIA_DISPONIBLE_1, NULL, PRIORIDAD_1, NULL);
    xTaskCreate(TareaMedicion, "Medicion", MEMORIA_DISPONIBLE_2, NULL, PRIORIDAD_2, NULL);
}

/*==================[tasks definition]=======================================*/

/**
 * @brief Tarea que lee teclas y actualiza estado
 *
 * - TEC1: IDLE ↔ MEDIR
 * - TEC2: MEDIR ↔ HOLD
 */
void TareaTeclas(void *pvParameter){

    uint8_t teclas;
    uint8_t teclas_prev = 0;

    while(true){

        teclas = SwitchesRead();

        if((teclas & SWITCH_1) && !(teclas_prev & SWITCH_1)){
            if(estado == IDLE)
                estado = MEDIR;
            else
                estado = IDLE;
        }

        if((teclas & SWITCH_2) && !(teclas_prev & SWITCH_2)){
            if(estado == MEDIR)
                estado = HOLD;
            else if(estado == HOLD)
                estado = MEDIR;
        }

        teclas_prev = teclas;

        vTaskDelay(LOOP_DELAY_MS / portTICK_PERIOD_MS);
    }
}

/*------------------------------------------------*/

/**
 * @brief Tarea que ejecuta la medición y controla salidas
 *
 * Máquina de estados:
 * - IDLE: apaga display y LEDs
 * - MEDIR: mide cada 1 segundo
 * - HOLD: mantiene el valor
 */
void TareaMedicion(void *pvParameter){

    uint16_t timer_medicion = 0;

    while(true){

        switch(estado){

            case IDLE:
                actualizar_leds(0);
                LcdItsE0803Off();
                break;

            case MEDIR:
                timer_medicion++;

                if(timer_medicion >= REFRESH_TICKS){

                    distancia = HcSr04ReadDistanceInCentimeters();

                    actualizar_leds(distancia);
                    LcdItsE0803Write(distancia);

                    timer_medicion = 0;
                }
                break;

            case HOLD:
                actualizar_leds(distancia);
                LcdItsE0803Write(distancia);
                break;
        }

        vTaskDelay(LOOP_DELAY_MS / portTICK_PERIOD_MS);
    }
}

/*==================[internal functions definition]==========================*/

/**
 * @brief Control de LEDs según distancia
 * @param d Distancia en cm
 */
void actualizar_leds(uint16_t d){

    if(d < 10){
        LedOff(LED_1);
        LedOff(LED_2);
        LedOff(LED_3);
    }
    else if(d >= 10 && d < 20){
        LedOn(LED_1);
        LedOff(LED_2);
        LedOff(LED_3);
    }
    else if(d >= 20 && d <= 30){
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

/*==================[end of file]============================================*/