/**
 * @file main.c
 * @brief Implementación de un medidor de distancia por ultrasonido con FreeRTOS e interrupciones
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
 * El sistema utiliza FreeRTOS y manejo de interrupciones para las teclas.
 *
 * @section funcionamiento Funcionamiento
 *
 * Máquina de estados:
 *
 * - IDLE: sistema detenido
 * - MEDIR: realiza medición cada 1 segundo
 * - HOLD: mantiene el último valor medido
 *
 * @section timing Temporización
 *
 * Se utiliza un ciclo de 50 ms para generar un contador que permite
 * realizar una medición cada 1 segundo.
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

/** @brief Período del loop en milisegundos */
#define LOOP_DELAY_MS 50

/** @brief Cantidad de ciclos para 1 segundo */
#define REFRESH_TICKS (1000 / LOOP_DELAY_MS)

/** @brief Prioridad de la tarea */
#define PRIORIDAD_MEDICION 4

/** @brief Memoria de la tarea */
#define STACK_MEDICION 2048

/*==================[internal data definition]===============================*/

/**
 * @brief Estados del sistema
 */
typedef enum{
    IDLE,   /**< Sistema detenido */
    MEDIR,  /**< Sistema midiendo */
    HOLD    /**< Valor congelado */
} estado_t;

/** @brief Estado actual */
static volatile estado_t estado = IDLE;

/** @brief Última distancia medida */
static volatile uint16_t distancia = 0;

/*==================[internal functions declaration]=========================*/

/**
 * @brief ISR para tecla 1
 *
 * Alterna entre IDLE y MEDIR
 *
 * @param param Parámetro no utilizado
 */
void tecla1_isr(void *param);

/**
 * @brief ISR para tecla 2
 *
 * Alterna entre MEDIR y HOLD
 *
 * @param param Parámetro no utilizado
 */
void tecla2_isr(void *param);

/**
 * @brief Actualiza LEDs según distancia
 *
 * @param d Distancia en cm
 */
void actualizar_leds(uint16_t d);

/**
 * @brief Tarea principal de medición
 *
 * @param pvParameter No utilizado
 */
void TareaMedicion(void *pvParameter);

/*==================[external functions definition]==========================*/

/**
 * @brief Función principal
 *
 * Inicializa periféricos y configura interrupciones
 */
void app_main(void){

    /* Inicialización de hardware */
    LedsInit();
    SwitchesInit();
    LcdItsE0803Init();
    HcSr04Init(GPIO_3, GPIO_2);

    /* Crear tarea */
    xTaskCreate(TareaMedicion, "Medicion", STACK_MEDICION, NULL, PRIORIDAD_MEDICION, NULL);

    /* Configuración de interrupciones */
    SwitchActivInt(SWITCH_1, tecla1_isr, NULL);
    SwitchActivInt(SWITCH_2, tecla2_isr, NULL);
}

/*==================[interrupts definition]==================================*/

/**
 * @brief ISR de tecla 1
 */
void tecla1_isr(void *param){

    if(estado == IDLE)
        estado = MEDIR;
    else
        estado = IDLE;
}

/**
 * @brief ISR de tecla 2
 */
void tecla2_isr(void *param){

    if(estado == MEDIR)
        estado = HOLD;
    else if(estado == HOLD)
        estado = MEDIR;
}

/*==================[tasks definition]=======================================*/

/**
 * @brief Tarea de medición y control
 *
 * Implementa la máquina de estados y realiza mediciones periódicas
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
 *
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