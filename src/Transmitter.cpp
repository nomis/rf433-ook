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


Transmitter::Transmitter(int pin) {
  this->pin = pin;
}

Transmitter::~Transmitter() {

}

void Transmitter::init() const {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
}

void Transmitter::processInput(Stream &console) {
  while (console.available()) {
    int c = console.read();
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

void Transmitter::processLine(Stream &console) {
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
      case '0':
      case '1':
        if (value <= MAX_BIT_US) {
          bitTime[token[0] - '0'] = value;
          configured = true;
        }
        break;

      case 'R':
        if (value > 0 && value <= MAX_REPEAT) {
          repeat = value;
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
      }
    } else {
      Code code(token);

      if (configured) {
        outputConfiguration(console);
        configured = false;
      }

      if (code.isValid()) {
        console.print("transmit: ");
        console.println(code);

        transmit(code);
      }
    }
  }

  if (configured) {
    outputConfiguration(console);
  }
}

void Transmitter::outputConfiguration(Stream &console) {
  console.print("config: {");
  console.print("prePauseTime: ");
  console.print(prePauseTime);
  console.print(",interPauseTime: ");
  console.print(interPauseTime);
  console.print(",postPauseTime: ");
  console.print(postPauseTime);
  console.print(",zeroBitDuration: ");
  console.print(bitTime[0]);
  console.print(",oneBitDuration: ");
  console.print(bitTime[1]);
  console.print(",repeat: ");
  console.print(repeat);
  console.println('}');
}

void Transmitter::transmit(const Code &code) {
  uint8_t state = LOW;

  digitalWrite(pin, LOW);
  togglePin(state, prePauseTime);

  for (uint_fast8_t n = 0; n < repeat; n++) {
    if (n > 0) {
      togglePin(state, interPauseTime);
    }

    if (code.preambleTime[0] || code.preambleTime[1]) {
      togglePin(state, code.preambleTime[0]);
      togglePin(state, code.preambleTime[1]);
    }

    for (uint_fast8_t i = 0; i < code.messageLength; i++) {
      uint8_t bit = code.message[i / 8] & (0x80 >> (i & 0x7)) ? 1 : 0;

      togglePin(state, bitTime[bit]);
    }
  }

  togglePin(state, postPauseTime);
  digitalWrite(pin, LOW);
}

inline void Transmitter::togglePin(uint8_t &state, unsigned long duration) {
  unsigned long start;

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
}
