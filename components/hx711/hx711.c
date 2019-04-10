/*
 * Copyright © 2014 Kosma Moczek <kosma@cloudyourcar.com>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See the COPYING file for more details.
 */

#include "hx711.h"



#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include <stdio.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <sys/time.h> /* gettimeofday() */
#include <sys/types.h> /* getpid() */
#include <unistd.h> /* getpid() */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/FreeRTOSConfig.h" 

#include "esp_err.h"
#include "esp_log.h"


#include "esp_system.h"

#include "soc/rtc.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/rtc_io_reg.h"

#include "driver/gpio.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"



uint8_t GAIN ;		// amplification factor
long hx711_OFFSET = 0;	// used for tare weight
float hx711_SCALE = 1;	// used to return weight in grams, kg, ounces, whatever


void hx711_begin(uint8_t gain) {
	
	uint8_t lgain=0;
	
	gpio_pad_select_gpio(WEIGHT_DATA);
    gpio_pad_select_gpio(WEIGHT_CLK);
    
    gpio_set_direction(WEIGHT_DATA, GPIO_MODE_INPUT);
    gpio_set_direction(WEIGHT_CLK, GPIO_MODE_OUTPUT);
    
    lgain=gain;
    if(gain==0)
     lgain=DEFAULT_GAIN_VALUE;

	hx711_set_gain(lgain);
}

uint8_t hx711_is_ready() {

	if(gpio_get_level(WEIGHT_DATA)==0)
		return 1;
	else
		return 0;
	
}

void hx711_set_gain(uint8_t gain) {

if(gain==0)
     gain=DEFAULT_GAIN_VALUE;
     
     
	switch (gain) {
		case 128:		// channel A, gain factor 128
			GAIN = 1;
			break;
		case 64:		// channel A, gain factor 64
			GAIN = 3;
			break;
		case 32:		// channel B, gain factor 32
			GAIN = 2;
			break;
	}

	hx711_power_up();
	hx711_read();
}

long hx711_read() {
	/* wait for the chip to become ready
	while (!is_ready()) {
		// Will do nothing on Arduino but prevent resets of ESP8266 (Watchdog Issue)
		yield();
	}*/
	long int  final_weight = 0;	
	uint32_t weight = 0;
  	int index = 0;
  	unsigned int i;
	
	
	
	//rtc_clk_cpu_freq_set(RTC_CPU_FREQ_80M);
	// pulse the clock pin 24 times to read the data

	for (index = 0; index < 24; index ++)
	{
		weight = weight | (gpio_get_level(WEIGHT_DATA) << index);
	}

	// set the channel and the gain factor for the next reading using the clock pin
	for (i = 0; i < GAIN; i++) 
	{
		gpio_set_level(WEIGHT_CLK, 0);
		gpio_set_level(WEIGHT_CLK, 1);
	}
	//rtc_clk_cpu_freq_set(RTC_CPU_FREQ_160M);

	// Replicate the most significant bit to pad out a 32-bit signed integer
	printf("\nweight=%u\n",weight);
	if (weight & 0x800000)
	{
		weight=(weight | 0xFF000000);	
	}
	else
	{
		weight=(weight & 0x00FFFFFF);
	}

	// Construct a 32-bit signed integer
    final_weight=(long int)weight;
    
    
	return final_weight;
}

long  hx711_read_average(uint8_t times) {
	long sum = 0;
	uint8_t i=0;
	uint8_t localtimes=0;
	
	localtimes=times;
	if(times==0)
     	localtimes=DEFAULT_TIME_VALUE;
	
	
	for (i = 0; i < localtimes; i++) {
		sum += hx711_read();
	}
	return sum / localtimes;
}

float hx711_get_value(uint8_t times) {

if(times==0)
    return hx711_read_average(1) - hx711_OFFSET;
else
	return hx711_read_average(times) - hx711_OFFSET;
}

float hx711_get_units(uint8_t times) {

	if(times==0)
     	return hx711_get_value(1) / hx711_SCALE;
    else
     	return hx711_get_value(times) / hx711_SCALE;
}

void hx711_tare(uint8_t times) {
	float sum = hx711_read_average(times);
	hx711_set_offset(sum);
}

void hx711_set_scale(float scale) {

if(scale==0)
     hx711_SCALE=1.0;
else
     hx711_SCALE = scale;
}

float hx711_get_scale() {
	return hx711_SCALE;
}

void hx711_set_offset(long offset) {
     
	hx711_OFFSET = offset;
}

long hx711_get_offset() {
	return hx711_OFFSET;
}

void hx711_power_down() {

		gpio_set_level(WEIGHT_CLK, 1);
		gpio_set_level(WEIGHT_CLK, 0);
}

void hx711_power_up() {
		gpio_set_level(WEIGHT_CLK, 1);
}