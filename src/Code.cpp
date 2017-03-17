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
 * Decode logic for HomeEasyV1A and HomeEasyV2A derived from code by Tim Hawes (2014).
 */

#include "Code.hpp"
#include "Receiver.hpp"
#include "Transmitter.hpp"

Code::Code() {
	valid = false;
}

Code::~Code() {

}

Code::Code(char *message) {
	unsigned int messageParts = 0;
	bool trailing = false;

	valid = false;
	messageLength = 0;
	memset(this->message, 0, sizeof(this->message));

	// Find preamble time
	preambleTime[0] = 0;
	preambleTime[1] = 0;
	for (char *parse = message, *saveptr = nullptr, *token;
			(token = strtok_r(parse, "-", &saveptr)) != nullptr;
			parse = nullptr) {
		if (messageParts < 2) {
			if (strlen(token) > 0) {
				char *endptr = nullptr;
				unsigned long value = strtoul(token, &endptr, 10);

				if (strlen(endptr) == 0) {
					if (value <= Transmitter::MAX_PREAMBLE_US) {
						preambleTime[messageParts] = value;
					}
				}
			}
		} else if (messageParts >= 3) {
			return;
		}
		message = token;
		messageParts++;
	}

	if (messageParts > 1 && (preambleTime[0] == 0 || preambleTime[1] == 0)) {
		return;
	}

	// Encode message
	for (uint_fast8_t i = 0; message[i] != 0; i++) {
		if ((message[i] >= '0' && message[i] <= '9') || (message[i] >= 'A' && message[i] <= 'F')) {
			uint8_t value = message[i] < 'A' ? (message[i] - '0') : ((message[i] - 'A') + 10);

			if (trailing) {
				if (value & 0x8) {
					if (messageLength + 3 <= MAX_LENGTH) {
						this->message[messageLength / 8] |= (value & 0x7) << 5;
						messageLength += 3;
					}
				} else if (value & 0x4) {
					if (messageLength + 2 <= MAX_LENGTH) {
						this->message[messageLength / 8] |= (value & 0x3) << 6;
						messageLength += 2;
					}
				} else if (value & 0x2) {
					if (messageLength + 1 <= MAX_LENGTH) {
						this->message[messageLength / 8] |= (value & 0x1) << 7;
						messageLength++;
					}
				}
			} else {
				if (messageLength + 4 <= MAX_LENGTH) {
					this->message[messageLength / 8] |= value << (4 - (messageLength & 0x4));
					messageLength += 4;
				}
			}
		} else if (message[i] == '+') {
			if (message[i + 1] == 0 || message[i + 2] != 0) {
				return;
			}

			trailing = true;
		} else {
			return;
		}
	}

	duration = 0;
	prePauseTime = 0;
	postPauseTime = 0;
	bitTotalTime[0] = 0;
	bitTotalTime[1] = 0;
	prePauseStandalone = true;
	postPausePresent = true;

	valid = messageLength >= MIN_LENGTH;
}

bool Code::isValid() const {
	return valid;
}

void Code::setValid(bool valid) {
	this->valid = valid;
}

enum class PreambleType {
	SHORT,
	ZERO,
	MEDIUM,
	ONE,
	LONG
};

