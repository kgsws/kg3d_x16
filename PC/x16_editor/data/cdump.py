#!/usr/bin/python3
import sys
import cbor

with open(sys.argv[1], "rb") as f:
	data = f.read();

data = cbor.loads(data)
print(data)
