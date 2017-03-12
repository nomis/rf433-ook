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

//#define DEBUG_TIMING
#ifdef DEBUG_TIMING
unsigned long timeHandlerSyncStandalone = 0;
unsigned long timeHandlerSyncFollowing = 0;
unsigned long timeHandlerZero = 0;
unsigned long timeHandlerOne = 0;
unsigned long timeHandlerSampleComplete = 0;
#endif

Receiver::Receiver() {

}

Receiver::~Receiver() {

}

void Receiver::attach(int pin) {
	attachInterrupt(digitalPinToInterrupt(pin), interruptHandler, CHANGE);
}

struct ReceiverData {
	// Sampling/timing
	unsigned long bitSampleTimes[Receiver::MAX_SAMPLES];
	uint_fast8_t sampleCount;
	unsigned long zeroTime;
	unsigned long oneTime;
	bool sampleComplete;
	unsigned long minSyncPeriod, maxSyncPeriod;

	// Message
	char code[Code::MAX_LENGTH + 1];
	uint_fast8_t codeLength;
	unsigned long start;
	int_fast8_t currentBit;
	uint_fast8_t value;

	// Statistics
	unsigned long preSyncTime;
	uint_fast8_t onePeriods;
	unsigned long zeroBitTotalTime;
	unsigned int zeroBitCount;
	unsigned long oneBitTotalTime;
	unsigned int oneBitCount;
};

// These take about 4µs each, so it's better to do them every time
// than to add to the work required to process a sync event
static inline unsigned long minZeroPeriod(const ReceiverData &data) {
	return data.zeroTime * Receiver::MIN_ZERO_DURATION / Receiver::DIVISOR;
}

static inline unsigned long maxZeroPeriod(const ReceiverData &data) {
	return data.zeroTime * Receiver::MAX_ZERO_DURATION / Receiver::DIVISOR;
}

static inline unsigned long minOnePeriod(const ReceiverData &data) {
	return data.oneTime * Receiver::MIN_ONE_DURATION / Receiver::DIVISOR;
}

static inline unsigned long maxOnePeriod(const ReceiverData &data) {
	return data.oneTime * Receiver::MAX_ONE_DURATION / Receiver::DIVISOR;
}

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
		if (duration >= MIN_PRE_SYNC_US) {
			data.codeLength = 0;
			data.sampleCount = 0;
			data.zeroTime = 0;
			data.oneTime = 0;
			data.sampleComplete = false;
			data.zeroBitTotalTime = 0;
			data.zeroBitCount = 0;
			data.oneBitTotalTime = 0;
			data.oneBitCount = 0;
			data.value = 0;
			data.currentBit = 3;
			data.start = last;
			data.preSyncTime = duration;
			data.minSyncPeriod = duration * MIN_POST_SYNC_DURATION / Receiver::DIVISOR;
			data.maxSyncPeriod = duration * MAX_POST_SYNC_DURATION / Receiver::DIVISOR;
			sync = true;

#ifdef DEBUG_TIMING
			// The time taken for each of these is different, because
			// when one message follows another we do more work by
			// restarting the handler to reset and reinterpret the sync
			if (preSyncStandalone) {
				timeHandlerSyncStandalone = micros() - now;
			} else {
				timeHandlerSyncFollowing = micros() - now;
			}
#endif
		} else {
			preSyncStandalone = true;
		}
	} else {
		bool postSyncPresent = false;

		if (!data.sampleComplete) {
			if (duration < MIN_PERIOD_US) {
				// Too short
				goto error;
			} else if (data.sampleCount < Receiver::MAX_SAMPLES) {
				data.bitSampleTimes[data.sampleCount++] = duration;

				if (data.zeroTime == 0) {
					// Assume first duration is 0-bit
					data.zeroTime = duration;
				} else if (duration >= data.zeroTime * Receiver::RELATIVE_DURATION / Receiver::DIVISOR) {
					if (data.oneTime == 0) {
						// This bit looks like a 1-bit relative to the duration of the
						// currently known 0-bit, this is now the duration of the 1-bit
						data.oneTime = duration;
					} else {
						// This looks like another 1-bit, average it into the timing
						data.oneTime += duration;
						data.oneTime /= 2;
					}
				} else if (data.zeroTime >= duration * Receiver::RELATIVE_DURATION / Receiver::DIVISOR) {
					// If the currently known 0-bit looks like a 1-bit relative to
					// this bit then the previous bits were 1-bits and this is now
					// the duration of the 0-bit
					data.oneTime = data.zeroTime;
					data.zeroTime = duration;
				} else {
					// This looks like another 0-bit, average it into the timing
					data.zeroTime += duration;
					data.zeroTime /= 2;
				}

				if (data.sampleCount >= Receiver::MIN_SAMPLES && data.zeroTime != 0 && data.oneTime != 0) {
					// Both bit durations have been detected, process existing sample data
					for (uint_fast8_t i = 0; i < data.sampleCount; i++) {
						if (data.bitSampleTimes[i] <= maxZeroPeriod(data)) {
							addBit(data, 0);
							data.zeroBitTotalTime += data.bitSampleTimes[i];
							data.zeroBitCount++;
						} else if (data.bitSampleTimes[i] >= minOnePeriod(data)) {
							addBit(data, 1);
							data.oneBitTotalTime += duration;
							data.oneBitCount++;
						} else {
							// Oops
							goto error;
						}
					}

					data.sampleComplete = true;
#ifdef DEBUG_TIMING
					timeHandlerSampleComplete = micros() - now;
#endif
				}

				goto done;
			} else {
				// Unable to identify periods after all sampling
				goto error;
			}
		} else if (duration >= data.minSyncPeriod && duration <= data.maxSyncPeriod) {
			postSyncPresent = true;
		} else if (data.codeLength == sizeof(data.code) - 1) {
			// Code too long
		} else if (duration >= minZeroPeriod(data) && duration <= maxOnePeriod(data)) {
			if (duration <= maxZeroPeriod(data)) {
				addBit(data, 0);
				data.zeroBitTotalTime += duration;
				data.zeroBitCount++;
#ifdef DEBUG_TIMING
				timeHandlerZero = micros() - now;
#endif
				goto done;
			} else if (duration >= minOnePeriod(data)) {
				addBit(data, 1);
				data.oneBitTotalTime += duration;
				data.oneBitCount++;
#ifdef DEBUG_TIMING
				timeHandlerOne = micros() - now;
#endif
				goto done;
			} else {
				// Invalid duration
			}
		} else {
			// Invalid duration
		}

		if (data.codeLength >= Code::MIN_LENGTH) {
			data.code[data.codeLength] = 0;

			receiver.addCode(Code(data.code, 3 - data.currentBit, data.value,
				now - data.start, preSyncStandalone, postSyncPresent,
				data.preSyncTime, duration,
				data.zeroBitTotalTime, data.zeroBitCount,
				data.oneBitTotalTime, data.oneBitCount));
		} else {
			// Code too short
		}

error:
		// Restart, reusing the current sync duration
		sync = false;
		preSyncStandalone = !postSyncPresent;
		goto retry;
	}

