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
 *
 * Interrupt handler protocol decode derived from:
 * RemoteSwitch library v2.0.0 made by Randy Simons http://randysimons.nl
 *
 * Copyright 2010 Randy Simons. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright notice, this list
 *       of conditions and the following disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY RANDY SIMONS ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL RANDY SIMONS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those of the
 * authors and should not be interpreted as representing official policies, either expressed
 * or implied, of Randy Simons.
 */

#include <stdint.h>

#include "Receiver.hpp"

Receiver receiver;

Receiver::Receiver() {

}

Receiver::~Receiver() {

}

void Receiver::attach(int pin) {
	attachInterrupt(digitalPinToInterrupt(pin), interruptHandler, CHANGE);
}

static void addBit(String &code, int_fast8_t &currentBit, uint_fast8_t &value, uint_fast8_t bit) {
	value |= bit << currentBit;

	if (currentBit == 0) {
		code += (value < 10) ? (char)('0' + value) : (char)('A' + (value - 10));

		currentBit = 3;
		value = 0;
	} else {
		currentBit--;
	}
}

void Receiver::interruptHandler() {
	static unsigned long last = 0;
	static bool sync = false;
	static unsigned long min1Period, max1Period;
	static unsigned long min3Period, max3Period;
	static unsigned long minSyncPeriod, maxSyncPeriod;
	static String code;
	static unsigned long start, stop;
	static unsigned long preSyncPeriod, postSyncPeriod;
	static unsigned long zeroBitPeriod, oneBitPeriod;
	static unsigned int zeroBitCount, oneBitCount;
	static int_fast8_t currentBit;
	static uint_fast8_t value;
	unsigned long now = micros();
	unsigned long duration = now - last;

retry:
	if (!sync) {
		if (duration >= SYNC_CYCLES * MIN_PERIOD_US) {
			unsigned long period = duration / SYNC_CYCLES;

			start = last;
			code = "";
			preSyncPeriod = period;
			zeroBitPeriod = oneBitPeriod = 0;
			zeroBitCount = oneBitCount = 0;
			currentBit = 3;
			value = 0;
			sync = true;

			min1Period = period * 4 / 10;
			max1Period = period * 16 / 10;
			min3Period = period * 23 / 10;
			max3Period = period * 37 / 10;
			minSyncPeriod = period * (SYNC_CYCLES - 6);
			maxSyncPeriod = period * (SYNC_CYCLES + 4);
		}
	} else {
		if (duration >= minSyncPeriod && duration <= maxSyncPeriod) {
			// Must receive an extra 0 bit immediately before sync
			if (currentBit == 2 && value == 0) {
				postSyncPeriod = duration / SYNC_CYCLES;
				stop = now;

				if (code.length() >= MIN_LENGTH) {
					if (zeroBitCount > 0) {
						zeroBitPeriod /= zeroBitCount;
					}

					if (oneBitCount > 0) {
						oneBitPeriod /= oneBitCount;
					}

					addCode(code, start, stop, preSyncPeriod, postSyncPeriod,
						zeroBitPeriod, oneBitPeriod);
				} else {
					// Code too short
				}
			} else {
				// Sync too early
			}
		} else {
			if (code.length() == MAX_LENGTH) {
				// Code too long
			} else if (duration >= min1Period && duration <= max1Period) {
				addBit(code, currentBit, value, 0);
				zeroBitPeriod += duration;
				zeroBitCount++;
				goto done;
			} else if (duration >= min3Period && duration <= max3Period) {
				addBit(code, currentBit, value, 1);
				oneBitPeriod += duration / 3;
				oneBitCount++;
				goto done;
			} else {
				// Invalid duration
			}
		}

		// Restart, reusing the current sync duration
		sync = false;
		goto retry;
	}

done:
	last = now;
}

void Receiver::addCode(const String &code,
		unsigned long start, unsigned long stop,
		unsigned long preSyncPeriod, unsigned long postSyncPeriod,
		unsigned long zeroBitPeriod, unsigned long oneBitPeriod) {
	if (receiver.codes.size() == MAX_CODES) {
		receiver.codes.pop_front();
	}

	receiver.codes.push_back({code, start, stop,
		preSyncPeriod, postSyncPeriod, zeroBitPeriod, oneBitPeriod});
}

void Receiver::printCode() {
	noInterrupts();
	if (!receiver.codes.empty()) {
		Code code = receiver.codes.front();
		receiver.codes.pop_front();
		interrupts();

		SerialUSB.println(code);
	} else {
		interrupts();
	}
}
