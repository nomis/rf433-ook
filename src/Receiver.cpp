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

#include <limits.h>
#include <stdint.h>

#include "Receiver.hpp"

Receiver receiver;

//#define DEBUG_TIMING
#ifdef DEBUG_TIMING
enum HandlerTiming {
	TIMING_OTHER,
	TIMING_SYNC_STANDALONE,
	TIMING_SYNC_FOLLOWING,
	TIMING_HANDLER_ZERO,
	TIMING_HANDLER_ONE,
	TIMING_SAMPLE_COMPLETE,
	LEN_TIMING
};

uint8_t handlerTimesMin[LEN_TIMING] = { 255 };
uint8_t handlerTimesMax[LEN_TIMING] = { 0 };
#endif

Receiver::Receiver() {

}

Receiver::~Receiver() {

}

void Receiver::attach(int pin) {
	attachInterrupt(digitalPinToInterrupt(pin), interruptHandler, CHANGE);
}

struct ReceiverTiming {
	// Sampling/timing
	unsigned long bitSampleTimes[Receiver::MAX_SAMPLES];
	uint_fast8_t sampleCount;
	unsigned long zeroTime;
	unsigned long oneTime;
	bool sampleComplete;
	unsigned long minSyncPeriod, maxSyncPeriod;

	// Message
	unsigned long start;
};

// These take about 4µs each, so it's better to do them every time
// than to add to the work required to process a sync event
static inline unsigned long minZeroPeriod(const ReceiverTiming &data) {
	return data.zeroTime * Receiver::MIN_ZERO_DURATION / Receiver::DIVISOR;
}

static inline unsigned long maxZeroPeriod(const ReceiverTiming &data) {
	return data.zeroTime * Receiver::MAX_ZERO_DURATION / Receiver::DIVISOR;
}

static inline unsigned long minOnePeriod(const ReceiverTiming &data) {
	return data.oneTime * Receiver::MIN_ONE_DURATION / Receiver::DIVISOR;
}

static inline unsigned long maxOnePeriod(const ReceiverTiming &data) {
	return data.oneTime * Receiver::MAX_ONE_DURATION / Receiver::DIVISOR;
}

inline void Receiver::addBit(Code *code, bool one) {
	const uint8_t value = 0x80 >> (code->messageLength & 0x07);

	if (one) {
		code->message[code->messageLength / 8] |= value;
	} else {
		code->message[code->messageLength / 8] &= ~value;
	}

	code->messageLength++;
}

void Receiver::interruptHandler() {
	static unsigned long last = 0;
	static bool sync = false;
	static bool preSyncStandalone = true;
	static ReceiverTiming data;
	static Code *code = nullptr;
	unsigned long now = micros();
	unsigned long duration = now - last;
#ifdef DEBUG_TIMING
	HandlerTiming timingType = TIMING_OTHER;
#endif

retry:
	if (!sync) {
		if (duration >= MIN_PRE_SYNC_US) {
			code = &receiver.codes[receiver.codeWriteIndex];
			code->messageLength = 0;
			data.sampleCount = 0;
			data.zeroTime = 0;
			data.oneTime = 0;
			data.sampleComplete = false;
			code->zeroBitTotalTime = 0;
			code->zeroBitCount = 0;
			code->oneBitTotalTime = 0;
			code->oneBitCount = 0;
			data.start = last;
			code->preSyncTime = duration;
			data.minSyncPeriod = duration * MIN_POST_SYNC_DURATION / Receiver::DIVISOR;
			data.maxSyncPeriod = duration * MAX_POST_SYNC_DURATION / Receiver::DIVISOR;
			sync = true;

#ifdef DEBUG_TIMING
			// The time taken for each of these is different, because
			// when one message follows another we do more work by
			// restarting the handler to reset and reinterpret the sync
			if (preSyncStandalone) {
				timingType = TIMING_SYNC_STANDALONE;
			} else {
				timingType = TIMING_SYNC_FOLLOWING;
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
							addBit(code, 0);
							code->zeroBitTotalTime += data.bitSampleTimes[i];
							code->zeroBitCount++;
						} else if (data.bitSampleTimes[i] >= minOnePeriod(data)) {
							addBit(code, 1);
							code->oneBitTotalTime += duration;
							code->oneBitCount++;
						} else {
							// Oops
							goto error;
						}
					}

					data.sampleComplete = true;
#ifdef DEBUG_TIMING
					timingType = TIMING_SAMPLE_COMPLETE;
#endif
				}

				goto done;
			} else {
				// Unable to identify periods after all sampling
				goto error;
			}
		} else if (duration >= data.minSyncPeriod && duration <= data.maxSyncPeriod) {
			postSyncPresent = true;
		} else if (code->messageLength == sizeof(code->message) * 8) {
			// Code too long
		} else if (duration >= minZeroPeriod(data) && duration <= maxOnePeriod(data)) {
			if (duration <= maxZeroPeriod(data)) {
				addBit(code, 0);
				code->zeroBitTotalTime += duration;
				code->zeroBitCount++;
#ifdef DEBUG_TIMING
				timingType = TIMING_HANDLER_ZERO;
#endif
				goto done;
			} else if (duration >= minOnePeriod(data)) {
				addBit(code, 1);
				code->oneBitTotalTime += duration;
				code->oneBitCount++;
#ifdef DEBUG_TIMING
				timingType = TIMING_HANDLER_ONE;
#endif
				goto done;
			} else {
				// Invalid duration
			}
		} else {
			// Invalid duration
		}

		if (code->messageLength >= Code::MIN_LENGTH) {
			code->duration = now - data.start;
			code->postSyncTime = duration;
			code->preSyncStandalone = preSyncStandalone;
			code->postSyncPresent = postSyncPresent;

			code = nullptr;
			receiver.addCode();
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

#ifdef DEBUG_TIMING
	unsigned long timingValue = micros() - now;
	if (timingValue > 255) {
		timingValue = 255;
	}

	if (handlerTimesMin[timingType] > timingValue) {
		handlerTimesMin[timingType] = timingValue;
	}

	if (handlerTimesMax[timingType] < timingValue) {
		handlerTimesMax[timingType] = timingValue;
	}
#endif
}

