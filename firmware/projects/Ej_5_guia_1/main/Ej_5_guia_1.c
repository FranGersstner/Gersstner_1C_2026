/*! @mainpage Template
 *
 * @section genDesc General Description
 *
 * This section describes how the program works.
 *
 * <a href="https://drive.google.com/...">Operation Example</a>
 *
 * @section hardConn Hardware Connection
 *
 * |    Peripheral  |   ESP32   	|
 * |:--------------:|:--------------|
 * | 	PIN_X	 	| 	GPIO_X		|
 *
 *
 * @section changelog Changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 12/09/2023 | Document creation		                         |
 *
 * @author Gersstner Francisco (francisco.gersstner@ingenieria.uner.edu.ar)
 *
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gpio_mcu.h"

/*==================[macros and definitions]=================================*/

/*==================[internal data definition]===============================*/
typedef struct
{
	gpio_t pin;			/*!< GPIO pin number */
	io_t dir;			/*!< GPIO direction '0' IN;  '1' OUT*/
} gpioConf_t;


/*==================[internal functions declaration]=========================*/
void BcdToGpio(uint8_t digit, gpioConf_t *vec)
{
    for (int i = 0; i < 4; i++) {
        uint8_t bit = (digit >> i) & 1;

        if (bit) {
            GPIOOn(vec[i].pin);
        } else {
            GPIOOff(vec[i].pin);
        }
    }
}

/*==================[external functions definition]==========================*/
void app_main(void){
	gpioConf_t gpioVec[4] = {
        {GPIO_20, GPIO_OUTPUT},
        {GPIO_21, GPIO_OUTPUT},
        {GPIO_22, GPIO_OUTPUT},
        {GPIO_23, GPIO_OUTPUT}
    };

    // Inicializar GPIOs
    for (int i = 0; i < 4; i++) {
        GPIOInit(gpioVec[i].pin, gpioVec[i].dir);
    }

    uint8_t numero = 5;  // 

    BcdToGpio(numero, gpioVec);
}
/*==================[end of file]============================================*/