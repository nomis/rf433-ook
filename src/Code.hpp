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
	Code(const char *code,
		int_fast8_t trailingBitCount, uint_fast8_t trailingBitsValue,
		unsigned long duration, bool preSyncStandalone, bool postSyncPresent,
		unsigned long preSyncTime, unsigned long postSyncTime,
		unsigned long zeroBitTotalTime, unsigned int zeroBitCount,
		unsigned long oneBitTotalTime, unsigned int oneBitCount);
	virtual ~Code();
	virtual size_t printTo(Print &p) const __attribute__((warn_unused_result));

	static constexpr uint8_t MIN_LENGTH = 12;
	static constexpr uint8_t MAX_LENGTH = 48;

protected:
	bool empty() const;
	void clear();
	size_t printHomeEasyV1A(bool &first, Print &p) const __attribute__((warn_unused_result));
	size_t printHomeEasyV2A(bool &first, Print &p) const __attribute__((warn_unused_result));

	char code[MAX_LENGTH + 1];
	unsigned long duration;
	unsigned long preSyncTime;
	unsigned long postSyncTime;
	unsigned long zeroBitTotalTime;
	unsigned int zeroBitCount;
	unsigned long oneBitTotalTime;
	unsigned int oneBitCount;
	unsigned int trailingBitCount : 2;
	unsigned int trailingBitsValue : 3;
	bool preSyncStandalone : 1;
	bool postSyncPresent : 1;
} __attribute__((packed));

#endif