done:
	last = now;
}

void Receiver::addCode(const Code &code) {
	if (!codes[codeWriteIndex].empty()) {
		// assert(codeReadIndex == codeWriteIndex);

		codeReadIndex++;
		if (codeReadIndex >= MAX_CODES) {
			codeReadIndex = 0;
		}

		// assert(!codes[codeReadIndex].empty());
	}

	codes[codeWriteIndex] = code;
	codeWriteIndex++;
	if (codeWriteIndex >= MAX_CODES) {
		codeWriteIndex = 0;
	}
}

void Receiver::printCode() {
	noInterrupts();
#ifdef DEBUG_TIMING
	unsigned long timeRead = micros();
#endif

	if (!codes[codeReadIndex].empty()) {
		Code code = codes[codeReadIndex];
		codes[codeReadIndex].clear();
		codeReadIndex++;
		if (codeReadIndex >= MAX_CODES) {
			codeReadIndex = 0;
		}
#ifdef DEBUG_TIMING
		timeRead = micros() - timeRead;

		unsigned long copyTimeHandlerSyncStandalone = timeHandlerSyncStandalone;
		unsigned long copyTimeHandlerSyncFollowing = timeHandlerSyncFollowing;
		unsigned long copyTimeHandlerZero = timeHandlerZero;
		unsigned long copyTimeHandlerOne = timeHandlerOne;
		unsigned long copyTimeHandlerSampleComplete = timeHandlerSampleComplete;
		timeHandlerSyncStandalone = 0;
		timeHandlerSyncFollowing = 0;
		timeHandlerZero = 0;
		timeHandlerOne = 0;
		timeHandlerSampleComplete = 0;
#endif
		interrupts();

		SerialUSB.println(code);

#ifdef DEBUG_TIMING
		if (copyTimeHandlerSyncStandalone != 0) {
			SerialUSB.print("# Sync (standalone) time: ");
			SerialUSB.println(copyTimeHandlerSyncStandalone);
		}
		if (copyTimeHandlerSyncFollowing != 0) {
			SerialUSB.print("# Sync (following) time: ");
			SerialUSB.println(copyTimeHandlerSyncFollowing);
		}
		if (copyTimeHandlerZero != 0) {
			SerialUSB.print("# Zero bit time: ");
			SerialUSB.println(copyTimeHandlerZero);
		}
		if (copyTimeHandlerOne != 0) {
			SerialUSB.print("# One bit time: ");
			SerialUSB.println(copyTimeHandlerOne);
		}
		if (copyTimeHandlerSampleComplete != 0) {
			SerialUSB.print("# Sample complete processing: ");
			SerialUSB.println(copyTimeHandlerSampleComplete);
		}
		SerialUSB.print("# Lookup and copy: ");
		SerialUSB.println(timeRead);
#endif

		return;
	}

	interrupts();
}
