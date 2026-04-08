/*! @file Ej_6_guia_1.c
 * @brief Código para manejar un display mediante conversión BCD.
 *
 * @mainpage Código para manejar un display BCD mediante placa ESP32-C6
 *
 * @section genDesc General Description
 *
 * Este programa toma un número entero y lo convierte a formato BCD 
 * (Binary-Coded Decimal) para separarlo en sus dígitos individuales.
 * Luego, el algoritmo realiza una única iteración secuencial sobre cada dígito:
 * 1. Configura los 4 pines de datos (GPIO_20 a 23) con el valor BCD del 
 * dígito actual.
 * 2. Enciende el pin de selección correspondiente a esa posición 
 * (GPIO_19, 18 o 9) e inmediatamente lo apaga en la instrucción siguiente.
 *
 * @note Nota sobre el comportamiento en hardware: En su implementación actual, 
 * el programa ejecuta esta secuencia una sola vez y sin retardos (delays) 
 * entre el encendido y el apagado. Por lo tanto, sirve como prueba de 
 * concepto de la conversión lógica de datos, pero el pulso de selección es 
 * instantáneo y no generará una visualización sostenida en un display físico.
 *
 * @section hardConn Hardware Connection
 *
 * |   Display      |   EDU-CIAA / ESP32-C6 |
 * |:--------------:|:---------------------:|
 * |     Vcc        |   5V                  |
 * |     BCD1       |   GPIO_20             |
 * |     BCD2       |   GPIO_21             |
 * |     BCD3       |   GPIO_22             |
 * |     BCD4       |   GPIO_23             |
 * |     SEL1       |   GPIO_19             |
 * |     SEL2       |   GPIO_18             |
 * |     SEL3       |   GPIO_9              |
 * |     Gnd        |   GND                 |
 *
 * @section changelog Changelog
 *
 * |   Date     | Description                                                          |
 * |:----------:|:---------------------------------------------------------------------|
 * | 08/04/2026 | Creación del código para manejar un display BCD mediante placa esp32c6|
 *
 * @author Gersstner Francisco (francisco.gersstner@ingenieria.uner.edu.ar)
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gpio_mcu.h"

/*==================[macros and definitions]=================================*/

/** * @def NRO_BITS_BCD
 * @brief Cantidad de bits utilizados para representar un dígito BCD.
 */
#define NRO_BITS_BCD 4

/** * @def SALIDAS_LCD
 * @brief Cantidad de pines de selección (dígitos) del display.
 */
#define SALIDAS_LCD 3

/*==================[internal data definition]===============================*/

/** * @struct gpioConf_t
 * @brief Estructura para configurar un pin GPIO.
 * * @details Agrupa el número de pin físico y su dirección (entrada o salida). 
 * Esto facilita enormemente la inicialización y el manejo de múltiples pines 
 * mediante el uso de arreglos (arrays).
 */
typedef struct 
{
    gpio_t pin;         /*!< Número del pin GPIO a controlar */
    io_t dir;           /*!< Dirección del pin: '0' Entrada (IN); '1' Salida (OUT) */
} gpioConf_t;

/*==================[internal functions declaration]=========================*/

/**
 * @brief Convierte un número entero a un arreglo de dígitos BCD.
 * * @details Toma un valor entero y extrae cada uno de sus dígitos utilizando 
 * operaciones sucesivas de módulo y división por 10. Los dígitos se almacenan 
 * en el arreglo provisto, ordenados de forma que el dígito más significativo 
 * quede en la primera posición (índice 0).
 * * @param[in]  data       Número entero (de hasta 32 bits) que se desea convertir.
 * @param[in]  digits     Cantidad de dígitos que se extraerán del número.
 * @param[out] bcd_number Puntero al arreglo donde se guardarán los dígitos BCD extraídos.
 * * @return Ninguno (void).
 */
void convertToBcdArray(uint32_t data, uint8_t digits, uint8_t *bcd_number)
{
    // if (bcd_number == NULL) return; // Esta línea es para hacer más robusto el programa, verifica que el puntero sea válido.

    for (int i = digits - 1; i >= 0; i--) { // Recorre el array de derecha a izquierda
        bcd_number[i] = data % 10;  // Extrae el último dígito
        data = data / 10;           // Elimina ese dígito para la siguiente iteración
    }
}

