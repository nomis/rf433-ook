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
	TIMING_PAUSE_STANDALONE,
	TIMING_PAUSE_FOLLOWING,
	TIMING_HANDLER_ZERO,
	TIMING_HANDLER_ONE,
	TIMING_SAMPLE_ZERO,
	TIMING_SAMPLE_ONE,
	TIMING_SAMPLE_SWAP,
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
	pinMode(pin, INPUT);
	attachInterrupt(digitalPinToInterrupt(pin), interruptHandler, CHANGE);
}

struct ReceiverTiming {
	// Sampling
	unsigned long sampleMinTime[2];
	unsigned long sampleMaxTime[2];
	bool sampleComplete;

	// Timing
	unsigned long bitTime[2];
	unsigned long minPauseTime, maxPauseTime;

	// Message
	unsigned long start;
};

// These take about 4µs each, so it's better to do some of them
// every time than to add to the work required to process a pause
static inline unsigned long minZeroPeriod(const ReceiverTiming &data) {
	return data.bitTime[0] * Receiver::MIN_ZERO_DURATION / Receiver::DIVISOR;
}

static inline unsigned long maxZeroPeriod(const ReceiverTiming &data) {
	return data.bitTime[0] * Receiver::MAX_ZERO_DURATION / Receiver::DIVISOR;
}

static inline unsigned long minOnePeriod(const ReceiverTiming &data) {
	return data.bitTime[1] * Receiver::MIN_ONE_DURATION / Receiver::DIVISOR;
}

static inline unsigned long maxOnePeriod(const ReceiverTiming &data) {
	return data.bitTime[1] * Receiver::MAX_ONE_DURATION / Receiver::DIVISOR;
}

inline void Receiver::addBit(Code *code, uint8_t bit, const unsigned long &duration) {
	const uint8_t value = 0x80 >> (code->messageLength & 0x07);

	if (bit) {
		code->message[code->messageLength / 8] |= value;
	} else {
		code->message[code->messageLength / 8] &= ~value;
	}

#ifdef TRACE_BITS
	if ((duration >> 3) <= 255) {
		code->traceBitTimes[code->messageLength] = duration >> 3;
	} else {
		code->traceBitTimes[code->messageLength] = 255;
	}
#endif
	code->messageLength++;
	code->bitTotalTime[bit] += duration;
}

template <typename T>
static inline void swap(T *values) {
	T tmp = values[0];
	values[0] = values[1];
	values[1] = tmp;
}

