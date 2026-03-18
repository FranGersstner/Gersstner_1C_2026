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
/*==================[macros and definitions]=================================*/

/*==================[internal data definition]===============================*/

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
/*==================[external functions definition]==========================*/
void app_main(void){
	uint8_t bcd[5];  // arreglo donde se guardan los dígitos

    convertToBcdArray(1234, 5, bcd);
}
/*==================[end of file]============================================*/