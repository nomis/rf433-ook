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

#ifndef RF433_OOK_CODE_HPP
#define RF433_OOK_CODE_HPP

#include <Arduino.h>

class Receiver;

class Code: public Printable {
	friend Receiver;

public:
	Code();
	Code(const char *code, unsigned long start, unsigned long stop,
		unsigned long preSyncPeriod, unsigned long postSyncPeriod,
		unsigned long zeroBitPeriod, unsigned long oneBitPeriod,
		unsigned long allBitPeriod);
	virtual ~Code();
	virtual size_t printTo(Print &p) const __attribute__((warn_unused_result));

	static constexpr uint8_t MIN_LENGTH = 12;
	static constexpr uint8_t MAX_LENGTH = 64;

protected:
	bool empty() const;
	void clear();
	size_t printHomeEasyV1(bool &first, Print &p) const __attribute__((warn_unused_result));

	char code[MAX_LENGTH + 1] = { 0 };
	unsigned long start;
	unsigned long stop;
	unsigned int preSyncPeriod;
	unsigned int postSyncPeriod;
	unsigned int zeroBitPeriod;
	unsigned int oneBitPeriod;
	unsigned int allBitPeriod;
} __attribute__((packed));

#endif
