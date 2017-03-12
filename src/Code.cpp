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

#include <limits.h>

#include "Code.hpp"

Code::Code(const String &code,
		const unsigned long start, const unsigned long stop,
		const unsigned long preSyncPeriod, const unsigned long postSyncPeriod,
		const unsigned long zeroBitPeriod, const unsigned long oneBitPeriod)
		: code(code), start(start), stop(stop) {
	if (preSyncPeriod <= UINT_MAX) {
		this->preSyncPeriod = preSyncPeriod;
	}
	if (postSyncPeriod <= UINT_MAX) {
		this->postSyncPeriod = postSyncPeriod;
	}
	if (zeroBitPeriod <= UINT_MAX) {
		this->zeroBitPeriod = zeroBitPeriod;
	}
	if (oneBitPeriod <= UINT_MAX) {
		this->oneBitPeriod = oneBitPeriod;
	}
}

Code::~Code() {

}

size_t Code::printTo(Print &p) const {
	size_t n = 0;
	bool first = true;

	n += p.print("{code: \"");
	n += p.print(code);
	n += p.print("\",start: ");
	n += p.print(start);
	n += p.print(",stop: ");
	n += p.print(stop);
	n += p.print(",now: ");
	n += p.print(micros());

	if (preSyncPeriod) {
		n += p.print(",preSyncPeriod: ");
		n += p.print(preSyncPeriod);
	}

	if (postSyncPeriod) {
		n += p.print(",postSyncPeriod: ");
		n += p.print(postSyncPeriod);
	}

	if (zeroBitPeriod) {
		n += p.print(",zeroBitPeriod: ");
		n += p.print(zeroBitPeriod);
	}

	if (oneBitPeriod) {
		n += p.print(",oneBitPeriod: ");
		n += p.print(oneBitPeriod);
	}

	n += p.print(",decode: {");
	n += printHomeEasyV1(first, p);
	n += p.print("}}");

	return n;
}

size_t Code::printHomeEasyV1(bool &first, Print &p) const {
	size_t n = 0;
	String decoded;
	uint8_t group;
	uint8_t device;
	String action;

	if (code.length() != 12)
		goto out;

	for (char c : code) {
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

	group = ((uint8_t)(decoded[0] - '0') << 3)
		| ((uint8_t)(decoded[1] - '0') << 2)
		| ((uint8_t)(decoded[2] - '0') << 1)
		| (uint8_t)(decoded[3] - '0');

	device = ((uint8_t)(decoded[4] - '0') << 3)
		| ((uint8_t)(decoded[5] - '0') << 2)
		| ((uint8_t)(decoded[6] - '0') << 1)
		| (uint8_t)(decoded[7] - '0');

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
		action = "unknown (" + action + ")";
	}

	if (first) {
		first = false;
	} else {
		n += p.print(',');
	}

	n += p.print("homeEasyV1: {code: \"");
	n += p.print(decoded);
	n += p.print("\",group: ");
	n += p.print(group);
	n += p.print(",device: ");
	n += p.print(device);
	n += p.print(",action: \"");
	n += p.print(action);
	n += p.print("\"}");

out:
	return n;
}
