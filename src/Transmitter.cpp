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
 */

#include <stdlib.h>
#include <string.h>

#include "Transmitter.hpp"

const Transmitter::Preset Transmitter::PRESETS[] = {
	{ 8800, { 0, 0 }, { 292, 980 }, 5 }, //< HomeEasyV1A
	{ 8912, { 172, 2582 } , { 220, 1304 }, 5 }, //< HomeEasyV2A
};

Transmitter::Transmitter(int pin) {
	this->pin = pin;
}

Transmitter::~Transmitter() {

}

void Transmitter::init() const {
	pinMode(pin, OUTPUT);
	digitalWrite(pin, LOW);
}

void Transmitter::processInput(Stream *console) {
	while (console->available()) {
		int c = console->read();
		if (c >= 0) {
			switch (c) {
			case '\r':
			case '\n':
				if (valid) {
					buffer[length] = 0;
					processLine(console);
				}
				buffer[0] = 0;
				length = 0;
				valid = true;
				break;

			default:
				if (length >= MAX_LENGTH) {
					valid = false;
				} else {
					buffer[length++] = c;
				}
				break;
			}
		}
	}
}

void Transmitter::processLine(Print *output) {
	bool configured = false;

	for (char *parse = buffer, *saveptr = nullptr, *token;
			(token = strtok_r(parse, ",", &saveptr)) != nullptr;
			parse = nullptr) {
		if (token[0] == '?') {
			configured = true;
		} else if (strlen(token) > 2 && token[1] == '=') {
			char *endptr = nullptr;
			unsigned long value = strtoul(&token[2], &endptr, 10);

			if (strlen(endptr) > 0) {
				continue;
			}

			switch (token[0]) {
			case '0': // bits
			case '1':
				if (value <= MAX_BIT_US) {
					bitTime[token[0] - '0'] = value;
					configured = true;
				}
				break;

			case 'H': // preamble high
				if (value <= MAX_PREAMBLE_US) {
						preambleTime[0] = value;
						configured = true;
				}
				break;

			case 'L': // preamble low
				if (value <= MAX_PREAMBLE_US) {
						preambleTime[1] = value;
						configured = true;
				}
				break;

			case 'R': // repeat
				if (value > 0 && value <= MAX_REPEAT) {
					repeat = value;
					configured = true;
				}
				break;

			case 'P': // pause
				if (value <= MAX_PAUSE_US) {
					prePauseTime = interPauseTime = postPauseTime = value;
					configured = true;
				}
				break;

			case 'B': // before
				if (value <= MAX_PAUSE_US) {
					prePauseTime = value;
					configured = true;
				}
				break;

			case 'I': // inter
				if (value <= MAX_PAUSE_US) {
					interPauseTime = value;
					configured = true;
				}
				break;

			case 'A': // after
				if (value <= MAX_PAUSE_US) {
					postPauseTime = value;
					configured = true;
				}
				break;

			case 'S': // preset
				if (value < sizeof(PRESETS) / sizeof(PRESETS[0])) {
					prePauseTime = interPauseTime = postPauseTime = PRESETS[value].pauseTime;
					preambleTime[0] = PRESETS[value].preambleTime[0];
					preambleTime[1] = PRESETS[value].preambleTime[1];
					bitTime[0] = PRESETS[value].bitTime[0];
					bitTime[1] = PRESETS[value].bitTime[1];
					repeat = PRESETS[value].repeat;
					configured = true;
				}
				break;
			}
		} else {
			Code code(token);

			if (configured) {
				outputConfiguration(output);
				configured = false;
			}

			if (code.isValid()) {
				output->print("transmit: ");
				output->println(code);

				transmit(code);
			}
		}
	}

	if (configured) {
		outputConfiguration(output);
	}
}

void Transmitter::outputConfiguration(Print *output) {
	output->print("config: {");
	output->print("prePauseTime: ");
	output->print(prePauseTime);
	output->print(",interPauseTime: ");
	output->print(interPauseTime);
	output->print(",postPauseTime: ");
	output->print(postPauseTime);
	output->print(",preambleTime: [");
	output->print(preambleTime[0]);
	output->print(',');
	output->print(preambleTime[1]);
	output->print("],zeroBitDuration: ");
	output->print(bitTime[0]);
	output->print(",oneBitDuration: ");
	output->print(bitTime[1]);
	output->print(",repeat: ");
	output->print(repeat);
	output->println('}');
}

void Transmitter::transmit(const Code &code) {
	state = LOW;
	start = micros();

	pausePin(prePauseTime);

	for (uint_fast8_t n = 0; n < repeat; n++) {
		if (n > 0) {
			pausePin(interPauseTime);
		}

		if (preambleTime[0] || preambleTime[1]) {
			togglePin(preambleTime[0]);
			togglePin(preambleTime[1]);
		}

		for (uint_fast8_t i = 0; i < code.messageLength; i++) {
			uint8_t bit = code.message[i / 8] & (0x80 >> (i & 0x7)) ? 1 : 0;

			togglePin(bitTime[bit]);
		}
	}

	pausePin(postPauseTime);
}

inline void Transmitter::togglePin(unsigned long duration) {
	noInterrupts();
	start = micros();
	digitalWrite(pin, state);
	interrupts();

	if (duration) {
		while (micros() - start < duration) {
			// Delay
		}
	}

	state = (state == LOW) ? HIGH : LOW;
	start += duration;
}

inline void Transmitter::pausePin(unsigned long duration) {
	// start has already been set
	digitalWrite(pin, LOW);

	if (duration) {
		while (micros() - start < duration) {
			// Delay
		}
	}

	state = HIGH;
	start += duration;
}
