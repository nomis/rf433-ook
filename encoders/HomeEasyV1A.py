#!/usr/bin/env python3

import argparse
import os
import socket
import serial
import sys

def __encode(code):
	return "".join([{ "0": "5", "1": "6", "2": "A" }[x] for x in code]) + "+5"

def encode(group=None, device=None, action=None):
	code = "{:04b}{:04b}0{:s}{:01b}".format(
		group % 16,
		1 if device is None else device % 16,
		"02" if device is None else "11",
		{ "on": 1, "off": 0 }[action])
	return "S=0," + __encode(code)

if __name__ == "__main__":
	parser = argparse.ArgumentParser(description="HomeEasyV1A message encoder")
	parser.add_argument("-s", "--server", metavar="FILENAME", type=str, help="server socket to send command to")
	parser.add_argument("-p", "--port", metavar="PORT", type=str, help="serial port to send command to")
	parser.add_argument("-b", "--baud-rate", metavar="BPS", type=int, default=115200, help="serial port baud rate")
	parser.add_argument("group", metavar="GROUP", type=int, choices=range(0, 16), help="transmitter group")
	parser.add_argument("-d", "--device", metavar="DEVICE", type=int, choices=range(0, 16), help="device identifier")
	parser.add_argument(dest="action", metavar="ACTION", type=str, choices=["on", "off"], help="action to perform")
	args = parser.parse_args()
	code = encode(args.group, args.device, args.action)
	if args.port:
		with serial.Serial(args.port, args.baud_rate) as s:
			s.write(code.encode("utf-8") + b"\n")
	elif args.server:
		s = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
		s.connect(args.server)
		s.write(code + "\n")
		s.close()
	else:
		print(code)