void Receiver::addCode() {
	codes[codeWriteIndex].setValid(true);
	codeWriteIndex++;
	if (codeWriteIndex >= MAX_CODES) {
		codeWriteIndex = 0;
	}

	if (codes[codeWriteIndex].isValid()) {
		// assert(codeReadIndex == codeWriteIndex);

		codeReadIndex++;
		if (codeReadIndex >= MAX_CODES) {
			codeReadIndex = 0;
		}

		// assert(codes[codeReadIndex].isValid());
	}
}

void Receiver::printCode() {
	noInterrupts();
#ifdef DEBUG_TIMING
	unsigned long timeRead = micros();
#endif

	if (codes[codeReadIndex].isValid()) {
		Code code = codes[codeReadIndex];
		codes[codeReadIndex].setValid(false);
		codeReadIndex++;
		if (codeReadIndex >= MAX_CODES) {
			codeReadIndex = 0;
		}
#ifdef DEBUG_TIMING
		timeRead = micros() - timeRead;

		uint8_t copyHandlerTimesMin[LEN_TIMING];
		uint8_t copyHandlerTimesMax[LEN_TIMING];

		memcpy(copyHandlerTimesMin, handlerTimesMin, sizeof(handlerTimesMin));
		memcpy(copyHandlerTimesMax, handlerTimesMax, sizeof(handlerTimesMax));
		memset(handlerTimesMin, 255, sizeof(handlerTimesMin));
		memset(handlerTimesMax, 0, sizeof(handlerTimesMax));
#endif
		interrupts();

		SerialUSB.println(code);

#ifdef DEBUG_TIMING
		SerialUSB.print("timing: {read: ");
		SerialUSB.print(timeRead);

		if (copyHandlerTimesMax[TIMING_SYNC_STANDALONE] != 0) {
			SerialUSB.print(",syncStandalone: [");
			SerialUSB.print(copyHandlerTimesMin[TIMING_SYNC_STANDALONE]);
			SerialUSB.print(',');
			SerialUSB.print(copyHandlerTimesMax[TIMING_SYNC_STANDALONE]);
			SerialUSB.print(']');
		}
		if (copyHandlerTimesMax[TIMING_SYNC_FOLLOWING] != 0) {
			SerialUSB.print(",syncFollowing: [");
			SerialUSB.print(copyHandlerTimesMin[TIMING_SYNC_FOLLOWING]);
			SerialUSB.print(',');
			SerialUSB.print(copyHandlerTimesMax[TIMING_SYNC_FOLLOWING]);
			SerialUSB.print(']');
		}
		if (copyHandlerTimesMax[TIMING_HANDLER_ZERO] != 0) {
			SerialUSB.print(",zeroBit: [");
			SerialUSB.print(copyHandlerTimesMin[TIMING_HANDLER_ZERO]);
			SerialUSB.print(',');
			SerialUSB.print(copyHandlerTimesMax[TIMING_HANDLER_ZERO]);
			SerialUSB.print(']');
		}
		if (copyHandlerTimesMax[TIMING_HANDLER_ONE] != 0) {
			SerialUSB.print(",oneBit: [");
			SerialUSB.print(copyHandlerTimesMin[TIMING_HANDLER_ONE]);
			SerialUSB.print(',');
			SerialUSB.print(copyHandlerTimesMax[TIMING_HANDLER_ONE]);
			SerialUSB.print(']');
		}
		if (copyHandlerTimesMax[TIMING_SAMPLE_COMPLETE] != 0) {
			SerialUSB.print(",sampleComplete: [");
			SerialUSB.print(copyHandlerTimesMin[TIMING_SAMPLE_COMPLETE]);
			SerialUSB.print(',');
			SerialUSB.print(copyHandlerTimesMax[TIMING_SAMPLE_COMPLETE]);
			SerialUSB.print(']');
		}
		if (copyHandlerTimesMax[TIMING_OTHER] != 0) {
			SerialUSB.print(",other: [");
			SerialUSB.print(copyHandlerTimesMin[TIMING_OTHER]);
			SerialUSB.print(',');
			SerialUSB.print(copyHandlerTimesMax[TIMING_OTHER]);
			SerialUSB.print(']');
		}
		SerialUSB.println("}");
#endif

		return;
	}

	interrupts();
}
