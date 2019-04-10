/*
 * Copyright © 2014 Kosma Moczek <kosma@cloudyourcar.com>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See the COPYING file for more details.
 */

#ifndef HX711_H
#define HX711_H


#include <string.h>
#include <stdlib.h>

#include <stdio.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <sys/time.h> /* gettimeofday() */
#include <sys/types.h> /* getpid() */
#include <unistd.h> /* getpid() */
#include <stdint.h>


//#define WEIGHT_DATA         34
//#define WEIGHT_CLK          35

#define WEIGHT_DATA         32
#define WEIGHT_CLK          33

#define DEFAULT_GAIN_VALUE 128
#define DEFAULT_TIME_VALUE 10


		
		// define clock and data pin, channel, and gain factor
		// channel selection is made by passing the appropriate gain: 128 or 64 for channel A, 32 for channel B
		// gain: 128 or 64 for channel A; channel B works with 32 gain factor only
		//HX711(byte dout, byte pd_sck, byte gain = 128);

//		HX711();


		// Allows to set the pins and gain later than in the constructor
		void hx711_begin(uint8_t gain);

		// check if HX711 is ready
		// from the datasheet: When output data is not ready for retrieval, digital output pin DOUT is high. Serial clock
		// input PD_SCK should be low. When DOUT goes to low, it indicates data is ready for retrieval.
		uint8_t hx711_is_ready();

		// set the gain factor; takes effect only after a call to read()
		// channel A can be set for a 128 or 64 gain; channel B has a fixed 32 gain
		// depending on the parameter, the channel is also set to either A or B
		void hx711_set_gain(uint8_t gain );

		// waits for the chip to be ready and returns a reading
		long hx711_read();

		// returns an average reading; times = how many times to read
		//uint8_t times = 10
		long hx711_read_average(uint8_t times);

		// returns (read_average() - OFFSET), that is the current value without the tare weight; times = how many readings to do
		//uint8_t times = 1
		float hx711_get_value(uint8_t times);

		// returns get_value() divided by hx711_SCALE, that is the raw value divided by a value obtained via calibration
		// times = how many readings to do
		//uint8_t times = 1
		float hx711_get_units(uint8_t times);

		// set the OFFSET value for tare weight; times = how many times to read the tare value
		void hx711_tare(uint8_t);

		// set the hx711_SCALE value; this value is used to convert the raw data to "human readable" data (measure units)
		//float scale = 1.f
		void hx711_set_scale(float scale);

		// get the current hx711_SCALE
		float hx711_get_scale();

		// set OFFSET, the value that's subtracted from the actual reading (tare weight)
		//long offset = 0
		void hx711_set_offset(long offset);

		// get the current OFFSET
		long hx711_get_offset();

		// puts the chip into power down mode
		void hx711_power_down();

		// wakes up the chip after power down mode
		void hx711_power_up();


#endif /* HX711_h */


