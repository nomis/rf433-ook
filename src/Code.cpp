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

}

template <size_t length>
void memcpy_unrolled(char *dst, const char *src) {
	*dst = *src;
	memcpy_unrolled<length - 1>(dst + 1, src + 1);
}

template <>
void memcpy_unrolled<0>(char *dst __attribute__((unused)), const char *src __attribute__((unused))) {

}

Code::Code(const char *code,
		int_fast8_t trailingBitCount, uint_fast8_t trailingBitsValue,
		unsigned long duration, bool preSyncStandalone, bool postSyncPresent,
		unsigned long preSyncTime, unsigned long postSyncTime,
		unsigned long zeroBitTotalTime, unsigned int zeroBitCount,
		unsigned long oneBitTotalTime, unsigned int oneBitCount)
		: duration(duration), preSyncTime(preSyncTime), postSyncTime(postSyncTime),
		zeroBitTotalTime(zeroBitTotalTime), zeroBitCount(zeroBitCount),
		oneBitTotalTime(oneBitTotalTime), oneBitCount(oneBitCount),
		trailingBitCount(trailingBitCount), trailingBitsValue(trailingBitsValue),
		preSyncStandalone(preSyncStandalone), postSyncPresent(postSyncPresent) {
	memcpy_unrolled<sizeof(this->code)>(this->code, code);
}

Code::~Code() {

}

bool Code::empty() const {
	return code[0] == 0;
}

void Code::clear() {
	code[0] = 0;
}

size_t Code::printTo(Print &p) const {
	size_t n = 0;
	bool first = true;
	char packedTrailingBits;

	// Re-pack the trailing bits for shorter output
	switch (trailingBitCount) {
	case 3:
		packedTrailingBits = 0x8 | (trailingBitsValue & 0x7);
		break;

	case 2:
		packedTrailingBits = 0x4 | (trailingBitsValue & 0x3);
		break;

	case 1:
		packedTrailingBits = 0x2 | (trailingBitsValue & 0x1);
		break;

	case 0:
	default:
		packedTrailingBits = 0;
		break;
	}

	packedTrailingBits = (packedTrailingBits < 10)
			? (char)('0' + packedTrailingBits)
			: (char)('A' + (packedTrailingBits - 10));

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
		n += printHomeEasyV1A(first, p);
		n += printHomeEasyV2A(first, p);
		n += p.print('}');
	}

	n += p.print('}');

#if 0
	n += p.print(" # ");
	n += p.print(sizeof(Code));
#endif

	return n;
}

size_t Code::printHomeEasyV1A(bool &first, Print &p) const {
	size_t n = 0;
	String decoded;
	int8_t group = -1;
	int8_t device = -1;
	String action;

	if (strlen(code) != 12 || trailingBitCount != 1 || trailingBitsValue != 0) {
		goto out;
	}

	for (const char *c = code; *c; c++) {
		switch (*c) {
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

size_t Code::printHomeEasyV2A(bool &first, Print &p) const {
	size_t n = 0;
	size_t length = strlen(code);
	String decoded;
	uint32_t group = 0;
	uint8_t device;
	int8_t dimLevel = -1;
	String action;

	if ((length != 32 && length != 36) || (trailingBitCount & 0x1) != 1 || (trailingBitsValue & 0x3) != 0x0)
		goto out;

	for (const char *c = code; *c; c++) {
		switch (*c) {
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

	if (length == 36) {
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
