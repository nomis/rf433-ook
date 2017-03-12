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
	void printCode();

	// This must be a power of 2 or the calculations take significantly longer
	static constexpr unsigned long DIVISOR = 1 << 3;

	static constexpr unsigned long ZERO_PERIODS = 1;
	static constexpr unsigned long ONE_PERIODS = 3;
	static constexpr unsigned long SYNC_PERIODS = 31;

	static constexpr unsigned long MIN_ZERO_DURATION = 4;
	static constexpr unsigned long MAX_ZERO_DURATION = 12;
	static constexpr unsigned long MIN_ONE_DURATION = 18;
	static constexpr unsigned long MAX_ONE_DURATION = 30;

	static constexpr unsigned long MIN_POST_SYNC_PERIODS = SYNC_PERIODS - 6;
	static constexpr unsigned long MAX_POST_SYNC_PERIODS = SYNC_PERIODS + 4;

	static constexpr unsigned long MIN_PERIOD_US = 120;

protected:
	static constexpr unsigned int MAX_CODES = 10;

	Code codes[MAX_CODES];
	uint_fast8_t codeReadIndex = 0;
	uint_fast8_t codeWriteIndex = 0;

private:
	static void interruptHandler();
	void addCode(const Code &code);
} __attribute__((packed));

extern Receiver receiver;

#endif
