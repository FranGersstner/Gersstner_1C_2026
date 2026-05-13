/**
 * @file main.c
 * @brief Medidor de distancia por ultrasonido con FreeRTOS, interrupciones y timers
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
 * El sistema utiliza:
 * - FreeRTOS (tareas)
 * - Interrupciones (teclas)
 * - Timer por hardware (temporización de medición)
 *
 * @section funcionamiento Funcionamiento
 *
 * Máquina de estados:
 *
 * - IDLE: sistema detenido
 * - MEDIR: realiza una medición cada 1 segundo
 * - HOLD: mantiene el último valor medido
 *
 * @section timing Temporización
 *
 * Se utiliza un timer de hardware que genera una interrupción cada 1 segundo.
 * Esta interrupción despierta la tarea de medición mediante notificaciones
 * de FreeRTOS, evitando el uso de delays y mejorando la precisión.
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
 * - TareaMedicion: medición, control de LEDs y display
 *
 * @section interrupts Interrupciones
 *
 * - tecla1_isr: cambia entre IDLE y MEDIR
 * - tecla2_isr: cambia entre MEDIR y HOLD
 * - FuncTimer: genera evento periódico de medición
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
#include "timer_mcu.h"
#include <stdbool.h>

#include "uart_mcu.h"
/*==================[macros and definitions]=================================*/

/** @brief Periodo de medición en microsegundos (1 segundo) */
#define PERIODO_MEDICION_US 1000000

/** @brief Prioridad de la tarea de medición */
#define PRIORIDAD_MEDICION 4

/** @brief Memoria asignada a la tarea de medición */
#define STACK_MEDICION 2048

/*==================[internal functions declaration]=========================*/

/**
 * @brief ISR de la tecla 1
 *
 * Alterna entre IDLE y MEDIR
 *
 * @param param Parámetro no utilizado
 */
void tecla1_isr(void *param);

/**
 * @brief ISR de la tecla 2
 *
 * Alterna entre MEDIR y HOLD
 *
 * @param param Parámetro no utilizado
 */
void tecla2_isr(void *param);

/**
 * @brief ISR del timer
 *
 * Se ejecuta cada 1 segundo y notifica a la tarea de medición
 *
 * @param param Parámetro no utilizado
 */
void FuncTimer(void *param);

/**
 * @brief Actualiza el estado de los LEDs según la distancia
 *
 * @param d Distancia en centímetros
 */
void actualizar_leds(uint16_t d);

/**
 * @brief Tarea principal de medición
 *
 * Espera la notificación del timer y ejecuta la máquina de estados
 *
 * @param pvParameter Parámetro no utilizado
 */
void TareaMedicion(void *pvParameter);

/**
 * @brief Callback de recepción UART
 *
 * Se ejecuta por interrupción cuando llega un dato desde la PC
 *
 * @param param Parámetro no utilizado
 */
void FuncUart(void *param);

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

/** @brief Handle de la tarea de medición */
static TaskHandle_t medicion_task_handle = NULL;

/** @brief Configuración del timer de medición */
static timer_config_t timer_medicion = {
    .timer = TIMER_A,
    .period = PERIODO_MEDICION_US,
    .func_p = FuncTimer,
    .param_p = NULL
};


/** @brief Configuración de la UART */
static serial_config_t my_uart = {
    .port = UART_PC,
    .baud_rate = 115200,
    .func_p = FuncUart,
    .param_p = NULL
};
/*==================[external functions definition]==========================*/

/**
 * @brief Función principal
 *
 * Inicializa hardware, configura interrupciones y timers,
 * y crea la tarea de medición.
 */
void app_main(void){

    /* Inicialización de periféricos */
    LedsInit();
    SwitchesInit();
    LcdItsE0803Init();
    HcSr04Init(GPIO_3, GPIO_2);

	UartInit(&my_uart);

    /* Creación de tarea */
    xTaskCreate(TareaMedicion, "Medicion", STACK_MEDICION, NULL, PRIORIDAD_MEDICION, &medicion_task_handle);

    /* Configuración de interrupciones de teclas */
    SwitchActivInt(SWITCH_1, tecla1_isr, NULL);
    SwitchActivInt(SWITCH_2, tecla2_isr, NULL);

    TimerInit(&timer_medicion);
    TimerStart(timer_medicion.timer);
}

/*==================[interrupts definition]==================================*/

void tecla1_isr(void *param){
    if(estado == IDLE)
        estado = MEDIR;
    else
        estado = IDLE;
}

void tecla2_isr(void *param){
    if(estado == MEDIR)
        estado = HOLD;
    else if(estado == HOLD)
        estado = MEDIR;
}

void FuncTimer(void *param){
    vTaskNotifyGiveFromISR(medicion_task_handle, pdFALSE);
}


void FuncUart(void *param){

    uint8_t dato;

    UartReadByte(UART_PC, &dato);

    switch(dato){

        case 'O':
        case 'o':
            if(estado == IDLE)
                estado = MEDIR;
            else
                estado = IDLE;
            break;

        case 'H':
        case 'h':
            if(estado == MEDIR)
                estado = HOLD;
            else if(estado == HOLD)
                estado = MEDIR;
            break;
    }
}

/*==================[tasks definition]=======================================*/

void TareaMedicion(void *pvParameter){

    while(true){

        /* Espera notificación del timer */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        switch(estado){

            case IDLE:
                actualizar_leds(0);
                LcdItsE0803Off();
                break;

            case MEDIR:
                distancia = HcSr04ReadDistanceInCentimeters();
                actualizar_leds(distancia);
                LcdItsE0803Write(distancia);

				printf("%03d cm\r\n", distancia);
                break;

            case HOLD:
                actualizar_leds(distancia);
                LcdItsE0803Write(distancia);
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