/**
 * @file sensor_conversion_functions.h
 * */

#ifndef SENSOR_CONVERSION_FUNCTIONS_H_
#define SENSOR_CONVERSION_FUNCTIONS_H_

// Standard C includes:
#include <stdio.h>      // For printf

/**
* @brief Convert raw ADC value to temperature
* @param adc_input raw ADC value
*/
static float getTemperatureValue(uint16_t adc_input) {

	printf("\r\nadc_input = %d", adc_input);
	float temp = 222.2*((float)adc_input/4096)-61.111;
	printf("\r\nConverts to Temperature...");

	return temp;
}

/**
* @brief Convert raw ADC value to humidity
* @param adc_input raw ADC value
*/
static float getHumidityValue(uint16_t adc_input) {

	printf("\r\nadc_input = %d", adc_input);
	float humidity = (190.6*((float)adc_input/4096)-40.2-128);
	printf("\r\nConverts to Humidity...");

	return humidity;
}

/**
* @brief Convert raw ADC value to pH level
* @param adc_input raw ADC value
*/
static float getpHlevel(uint16_t adc_input) {

	int internal_temp = cc2538_temp_sensor.value(CC2538_SENSORS_VALUE_TYPE_CONVERTED)/1000; /**< use internal temperature for better accuracy */

	printf("\r\nadc_input = %d", adc_input);
	float ph_level = (2.5-((float)adc_input*5/4096))/(0.257179+0.000941468*internal_temp);
	printf("\r\nConverts to pH level...");

	return ph_level;
}

/**
* @brief Convert raw ADC value to soil moisture for sensor #1
* @param adc_input raw ADC value
*/
static float getSoilMoisture1(uint16_t adc_input) {
	float moisture_level = (1-((float)adc_input-592)/(907-592))*100;

	if (moisture_level < 0){
		moisture_level = 0;
	}
	else if (moisture_level > 100){
		moisture_level = 100;
	}

	printf("\r\nConverts to Soil Moisture...");

	return moisture_level;
}

/**
* @brief Convert raw ADC value to soil moisture for sensor #2
* @param adc_input raw ADC value
*/
static float getSoilMoisture2(uint16_t adc_input) {
	float moisture_level = (1-((float)adc_input-621)/(930-621))*100;

	if (moisture_level < 0){
		moisture_level = 0;
	}
	else if (moisture_level > 100){
		moisture_level = 100;
	}

	printf("\r\nConverts to Soil Moisture...");

	return moisture_level;
}

/**
* @brief Convert raw ADC value to light in lux
* @param adc_input raw ADC value
*/
static int getLightSensorValue(uint16_t adc_input) {
	int lux = 1.2179*(adc_input*3.3/4096)*200+36.996;
	//Return the value of the light with maximum value equal to 1000
	if (lux > 1000) {
		lux = 1000;
	}

	printf("\r\nConverts to Lux...");

	return lux;
}

#endif


