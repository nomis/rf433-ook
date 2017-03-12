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

protected:
	static constexpr unsigned int MAX_CODES = 10;

	Code codes[MAX_CODES];
	uint_fast8_t codeIndex = 0; /// The next code to be written

private:
	static void interruptHandler();
	static void addCode(const Code &code);

	// 1 period = 0-bit
	static constexpr unsigned long MIN_ZERO_PERIOD_PERCENT = 40;
	static constexpr unsigned long MAX_ZERO_PERIOD_PERCENT = 160;

	// 3 periods = 1-bit
	static constexpr unsigned long MIN_ONE_PERIOD_PERCENT = 230;
	static constexpr unsigned long MAX_ONE_PERIOD_PERCENT = 370;

	static constexpr unsigned long SYNC_PERIODS = 31;
	static constexpr unsigned long MIN_POST_SYNC_PERIODS = SYNC_PERIODS - 6;
	static constexpr unsigned long MAX_POST_SYNC_PERIODS = SYNC_PERIODS + 4;

	static constexpr unsigned long MIN_PERIOD_US = 120;
	static constexpr unsigned long MAX_PERIOD_US = UINT_MAX * 100UL / MAX_ONE_PERIOD_PERCENT; // Must be able to fit duration calculation into maxOnePeriod
} __attribute__((packed));

extern Receiver receiver;

#endif
