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
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led.h"
/*==================[macros and definitions]=================================*/
/*==================[internal data definition]===============================*/

struct leds
{
    uint8_t mode;       //ON, OFF, TOGGLE
	uint8_t n_led;      //indica el número de led a controlar
	uint8_t n_ciclos;   //indica la cantidad de ciclos de encendido/apagado
	uint16_t periodo;   //indica el tiempo de cada ciclo
} my_leds; 

enum {ON, OFF, TOGGLE};
//#define OFF 0
//#define ON 1			Otro modo	
//#define TOGGLE 2

/*==================[internal functions declaration]=========================*/
void Control_Leds(struct leds *misleds) {
	if (misleds == NULL) return;

	switch (misleds->mode) {
	
	case ON:
		LedOn(misleds->n_led);
		break;
	
	case OFF:
		LedOff(misleds->n_led);
		break;
	
	case TOGGLE: {
		uint8_t i = 0;
		while (i < 2*misleds->n_ciclos) { //el 2*misleds->n_ciclos es porque estoy tomando a 1 ciclo como prender + apagar
			LedToggle(misleds->n_led);
			vTaskDelay(misleds->periodo/portTICK_PERIOD_MS);
			i++;
		}
	break;
	}

	}
}


/*==================[external functions definition]==========================*/
void app_main(void){
	LedsInit();
	my_leds.mode = TOGGLE;
	my_leds.n_ciclos = 5;
	my_leds.n_led = LED_2;
	my_leds.periodo = 500;
	Control_Leds(&my_leds);

}
/*==================[end of file]============================================*/