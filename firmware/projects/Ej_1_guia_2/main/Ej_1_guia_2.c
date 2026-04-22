/**
 * @file main.c
 * @brief Implementación de un medidor de distancia por ultrasonido con FreeRTOS
 */

/*! @mainpage Medidor de distancia por ultrasonido
 *
 * @section genDesc Descripción General
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
 * El sistema se basa en una máquina de estados:
 *
 * - IDLE: sistema detenido, display apagado
 * - MEDIR: realiza una medición cada 1 segundo
 * - HOLD: mantiene el último valor medido sin actualizarlo
 *
 * @section timing Temporización
 *
 * El sistema realiza una medición cada 1 segundo utilizando un contador
 * basado en un período de ejecución de 50 ms. Esto evita lecturas continuas
 * del sensor y permite una visualización estable del valor, facilitando
 * la función HOLD.
 *
 * @section hardConn Conexión de Hardware
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
 * - 10 ≤ d < 20 cm → LED_1 encendido
 * - 20 ≤ d ≤ 30 cm → LED_1 y LED_2 encendidos
 * - d > 30 cm      → LED_1, LED_2 y LED_3 encendidos
 *
 * @section tasks Tareas FreeRTOS
 *
 * - TareaTeclas: lectura de botones y cambio de estado
 * - TareaMedicion: medición, control de LEDs y display
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

/** @brief Período del loop principal en milisegundos */
#define LOOP_DELAY_MS 50

/** @brief Cantidad de ciclos necesarios para alcanzar 1 segundo */
#define REFRESH_TICKS (1000 / LOOP_DELAY_MS)

/** @brief Prioridad de la tarea de teclas */
#define PRIORIDAD_1 5

/** @brief Prioridad de la tarea de medición */
#define PRIORIDAD_2 4

/** @brief Memoria asignada a la tarea de teclas */
#define MEMORIA_DISPONIBLE_1 1024

/** @brief Memoria asignada a la tarea de medición */
#define MEMORIA_DISPONIBLE_2 2048

/*==================[internal data definition]===============================*/

/**
 * @brief Estados del sistema
 */
typedef enum{
    IDLE,   /**< Sistema detenido */
    MEDIR,  /**< Sistema midiendo */
    HOLD    /**< Valor congelado */
} estado_t;

/** @brief Estado actual del sistema */
static volatile estado_t estado = IDLE;

/** @brief Última distancia medida en centímetros */
static volatile uint16_t distancia = 0;

/*==================[internal functions declaration]=========================*/

/**
 * @brief Actualiza el estado de los LEDs según la distancia
 *
 * @param d Distancia medida en centímetros
 */
void actualizar_leds(uint16_t d);

/**
 * @brief Tarea encargada de la lectura de teclas
 *
 * Detecta eventos de presión mediante flanco de subida y actualiza
 * el estado del sistema.
 *
 * @param pvParameter Parámetro de la tarea (no utilizado)
 */
void TareaTeclas(void *pvParameter);

/**
 * @brief Tarea encargada de la medición y control del sistema
 *
 * Ejecuta la máquina de estados, realiza mediciones periódicas y
 * actualiza el display y los LEDs.
 *
 * @param pvParameter Parámetro de la tarea (no utilizado)
 */
void TareaMedicion(void *pvParameter);

/*==================[external functions definition]==========================*/

/**
 * @brief Punto de entrada del programa
 *
 * Inicializa los periféricos y crea las tareas bajo FreeRTOS.
 *
 * @return void
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
 * @brief Tarea de lectura de teclas
 *
 * - TEC1: alterna entre IDLE y MEDIR
 * - TEC2: alterna entre MEDIR y HOLD
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
 * @brief Tarea de medición y control
 *
 * Implementa la máquina de estados del sistema y realiza una medición
 * cada 1 segundo cuando el sistema se encuentra en estado MEDIR.
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
 * @brief Controla los LEDs según la distancia medida
 *
 * @param d Distancia en centímetros
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