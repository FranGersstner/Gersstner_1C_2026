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


int8_t convertToBcdArray(uint32_t data, uint8_t digits, uint8_t *bcd_number)
{
    if (bcd_number == NULL) return -1; //Esta linea es para hacer mas robusto el programa, Verifica que el puntero sea válido.

    for (int i = digits - 1; i >= 0; i--) { //Recorre el array de derecha a izquierda
        bcd_number[i] = data % 10;  // extrae el último dígito
        data = data / 10;           // elimina ese dígito
    }
	printf("Numero en BCD: ");
    for (int i = 0; i < digits; i++) {
        printf("%d", bcd_number[i]);
    }
    printf("\n");
    return 0;
}

void BcdToGpio(uint8_t digit, gpioConf_t *vec) //Función que recibe como parámetro un dígito BCD y un vector de estructura
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

void Convertir_num_y_mostrar(uint32_t number, uint8_t digits, gpioConf_t *gpioVec, gpioConf_t *selVec)
{
    uint8_t bcd[digits];  // array local

    // Paso 1: convertir a BCD
    convertToBcdArray(number, digits, bcd);

    // Paso 2: recorrer dígitos
    for (int i = 0; i < digits; i++) {

        BcdToGpio(bcd[i], gpioVec);   // manda el número
		
		GPIOOn(selVec[i].pin);
		GPIOOff(selVec[i].pin);
    }
}
/*==================[external functions definition]==========================*/


void app_main(void)
{
    gpioConf_t gpioVec[4] = {
        {GPIO_20, GPIO_OUTPUT},
        {GPIO_21, GPIO_OUTPUT},
        {GPIO_22, GPIO_OUTPUT},
        {GPIO_23, GPIO_OUTPUT}
    };

    gpioConf_t selVec[3] = {
        {GPIO_19, GPIO_OUTPUT},
        {GPIO_18, GPIO_OUTPUT},
        {GPIO_9, GPIO_OUTPUT}
    };

    // Inicializar TODO
    for (int i = 0; i < 4; i++) {
        GPIOInit(gpioVec[i].pin, gpioVec[i].dir);
    }

    for (int i = 0; i < 3; i++) {
        GPIOInit(selVec[i].pin, selVec[i].dir);
    }
	Convertir_num_y_mostrar(103, 3, gpioVec, selVec);

}
/*==================[end of file]============================================*/