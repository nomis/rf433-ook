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

#ifndef RF433_OOK_MAIN_HPP
#define RF433_OOK_MAIN_HPP

#include <Arduino.h>

// Input/output
#ifdef ARDUINO_AVR_MICRO
constexpr auto *console = &SerialUSB;
#else
constexpr auto *console = &Serial;
#endif
constexpr unsigned long CONSOLE_BAUD_RATE = 115200;

// 433MHz OOK
constexpr bool RX_ENABLED = true;
constexpr int RX_PIN = 2;
constexpr bool TX_ENABLED = true;
constexpr bool TX_SILENT = false;
constexpr int TX_PIN = 10;

#endif
