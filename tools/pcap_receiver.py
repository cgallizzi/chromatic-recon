#!/usr/bin/env python3
"""Chromatic Recon - PCAP receiver.

Reads the .pcap byte stream the PCAP tab sends over the USB serial and writes
it to a file you can open in Wireshark. It syncs on the pcap magic so any
boot/console noise before the stream is discarded.

Usage:
    python pcap_receiver.py COM10 capture.pcap
    python pcap_receiver.py COM10 capture.pcap --baud 115200

Then on the Chromatic: open the PCAP tab, pick a channel (Up/Down), press A to
start. Press Ctrl-C here to stop, then open capture.pcap in Wireshark.
(Requires: pip install pyserial)
"""
import sys
import argparse
import serial   # pip install pyserial

PCAP_MAGIC = b"\xd4\xc3\xb2\xa1"   # 0xa1b2c3d4, little-endian on the wire


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("port", help="serial port, e.g. COM10 or /dev/ttyACM0")
    ap.add_argument("outfile", help="output .pcap path")
    ap.add_argument("--baud", type=int, default=115200)
    args = ap.parse_args()

    # Open WITHOUT asserting DTR/RTS - on this board those lines are wired to
    # the ESP32 reset/boot pins, so the default pyserial open would reset/halt
    # the console. Deassert them before opening.
    ser = serial.Serial()
    ser.port = args.port
    ser.baudrate = args.baud
    ser.timeout = 1
    ser.dtr = False
    ser.rts = False
    ser.open()
    print(f"Listening on {args.port} @ {args.baud}. Waiting for pcap stream...")
    print("Start the capture on the Chromatic (PCAP tab -> A). Ctrl-C to stop.")

    synced = False
    window = b""
    total = 0
    with open(args.outfile, "wb") as f:
        try:
            while True:
                chunk = ser.read(4096)
                if not chunk:
                    continue
                if synced:
                    f.write(chunk)
                    f.flush()
                    total += len(chunk)
                    print(f"\rcaptured {total} bytes", end="", flush=True)
                else:
                    window += chunk
                    idx = window.find(PCAP_MAGIC)
                    if idx >= 0:
                        payload = window[idx:]
                        f.write(payload)
                        f.flush()
                        total += len(payload)
                        synced = True
                        print("pcap stream found - writing...")
                    else:
                        window = window[-3:]   # keep tail in case magic is split
        except KeyboardInterrupt:
            print(f"\nStopped. Wrote {total} bytes to {args.outfile}")
        finally:
            ser.close()


if __name__ == "__main__":
    main()
