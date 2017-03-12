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

#include <Arduino.h>

#ifdef abs
# undef abs
#endif

#include <list>

#include "Code.hpp"

class Receiver {
public:
	Receiver();
	virtual ~Receiver();
	void attach(int pin);
	void printCode();

protected:
	std::list<Code> codes;

private:
	static void interruptHandler();
	static void addCode(const String &code, unsigned long start, unsigned long stop,
		unsigned long preSyncPeriod, unsigned long postSyncPeriod,
		unsigned long zeroBitPeriod, unsigned long oneBitPeriod,
		unsigned long allBitPeriod);

	static constexpr unsigned int MAX_CODES = 20;

	static constexpr unsigned long MIN_PERIOD_US = 120;
	static constexpr unsigned int SYNC_CYCLES = 31;
	static constexpr unsigned int MIN_LENGTH = 12;
	static constexpr unsigned int MAX_LENGTH = 64;
};

extern Receiver receiver;

#endif