bool Code::finalise() {
	bool hasPreamble;
	PreambleType preambleType[2];
	unsigned int zeroBitCount;
	unsigned int oneBitCount;
	unsigned long bitTime[2];

	messageCountBits(zeroBitCount, oneBitCount);

	if (zeroBitCount > 0 || oneBitCount > 0) {
		bitTime[0] = zeroBitCount > 0 ? (bitTotalTime[0] / zeroBitCount) : 0;
		bitTime[1] = oneBitCount > 0 ? (bitTotalTime[1] / oneBitCount) : 0;

		for (uint_fast8_t i = 0; i < 2; i++) {
			if (preambleTime[i] < bitTime[0] * Receiver::MIN_ZERO_DURATION / Receiver::DIVISOR) {
				preambleType[i] = PreambleType::SHORT;
			} else if (preambleTime[i] > bitTime[1] * Receiver::MAX_ONE_DURATION / Receiver::DIVISOR) {
				preambleType[i] = PreambleType::LONG;
			} else if (preambleTime[i] <= bitTime[0] * Receiver::MAX_ZERO_DURATION / Receiver::DIVISOR) {
				preambleType[i] = PreambleType::ZERO;
			} else if (preambleTime[i] >= bitTime[1] * Receiver::MIN_ONE_DURATION / Receiver::DIVISOR) {
				preambleType[i] = PreambleType::ONE;
			} else {
				preambleType[i] = PreambleType::MEDIUM;
			}
		}

		if (preambleType[0] == PreambleType::ZERO && preambleType[1] >= PreambleType::ONE) {
			if (preambleTime[1] > preambleTime[0] * Receiver::PREAMBLE_RELATIVE_DURATION / Receiver::DIVISOR) {
				hasPreamble = true;
			} else if (preambleType[1] == PreambleType::ONE) {
				hasPreamble = false;
			} else {
				// Invalid timing of non-preamble bits
				return false;
			}
		} else if ((preambleType[0] == PreambleType::ZERO || preambleType[0] == PreambleType::ONE)
				&& (preambleType[1] == PreambleType::ZERO || preambleType[1] == PreambleType::ONE)) {
			hasPreamble = false;
		} else {
			// Invalid timing of bits
			return false;
		}
	} else {
		// No data
		return false;
	}

	if (hasPreamble) {
		// Nothing to do
	} else {
		int preambleBits[2] = {
			(preambleType[0] == PreambleType::ONE) ? 1 : 0,
			(preambleType[1] == PreambleType::ONE) ? 1 : 0
		};

		// Add the preamble times to the bit total times
		bitTotalTime[preambleBits[0]] += preambleTime[0];
		bitTotalTime[preambleBits[1]] += preambleTime[1];

		// Move all the bits up
		for (uint_fast8_t i = sizeof(message) - 1; i > 0; i--) {
			message[i] = ((message[i - 1] << 6) & 0xC0) | ((message[i] >> 2) & 0x3F);
		}
		message[0] >>= 2;

		// Add the preamble bits
		message[0] |= preambleBits[0] ? 0x80 : 0;
		message[0] |= preambleBits[1] ? 0x40 : 0;
		messageLength += 2;

		// Remove preamble times
		preambleTime[0] = 0;
		preambleTime[1] = 0;
	}

	// Guess the missing final bit based on the other bit values in the message
	const uint8_t trailingValue = messageTrailingValue();
	uint8_t finalBit;
	bool values1bit[1 << 1] = { false };
	bool values2bit[1 << 2] = { false };
	bool values3bit[1 << 3] = { false };
	bool values4bit[1 << 4] = { false };

	for (uint_fast8_t i = 0; i < (messageLength >> 2); i++) {
		values1bit[messageValueAt(i) >> 3] = true;
		values2bit[messageValueAt(i) >> 2] = true;
		values3bit[messageValueAt(i) >> 1] = true;
		values4bit[messageValueAt(i)] = true;
	}

	switch (messageTrailingCount()) {
	case 3:
		if (values4bit[(trailingValue << 1) | 1]) {
			finalBit = 1;
		} else {
			finalBit = 0;
		}
		break;

	case 2:
		if (values3bit[(trailingValue << 1) | 1]) {
			finalBit = 1;
		} else {
			finalBit = 0;
		}
		break;

	case 1:
		if (values2bit[(trailingValue << 1) | 1]) {
			finalBit = 1;
		} else {
			finalBit = 0;
		}
		break;

	case 0:
		if (values1bit[(trailingValue << 1) | 1]) {
			finalBit = 1;
		} else {
			finalBit = 0;
		}
		break;
	}

	const uint8_t value = 0x80 >> (messageLength & 0x07);
	if (finalBit) {
		message[messageLength / 8] |= value;
	} else {
		message[messageLength / 8] &= ~value;
	}
	messageLength++;
	bitTotalTime[finalBit] += bitTime[finalBit];

	return true;
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
	if (messageTrailingCount() > 0) {
		packedTrailingBits = (1 << messageTrailingCount()) | messageTrailingValue();
		packedTrailingBits = (packedTrailingBits < 10)
				? (char)('0' + packedTrailingBits)
				: (char)('A' + (packedTrailingBits - 10));
	} else {
		packedTrailingBits = 0;
	}
}

void Code::messageCountBits(unsigned int &zeroBitCount, unsigned int &oneBitCount) const {
	uint8_t value;

	zeroBitCount = 0;
	oneBitCount = 0;

	for (uint_fast8_t i = 0; i < (messageLength >> 2); i++) {
		value = messageValueAt(i);

		for (uint_fast8_t bit = 0; bit < 4; bit++) {
			if (value & (1 << bit)) {
				oneBitCount++;
			} else {
				zeroBitCount++;
			}
		}
	}

	value = messageTrailingValue();
	for (uint_fast8_t bit = 0; bit < 3; bit++) {
		if (value & (1 << bit)) {
			oneBitCount++;
		} else {
			zeroBitCount++;
		}
	}
}

