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
 * Decode logic for HomeEasyV1 derived from code by Tim Hawes (2014).
 */

#include "Code.hpp"
#include "Receiver.hpp"

Code::Code() {
	valid = false;
}

Code::~Code() {

}

bool Code::isValid() const {
	return valid;
}

void Code::setValid(bool valid) {
	this->valid = valid;
}

uint8_t inline Code::messageValueAt(uint8_t index) const {
	return (message[index / 2] >> ((index & 1) ? 0 : 4)) & 0xF;
}

uint8_t inline Code::messageTrailingCount() const {
	return (messageLength & 0x03);
}

uint8_t inline Code::messageTrailingValue() const {
	return (messageValueAt(messageLength >> 2) >> (4 - messageTrailingCount()))
			& (0x7 >> (3 - messageTrailingCount()));
}

static char toHex(uint8_t value) {
	return (value < 10)
		? (char)('0' + value)
		: (char)('A' + (value - 10));
}

void Code::messageAsString(String &code, char &packedTrailingBits) const {
	for (uint_fast8_t i = 0; i < (messageLength >> 2); i++) {
		code += toHex(messageValueAt(i));
	}

	// Re-pack the trailing bits for shorter output
	packedTrailingBits = (1 << messageTrailingCount()) | messageTrailingValue();
	packedTrailingBits = (packedTrailingBits < 10)
			? (char)('0' + packedTrailingBits)
			: (char)('A' + (packedTrailingBits - 10));
}

size_t Code::printTo(Print &p) const {
	size_t n = 0;
	bool first = true;
	String code;
	char packedTrailingBits;

	messageAsString(code, packedTrailingBits);

	n += p.print("{code: \"");
	n += p.print(code);
	if (packedTrailingBits != 0) {
		n += p.print('+');
		n += p.print(packedTrailingBits);
	}
	n += p.print("\",preSync: \"");
	n += p.print(preSyncStandalone ? "standalone" : "following");
	n += p.print("\",postSync: \"");
	n += p.print(postSyncPresent ? "present" : "missing");
	n += p.print("\",duration: ");
	n += p.print(duration);

	if (preSyncTime) {
		n += p.print(",preSyncTime: ");
		n += p.print(preSyncTime);
	}

	if (postSyncTime) {
		n += p.print(",postSyncTime: ");
		n += p.print(postSyncTime);
	}

	if (zeroBitCount) {
		n += p.print(",zeroBitDuration: ");
		n += p.print(zeroBitTotalTime / zeroBitCount);
	}

	if (oneBitCount) {
		n += p.print(",oneBitDuration: ");
		n += p.print(oneBitTotalTime / oneBitCount);
	}

	if (postSyncPresent) {
		n += p.print(",decode: {");
		n += printHomeEasyV1A(first, code, p);
		n += printHomeEasyV2A(first, code, p);
		n += p.print('}');
	}

	n += p.print('}');

#if 0
	n += p.print(" # ");
	n += p.print(sizeof(Code));
#endif

	return n;
}

size_t Code::printHomeEasyV1A(bool &first, const String &code, Print &p) const {
	size_t n = 0;
	String decoded;
	int8_t group = -1;
	int8_t device = -1;
	String action;

	if (code.length() != 12 || messageTrailingCount() != 1 || messageTrailingValue() != 0x0) {
		goto out;
	}

	for (const char c : code) {
		switch (c) {
		case '5':
			decoded += '0';
			break;

		case '6':
			decoded += '1';
			break;

		case 'A':
			decoded += '2';
			break;

		default:
			goto out;
		}
	}

	if (decoded.substring(0, 4).indexOf('2') == -1) {
		group = ((uint8_t)(decoded[0] - '0') << 3)
			| ((uint8_t)(decoded[1] - '0') << 2)
			| ((uint8_t)(decoded[2] - '0') << 1)
			| (uint8_t)(decoded[3] - '0');
	}

	if (decoded.substring(4, 8).indexOf('2') == -1) {
		device = ((uint8_t)(decoded[4] - '0') << 3)
			| ((uint8_t)(decoded[5] - '0') << 2)
			| ((uint8_t)(decoded[6] - '0') << 1)
			| (uint8_t)(decoded[7] - '0');
	}

	action = decoded.substring(8);
	if (action == "0111") {
		action = "on";
	} else if (action == "0110") {
		action = "off";
	} else if (action == "0021") {
		action = "group on";
	} else if (action == "0020") {
		action = "group off";
	} else {
		action = "";
	}

	if (first) {
		first = false;
	} else {
		n += p.print(',');
	}

	n += p.print("HomeEasyV1A: {code: \"");
	n += p.print(decoded);
	n += p.print('\"');
	if (group >= 0) {
		n += p.print(",group: ");
		n += p.print(group);
	}
	if (device >= 0) {
		n += p.print(",device: ");
		n += p.print(device);
	}
	if (action != "") {
		n += p.print(",action: \"");
		n += p.print(action);
		n += p.print('\"');
	}
	n += p.print('}');

out:
	return n;
}

size_t Code::printHomeEasyV2A(bool &first, const String &code, Print &p) const {
	size_t n = 0;
	String decoded;
	uint32_t group = 0;
	uint8_t device;
	int8_t dimLevel = -1;
	String action;

	if ((code.length() != 32 && code.length() != 36) || messageTrailingCount() != 3 || (messageTrailingValue() & 0x1) != 0x0)
		goto out;

	for (const char c : code) {
		switch (c) {
		case '4':
		case '0':
			decoded += '0';
			break;

		case '5':
		case '1':
			decoded += '1';
			break;

		default:
			goto out;
		}
	}

	for (uint_fast8_t i = 0; i <= 25; i++) {
		group |= (uint32_t)(uint8_t)(decoded[25 - i] - '0') << i;
	}

	action = (decoded[26] == '1' ? "group " : "");
	action += (decoded[27] == '1' ? "on" : "off");

	device = ((uint8_t)(decoded[28] - '0') << 3)
		| ((uint8_t)(decoded[29] - '0') << 2)
		| ((uint8_t)(decoded[30] - '0') << 1)
		| (uint8_t)(decoded[31] - '0');

	if (code.length() == 36) {
		dimLevel = ((uint8_t)(decoded[32] - '0') << 3)
			| ((uint8_t)(decoded[33] - '0') << 2)
			| ((uint8_t)(decoded[34] - '0') << 1)
			| (uint8_t)(decoded[35] - '0');
	}

	if (first) {
		first = false;
	} else {
		n += p.print(',');
	}

	n += p.print("HomeEasyV2A: {code: \"");
	n += p.print(decoded);
	n += p.print("\",group: ");
	n += p.print(group);
	n += p.print(",device: ");
	n += p.print(device);
	n += p.print(",action: \"");
	n += p.print(action);
	n += p.print('\"');
	if (dimLevel != -1) {
		n += p.print(",dimLevel: ");
		n += p.print(dimLevel * 67 / 10);
	}
	n += p.print('}');

out:
	return n;
}