/**
 * @brief Mapea un dígito BCD a un arreglo de pines GPIO.
 * * @details Realiza un desplazamiento de bits (bit shift) lógico sobre el dígito BCD 
 * para evaluar el estado de cada uno de sus 4 bits constitutivos. Dependiendo de si 
 * el bit es '1' o '0', enciende (GPIOOn) o apaga (GPIOOff) el pin de datos correspondiente.
 * * @param[in] digit Dígito BCD (valor entre 0 y 9) a representar en hardware.
 * @param[in] vec   Puntero al arreglo de configuración de pines GPIO (debe contener NRO_BITS_BCD elementos).
 * * @return Ninguno (void).
 */
void BcdToGpio(uint8_t digit, gpioConf_t *vec) 
{
    for (int i = 0; i < NRO_BITS_BCD; i++) {
        uint8_t bit = (digit >> i) & 1; // Extrae el valor del bit en la posición 'i'

        if (bit) {
            GPIOOn(vec[i].pin);
        } else {
            GPIOOff(vec[i].pin);
        }
    }
}

/**
 * @brief Convierte un número a BCD y lo envía secuencialmente al display.
 * * @details La función coordina el proceso de visualización en dos pasos. 
 * Primero, llama a `convertToBcdArray` para obtener los dígitos. Segundo, itera 
 * sobre cada dígito llamando a `BcdToGpio` para establecer los valores lógicos 
 * en los pines de datos, y genera un pulso activando y desactivando el pin 
 * de selección correspondiente.
 * * @note Debido a la ausencia de retardos (`delays`) entre el encendido y el apagado 
 * del pin de selección, esta función ejecutará un barrido instantáneo que no 
 * será visible para el ojo humano en un entorno físico.
 * * @param[in] number  Número entero que se desea mostrar en el display.
 * @param[in] digits  Cantidad de dígitos que tiene el display.
 * @param[in] gpioVec Puntero al arreglo de configuración de pines de datos (BCD).
 * @param[in] selVec  Puntero al arreglo de configuración de pines de selección (Dígitos).
 * * @return Ninguno (void).
 */
void Convertir_num_y_mostrar(uint32_t number, uint8_t digits, gpioConf_t *gpioVec, gpioConf_t *selVec)
{
    uint8_t bcd[digits];  // Array local para almacenar los dígitos separados

    // Paso 1: Convertir el número entero a formato BCD
    convertToBcdArray(number, digits, bcd);

    // Paso 2: Recorrer los dígitos y enviarlos a los pines GPIO
    for (int i = 0; i < digits; i++) {

        BcdToGpio(bcd[i], gpioVec);   // Establece los 4 bits del dígito actual
        
        GPIOOn(selVec[i].pin);        // Activa el transistor/pin de selección
        GPIOOff(selVec[i].pin);       // Desactiva el transistor/pin de selección
    }
}

/*==================[external functions definition]==========================*/

/**
 * @brief Función principal de la aplicación.
 * * @details Define localmente los arreglos de configuración para los pines de datos 
 * y de selección del display, mapeándolos a los GPIOs específicos de la placa ESP32-C6. 
 * Luego, inicializa todos los puertos configurados como salidas (OUTPUT) y ejecuta 
 * una prueba estática intentando mostrar el valor "123".
 * * @return Ninguno (void).
 */
void app_main(void)
{
    /* Configuración de los pines de datos (BCD) */
    gpioConf_t gpioVec[NRO_BITS_BCD] = {
        {GPIO_20, GPIO_OUTPUT},
        {GPIO_21, GPIO_OUTPUT},
        {GPIO_22, GPIO_OUTPUT},
        {GPIO_23, GPIO_OUTPUT}
    };

    /* Configuración de los pines de selección de dígito */
    gpioConf_t selVec[SALIDAS_LCD] = {
        {GPIO_19, GPIO_OUTPUT},
        {GPIO_18, GPIO_OUTPUT},
        {GPIO_9, GPIO_OUTPUT}
    };

    /* Inicialización del hardware para los pines de datos */
    for (int i = 0; i < NRO_BITS_BCD; i++) {
        GPIOInit(gpioVec[i].pin, gpioVec[i].dir);
    }

    /* Inicialización del hardware para los pines de selección */
    for (int i = 0; i < SALIDAS_LCD; i++) {
        GPIOInit(selVec[i].pin, selVec[i].dir);
    }

    /* Variables de prueba para el display */
    uint32_t NUMERO_A_MOSTRAR = 123;
    uint8_t CANTIDAD_DE_NUMEROS_PARA_DISPLAY = 3;

    /* Ejecución de la rutina de conversión y muestreo */
    Convertir_num_y_mostrar(NUMERO_A_MOSTRAR, CANTIDAD_DE_NUMEROS_PARA_DISPLAY, gpioVec, selVec);
}
/*==================[end of file]============================================*/