size_t Code::printTo(Print &p) const {
	size_t n = 0;
	bool first = true;
	String code;
	char packedTrailingBits;
	unsigned int zeroBitCount;
	unsigned int oneBitCount;

	messageAsString(code, packedTrailingBits);
	messageCountBits(zeroBitCount, oneBitCount);

	n += p.print("{code: \"");
	if (preambleTime[0] || preambleTime[1]) {
		n += p.print(preambleTime[0]);
		n += p.print('-');
		n += p.print(preambleTime[1]);
		n += p.print('-');
	}
	n += p.print(code);
	if (packedTrailingBits != 0) {
		n += p.print('+');
		n += p.print(packedTrailingBits);
	}
	n += p.print("\",prePause: \"");
	n += p.print(prePauseStandalone ? "standalone" : "following");
	n += p.print("\",postPause: \"");
	n += p.print(postPausePresent ? "present" : "missing");
	n += p.print('\"');

	if (duration) {
		n += p.print(",duration: ");
		n += p.print(duration);
	}

	if (prePauseTime) {
		n += p.print(",prePauseTime: ");
		n += p.print(prePauseTime);
	}

	if (postPauseTime) {
		n += p.print(",postPauseTime: ");
		n += p.print(postPauseTime);
	}

	if (bitTotalTime[0] && zeroBitCount) {
		n += p.print(",zeroBitDuration: ");
		n += p.print(bitTotalTime[0] / zeroBitCount);
	}

	if (bitTotalTime[1] && oneBitCount) {
		n += p.print(",oneBitDuration: ");
		n += p.print(bitTotalTime[1] / oneBitCount);
	}

	if (postPausePresent) {
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

	if (code.length() != 12) {
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
	int32_t group = 0;
	int8_t device = -1;
	int8_t dimLevel = -1;
	String action;

	if (code.length() != 32 && code.length() != 36)
		goto out;

	for (const char c : code) {
		switch (c) {
		case '1':
			decoded += '0';
			break;

		case '4':
			decoded += '1';
			break;

		case '0':
			decoded += '2';
			break;

		default:
			goto out;
		}
	}

	if (decoded.substring(0, 26).indexOf('2') == -1) {
		group = 0;
		for (uint_fast8_t i = 0; i <= 25; i++) {
			group |= (uint32_t)(uint8_t)(decoded[25 - i] - '0') << i;
		}
	}

	switch (decoded[27]) {
	case '0':
		action = "off";
		break;

	case '1':
		action = "on";
		break;

	case '2':
		action = "dim";
		break;
	}

	switch (decoded[26]) {
	case '0':
		break;

	case '1':
		action = "group " + action;
		break;

	default:
		action = "";
		break;
	}

	if (decoded.substring(28, 32).indexOf('2') == -1) {
		device = ((uint8_t)(decoded[28] - '0') << 3)
			| ((uint8_t)(decoded[29] - '0') << 2)
			| ((uint8_t)(decoded[30] - '0') << 1)
			| (uint8_t)(decoded[31] - '0');
	}

	if (code.length() == 36) {
		if (decoded.substring(32, 36).indexOf('2') == -1) {
			dimLevel = ((uint8_t)(decoded[32] - '0') << 3)
				| ((uint8_t)(decoded[33] - '0') << 2)
				| ((uint8_t)(decoded[34] - '0') << 1)
				| (uint8_t)(decoded[35] - '0');
		}
	}

	if (first) {
		first = false;
	} else {
		n += p.print(',');
	}

	n += p.print("HomeEasyV2A: {code: \"");
	n += p.print(decoded);
	n += p.print('\"');
	if (group != -1) {
		n += p.print(",group: ");
		n += p.print(group);
	}
	if (device != -1) {
		n += p.print(",device: ");
		n += p.print(device);
	}
	if (action != "") {
		n += p.print(",action: \"");
		n += p.print(action);
		n += p.print('\"');
	}
	if (dimLevel != -1) {
		n += p.print(",dimLevel: ");
		n += p.print(dimLevel * 67 / 10);
	}
	n += p.print('}');

out:
	return n;
}