void Receiver::interruptHandler() {
	static unsigned long last = 0;
	static bool pause = false;
	static bool prePauseStandalone = true;
	static ReceiverTiming data;
	static Code *code = nullptr;
	unsigned long now = micros();
	unsigned long duration = now - last;
#ifdef DEBUG_TIMING
	HandlerTiming timingType = TIMING_OTHER;
#endif

retry:
	if (!pause) {
		if (duration >= MIN_PRE_PAUSE_US) {
			code = &receiver.codes[receiver.codeWriteIndex];

			data.sampleMinTime[0] = ~0;
			data.sampleMinTime[1] = ~0;
			data.sampleMaxTime[0] = 0;
			data.sampleMaxTime[1] = 0;
			data.sampleComplete = false;
			code->messageLength = 0;
			code->preambleTime[0] = 0;
			code->preambleTime[1] = 0;
			data.bitTime[0] = 0;
			data.bitTime[1] = 0;
			code->bitTotalTime[0] = 0;
			code->bitTotalTime[1] = 0;
			data.start = now;
			code->prePauseTime = duration;
			data.minPauseTime = duration * MIN_POST_PAUSE_DURATION / Receiver::DIVISOR;
			data.maxPauseTime = duration * MAX_POST_PAUSE_DURATION / Receiver::DIVISOR;

			if (data.minPauseTime < MIN_PRE_PAUSE_US) {
				data.minPauseTime = MIN_PRE_PAUSE_US;
			}

			pause = true;

#ifdef DEBUG_TIMING
			// The time taken for each of these is different, because
			// when one message follows another we do more work by
			// restarting the handler to reset and reinterpret the pause
			if (prePauseStandalone) {
				timingType = TIMING_PAUSE_STANDALONE;
			} else {
				timingType = TIMING_PAUSE_FOLLOWING;
			}
#endif
		} else {
			prePauseStandalone = true;
		}
	} else {
		bool postPausePresent = false;

		// Check max length (but we can't receive the final bit)
		if (code->messageLength == Code::MAX_LENGTH - 1) {
			// Code too long
		} else if (code->preambleTime[0] == 0) {
			code->preambleTime[0] = duration;
			goto done;
		} else if (code->preambleTime[1] == 0) {
			code->preambleTime[1] = duration;
			goto done;
		} else if (!data.sampleComplete) {
			if (duration < MIN_BIT_US) {
				// Too short
				goto error;
			} else {
				bool bit;

				if (data.bitTime[0] == 0) {
					// Assume first duration is 0-bit
					data.bitTime[0] = duration;

					bit = 0;
#ifdef DEBUG_TIMING
					timingType = TIMING_SAMPLE_ZERO;
#endif
				} else if (duration >= data.bitTime[0] * Receiver::MIN_RELATIVE_DURATION / Receiver::DIVISOR) {
					if (data.bitTime[1] == 0) {
						// This bit looks like a 1-bit relative to the duration of the
						// currently known 0-bit, this is now the duration of the 1-bit
						data.bitTime[1] = duration;
					} else {
						// This looks like another 1-bit, average it into the timing
						data.bitTime[1] += duration;
						data.bitTime[1] /= 2;
					}

					bit = 1;
#ifdef DEBUG_TIMING
					timingType = TIMING_SAMPLE_ONE;
#endif
				} else if (data.bitTime[0] >= duration * Receiver::MIN_RELATIVE_DURATION / Receiver::DIVISOR) {
					// If the currently known 0-bit looks like a 1-bit relative to
					// this bit then the previous bits were 1-bits and this is now
					// the duration of the 0-bit
					data.bitTime[1] = data.bitTime[0];
					data.bitTime[0] = duration;

					swap(data.sampleMinTime);
					swap(data.sampleMaxTime);
					swap(code->bitTotalTime);

					// Invert previously stored bits
					for (uint_fast8_t i = 0; i < ((code->messageLength + 7) >> 3); i++) {
						code->message[i] = ~code->message[i];
					}

					bit = 0;
#ifdef DEBUG_TIMING
					timingType = TIMING_SAMPLE_SWAP;
#endif
				} else {
					// This looks like another 0-bit, average it into the timing
					data.bitTime[0] += duration;
					data.bitTime[0] /= 2;

					bit = 0;
#ifdef DEBUG_TIMING
					timingType = TIMING_SAMPLE_ZERO;
#endif
				}

				addBit(code, bit, duration);

				if (duration < data.sampleMinTime[bit]) {
					data.sampleMinTime[bit] = duration;
				}

				if (duration > data.sampleMaxTime[bit]) {
					data.sampleMaxTime[bit] = duration;
				}

				if (code->messageLength >= Receiver::MIN_SAMPLES && data.bitTime[0] != 0 && data.bitTime[1] != 0) {
					// Both bit durations have been detected, check the existing timings
					if (data.sampleMinTime[0] < minZeroPeriod(data) || data.sampleMaxTime[0] > maxZeroPeriod(data)
							|| data.sampleMinTime[1] < minOnePeriod(data) || data.sampleMaxTime[1] > maxOnePeriod(data)) {
						// Oops
						goto error;
					}

#ifdef DEBUG_TIMING
					timingType = TIMING_SAMPLE_COMPLETE;
#endif

					data.sampleComplete = true;
				} else if (code->messageLength >= Receiver::MAX_SAMPLES) {
					// Unable to identify periods after all sampling
					goto error;
				}

				goto done;
			}
		} else if (duration >= data.minPauseTime && duration <= data.maxPauseTime) {
			postPausePresent = true;
		} else if (duration >= minZeroPeriod(data) && duration <= maxOnePeriod(data)) {
			if (duration <= maxZeroPeriod(data)) {
				addBit(code, 0, duration);
#ifdef DEBUG_TIMING
				timingType = TIMING_HANDLER_ZERO;
#endif
				goto done;
			} else if (duration >= minOnePeriod(data)) {
				addBit(code, 1, duration);
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

		// Check min length (but we can't receive the final bit)
		if (code->messageLength >= Code::MIN_LENGTH - 1) {
			code->duration = (last - data.start) + data.bitTime[1];
			code->postPauseTime = duration;
			code->prePauseStandalone = prePauseStandalone;
			code->postPausePresent = postPausePresent;

			code = nullptr;
			receiver.addCode();
		} else {
			// Code too short
		}

error:
		// Restart, reusing the current pause duration
		pause = false;
		prePauseStandalone = !postPausePresent;
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
		codes[codeWriteIndex].setValid(false);

		// assert(codeReadIndex == codeWriteIndex);

		codeReadIndex++;
		if (codeReadIndex >= MAX_CODES) {
			codeReadIndex = 0;
		}

		// assert(codes[codeReadIndex].isValid());
	}
}

void Receiver::printCode(Stream &output) {
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

#ifdef TRACE_BITS
		output.print("# -2\t");
		output.println(code.preambleTime[0]);
		output.print("# -1\t");
		output.println(code.preambleTime[1]);

		for (uint_fast8_t i = 0; i < code.messageLength; i++) {
			output.print("# ");
			output.print(i);
			output.print('\t');
			output.print((unsigned long)code.traceBitTimes[i] << 3);
			output.print('\t');
			output.print(code.message[i / 8] >> (7 - (i & 0x07)) & 1);
			output.println();
		}
#endif

		if (code.finalise()) {
			output.print("receive: ");
			output.println(code);
		}

#ifdef DEBUG_TIMING
		output.print("timing: {read: ");
		output.print(timeRead);

		if (copyHandlerTimesMax[TIMING_PAUSE_STANDALONE] != 0) {
			output.print(",pauseStandalone: [");
			output.print(copyHandlerTimesMin[TIMING_PAUSE_STANDALONE]);
			output.print(',');
			output.print(copyHandlerTimesMax[TIMING_PAUSE_STANDALONE]);
			output.print(']');
		}
		if (copyHandlerTimesMax[TIMING_PAUSE_FOLLOWING] != 0) {
			output.print(",pauseFollowing: [");
			output.print(copyHandlerTimesMin[TIMING_PAUSE_FOLLOWING]);
			output.print(',');
			output.print(copyHandlerTimesMax[TIMING_PAUSE_FOLLOWING]);
			output.print(']');
		}
		if (copyHandlerTimesMax[TIMING_HANDLER_ZERO] != 0) {
			output.print(",zeroBit: [");
			output.print(copyHandlerTimesMin[TIMING_HANDLER_ZERO]);
			output.print(',');
			output.print(copyHandlerTimesMax[TIMING_HANDLER_ZERO]);
			output.print(']');
		}
		if (copyHandlerTimesMax[TIMING_HANDLER_ONE] != 0) {
			output.print(",oneBit: [");
			output.print(copyHandlerTimesMin[TIMING_HANDLER_ONE]);
			output.print(',');
			output.print(copyHandlerTimesMax[TIMING_HANDLER_ONE]);
			output.print(']');
		}
		if (copyHandlerTimesMax[TIMING_SAMPLE_ZERO] != 0) {
			output.print(",sampleZero: [");
			output.print(copyHandlerTimesMin[TIMING_SAMPLE_ZERO]);
			output.print(',');
			output.print(copyHandlerTimesMax[TIMING_SAMPLE_ZERO]);
			output.print(']');
		}
		if (copyHandlerTimesMax[TIMING_SAMPLE_ONE] != 0) {
			output.print(",sampleOne: [");
			output.print(copyHandlerTimesMin[TIMING_SAMPLE_ONE]);
			output.print(',');
			output.print(copyHandlerTimesMax[TIMING_SAMPLE_ONE]);
			output.print(']');
		}
		if (copyHandlerTimesMax[TIMING_SAMPLE_COMPLETE] != 0) {
			output.print(",sampleComplete: [");
			output.print(copyHandlerTimesMin[TIMING_SAMPLE_COMPLETE]);
			output.print(',');
			output.print(copyHandlerTimesMax[TIMING_SAMPLE_COMPLETE]);
			output.print(']');
		}
		if (copyHandlerTimesMax[TIMING_OTHER] != 0) {
			output.print(",other: [");
			output.print(copyHandlerTimesMin[TIMING_OTHER]);
			output.print(',');
			output.print(copyHandlerTimesMax[TIMING_OTHER]);
			output.print(']');
		}
		output.println("}");
#endif

		return;
	}

	interrupts();
}
