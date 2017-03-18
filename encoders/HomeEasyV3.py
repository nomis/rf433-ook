#!/usr/bin/env python3

import argparse
import serial
import socket
import sys

def __encode(code):
	return "".join([{ "0": "1", "1": "4", "2": "0" }[x] for x in code]) + "+5"

def encode(group=None, device=None, action=None, level=None):
	code = "{:026b}{:01b}{:s}{:04b}".format(
		group % (2 ** 26),
		1 if device is None else 0,
		{ "on": ("1" if level is None else "2"), "off": "0" }[action],
		1 if device is None else device % 16)
	if level is not None:
		code += "{:04b}".format(int(level // 6.6) % 16)
	return "S=3," + __encode(code)

if __name__ == "__main__":
	parser = argparse.ArgumentParser(description="HomeEasyV3 message encoder")
	parser.add_argument("-s", "--server", metavar="FILENAME", type=str, help="server socket to send command to")
	parser.add_argument("-p", "--port", metavar="PORT", type=str, help="serial port to send command to")
	parser.add_argument("-b", "--baud-rate", metavar="BPS", type=int, default=115200, help="serial port baud rate")
	parser.add_argument("group", metavar="GROUP", type=int, help="transmitter group")
	parser.add_argument("-d", "--device", metavar="DEVICE", type=int, choices=range(0, 16), help="device identifier")
	parser.add_argument(dest="action", metavar="ACTION", type=str, choices=["on", "off"], help="action to perform")
	parser.add_argument("-l", "--level", metavar="LEVEL", type=int, choices=range(0, 101), help="dim level")
	args = parser.parse_args()
	code = encode(args.group, args.device, args.action, args.level)
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
