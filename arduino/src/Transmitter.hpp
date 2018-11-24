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

#ifndef RF433_OOK_TRANSMITTER_HPP
#define RF433_OOK_TRANSMITTER_HPP

#include <Arduino.h>

#include "Code.hpp"

class Transmitter {
public:
	Transmitter(int pin, bool silent);
	virtual ~Transmitter();
	void init() const;
	void processInput(Stream *console);

protected:
	struct Preset {
		unsigned int pauseTime;
		unsigned int preambleTime[2];
		unsigned int bitTime[2];
		unsigned int repeat;
	};

	static constexpr uint8_t MAX_LENGTH = 100;
	static constexpr unsigned long MAX_PREAMBLE_US = 10000;
	static constexpr unsigned long MAX_BIT_US = 5000;
	static constexpr unsigned long MAX_PAUSE_US = 50000;
	static constexpr unsigned long MAX_REPEAT = 20;

	static const Preset PRESETS[];

	void processLine(Print *output);
	void outputConfiguration(Print *output);
	void transmit(const Code &code);
	void togglePin(unsigned long duration);
	void pausePin(unsigned long duration);

	char buffer[MAX_LENGTH + 1] = { 0 };
	uint8_t length = 0;
	bool valid = true;

	int pin;
	bool silent;
	unsigned int prePauseTime = 10000;
	unsigned int interPauseTime = 10000;
	unsigned int postPauseTime = 10000;
	unsigned int preambleTime[2] = { 0, 0 };
	unsigned int bitTime[2] = { 300, 900 };
	unsigned int repeat = 5;

private:
	uint8_t state;
	unsigned long start;
} __attribute__((packed));

#endif
