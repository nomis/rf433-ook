/*
 * rf433-ook - Arduino 433MHz OOK Receiver/Transmitter
 * Copyright 2017  Simon Arlott
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <limits.h>

#include "Main.hpp"
#include "Receiver.hpp"
#include "Transmitter.hpp"

static int freeMemory() {
	extern int __heap_start, *__brkval;
	int v;
	return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}

static void checkFreeMemory() {
	static int lowMemoryWatermark = INT_MAX;
	int currentFreeMemory = freeMemory();

	if (currentFreeMemory < lowMemoryWatermark) {
		SerialUSB.print("# Free memory: ");
		if (lowMemoryWatermark != INT_MAX) {
			SerialUSB.print(lowMemoryWatermark);
			SerialUSB.print(" -> ");
		}
		SerialUSB.println(currentFreeMemory);
		lowMemoryWatermark = currentFreeMemory;
	}
}

Transmitter transmitter(TX_PIN);

void setup() {
	if (RX_ENABLED) {
		receiver.attach(RX_PIN);
	}

	if (TX_ENABLED) {
		transmitter.init();
	}

	console->begin(CONSOLE_BAUD_RATE);
}

void loop() {
	if (*console) {
		checkFreeMemory();

		if (RX_ENABLED) {
			static unsigned long last = millis();
			unsigned long now = millis();

			if (now - last >= 20) {
				last = now;
				receiver.printCode(console);
			}
		}
	}

	if (TX_ENABLED) {
		transmitter.processInput(console);
	}
}
