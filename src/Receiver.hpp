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

#ifndef RF433_OOK_RECEIVER_HPP
#define RF433_OOK_RECEIVER_HPP

#include <limits.h>
#include <Arduino.h>

#include "Code.hpp"

class Receiver {
public:
	Receiver();
	virtual ~Receiver();
	void attach(int pin);
	void printCode(Print *output);

	// All durations are specified as the numerator of a fractional number
	// with the following denominator (divisor), which must be a power of 2
	// or the calculations will take significantly longer
	static constexpr unsigned long DIVISOR = 1 << 3;

	// Relative duration compared to sampled bit periods
	static constexpr unsigned long MIN_ZERO_DURATION = 6;
	static constexpr unsigned long MAX_ZERO_DURATION = 10;
	static constexpr unsigned long MIN_ONE_DURATION = 6;
	static constexpr unsigned long MAX_ONE_DURATION = 10;

	// Relative duration of the 1-bit compared to the 0-bit for a preabmle
	static constexpr unsigned long PREAMBLE_RELATIVE_DURATION = 64;

	// Sample bits until at least this many (to get the best average duration)
	static constexpr unsigned long MIN_SAMPLES = 8;
	// Continue sampling until this many bits (if the 1-bit duration is still unknown)
	static constexpr unsigned long MAX_SAMPLES = 32;

protected:
	// Minimum relative size of a 1-bit compared to a 0-bit
	static constexpr unsigned long MIN_RELATIVE_DURATION = 14;

	// Relative duration compared to pre pause duration
	static constexpr unsigned long MIN_POST_PAUSE_DURATION = 4;
	static constexpr unsigned long MAX_POST_PAUSE_DURATION = 32;

	// Minimum time for an initial pause or processing will be ignored
	static constexpr unsigned long MIN_PAUSE_US = 4000;

	// Minimum time for a bit or processing will abort
	static constexpr unsigned long MIN_BIT_US = 100;

#ifdef TRACE_BITS
	static constexpr unsigned int MAX_CODES = 2;
#else
	static constexpr unsigned int MAX_CODES = 16;
#endif

	Code codes[MAX_CODES];
	uint_fast8_t codeReadIndex = 0;
	uint_fast8_t codeWriteIndex = 0;

private:
	static void interruptHandler();
	static void addBit(Code *code, uint8_t bit, const unsigned long &duration);
	void addCode();
} __attribute__((packed));

extern Receiver receiver;

#endif
