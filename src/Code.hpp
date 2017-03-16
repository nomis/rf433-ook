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
	Code(const char *message);
	virtual ~Code();
	virtual size_t printTo(Print &p) const __attribute__((warn_unused_result));
	bool isValid() const;

	static constexpr uint8_t MIN_LENGTH = 12 * 4;
	static constexpr uint8_t MAX_LENGTH = 48 * 4;

	uint8_t message[(MAX_LENGTH + 7) / 8];
	uint8_t messageLength;

protected:
	void setValid(bool valid);

	uint8_t messageValueAt(uint8_t index) const;
	uint8_t messageTrailingCount() const;
	uint8_t messageTrailingValue() const;
	void messageAsString(String &code, char &packedTrailingBits) const;
	void messageCountBits(unsigned int &zeroBitCount, unsigned int &oneBitCount) const;

	size_t printHomeEasyV1A(bool &first, const String &code, Print &p) const __attribute__((warn_unused_result));
	size_t printHomeEasyV2A(bool &first, const String &code, Print &p) const __attribute__((warn_unused_result));

	unsigned long duration;
	unsigned long preSyncTime;
	unsigned long postSyncTime;
	unsigned long bitTotalTime[2];
	bool preSyncStandalone : 1;
	bool postSyncPresent : 1;
	bool valid : 1;
} __attribute__((packed));

#endif
