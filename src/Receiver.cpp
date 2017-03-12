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

struct ReceiverData {
	unsigned short minZeroPeriod, maxZeroPeriod;
	unsigned short minOnePeriod, maxOnePeriod;
	unsigned long minSyncPeriod, maxSyncPeriod;
	char code[Code::MAX_LENGTH + 1];
	uint_fast8_t codeLength;
	unsigned long start;
	unsigned long preSyncPeriod;
	unsigned long zeroBitPeriod, oneBitPeriod, allBitPeriod;
	unsigned int zeroBitCount, oneBitCount, allBitCount;
	int_fast8_t currentBit;
	uint_fast8_t value;
};

static inline void addBit(ReceiverData &data, uint_fast8_t bit) {
	data.value |= bit << data.currentBit;

	if (data.currentBit == 0) {
		data.code[data.codeLength++] = (data.value < 10)
				? (char)('0' + data.value)
				: (char)('A' + (data.value - 10));

		data.currentBit = 3;
		data.value = 0;
	} else {
		data.currentBit--;
	}
}

void Receiver::interruptHandler() {
	static unsigned long last = 0;
	static bool sync = false;
	static bool preSyncStandalone = true;
	static ReceiverData data;
	unsigned long now = micros();
	unsigned long duration = now - last;

retry:
	if (!sync) {
		if (duration >= SYNC_PERIODS * MIN_PERIOD_US
				&& duration <= SYNC_PERIODS * MAX_PERIOD_US) {
			unsigned long period = duration / SYNC_PERIODS;

			memset(&data, 0, sizeof(data));
			data.start = last;
			data.preSyncPeriod = period;
			data.currentBit = 3;

			data.minZeroPeriod = period * MIN_ZERO_PERIOD_PERCENT / 100;
			data.maxZeroPeriod = period * MAX_ZERO_PERIOD_PERCENT / 100;

			data.minOnePeriod = period * MIN_ONE_PERIOD_PERCENT / 100;
			data.maxOnePeriod = period * MAX_ONE_PERIOD_PERCENT / 100;

			data.minSyncPeriod = period * MIN_POST_SYNC_PERIODS;
			data.maxSyncPeriod = period * MAX_POST_SYNC_PERIODS;

			sync = true;
		}
	} else {
		unsigned long postSyncPeriod = 0;
		bool postSyncPresent = false;

		if (duration >= data.minSyncPeriod && duration <= data.maxSyncPeriod) {
			postSyncPeriod = duration / SYNC_PERIODS;
			postSyncPresent = true;
		} else {
			if (data.codeLength == sizeof(data.code) - 1) {
				// Code too long
			} else if (duration >= data.minZeroPeriod && duration <= data.maxZeroPeriod) {
				addBit(data, 0);
				data.zeroBitPeriod += duration;
				data.zeroBitCount++;
				data.allBitPeriod += duration;
				data.allBitCount++;
				goto done;
			} else if (duration >= data.minOnePeriod && duration <= data.maxOnePeriod) {
				addBit(data, 1);
				data.oneBitPeriod += duration / 3;
				data.oneBitCount++;
				data.allBitPeriod += duration / 3;
				data.allBitCount++;
				goto done;
			} else {
				// Invalid duration
			}
		}

		if (data.codeLength >= Code::MIN_LENGTH) {
			if (data.zeroBitCount > 0) {
				data.zeroBitPeriod /= data.zeroBitCount;
			}

			if (data.oneBitCount > 0) {
				data.oneBitPeriod /= data.oneBitCount;
			}

			if (data.allBitCount > 0) {
				data.allBitPeriod /= data.allBitCount;
			}

			data.code[data.codeLength] = 0;
			addCode(Code(data.code, 3 - data.currentBit, data.value, now - data.start,
				preSyncStandalone, postSyncPresent, data.preSyncPeriod, postSyncPeriod,
				data.zeroBitPeriod, data.oneBitPeriod, data.allBitPeriod));
		} else {
			// Code too short
		}

		// Restart, reusing the current sync duration
		sync = false;
		preSyncStandalone = !postSyncPresent;
		goto retry;
	}

done:
	last = now;
}

void Receiver::addCode(const Code &code) {
	receiver.codes[receiver.codeIndex] = code;
	receiver.codeIndex = (receiver.codeIndex + 1) % MAX_CODES;
}

void Receiver::printCode() {
	noInterrupts();

	for (uint_fast16_t n = 0; n < MAX_CODES; n++) {
		// This won't work if `codeIndex + n` wraps before `% MAX_CODES` is applied
		uint_fast8_t i = ((uint_fast16_t)codeIndex + n) % MAX_CODES;

		if (!codes[i].empty()) {
			Code code = codes[i];
			codes[i].clear();
			interrupts();

			SerialUSB.println(code);
			return;
		}
	}

	interrupts();
}
