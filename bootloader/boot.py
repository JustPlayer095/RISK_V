#!/usr/bin/env python3
import argparse
import struct
import sys
import time
from pathlib import Path

import serial

BL_WAITING = 0x01
BL_ACK = 0x02

BL_ERR_TOO_BIG = 0x03
BL_ERR_RECEIVE = 0x04
BL_ERR_CRC32 = 0x06
BL_ERR_WAIT_WRITE_PAGE = 0x07
BL_ERR_WAIT_ERASE_PAGE = 0x0B


def crc32_bl(data: bytes) -> int:
    # Matches bootloader/src/bl_crc32.c
    crc = 0xFFFFFFFF
    for b in data:
        crc ^= (b << 24) & 0xFFFFFFFF
        for _ in range(8):
            if crc & 0x80000000:
                crc = ((crc << 1) ^ 0x04C11DB7) & 0xFFFFFFFF
            else:
                crc = (crc << 1) & 0xFFFFFFFF
    return crc ^ 0xFFFFFFFF


def read_byte(ser: serial.Serial, timeout_s: float, what: str) -> int:
    t0 = time.time()
    while time.time() - t0 < timeout_s:
        b = ser.read(1)
        if b:
            return b[0]
    raise TimeoutError(f"Timeout waiting for {what}")


def expect(ser: serial.Serial, expected: int, timeout_s: float, what: str):
    got = read_byte(ser, timeout_s, what)
    if got != expected:
        raise RuntimeError(f"{what}: expected 0x{expected:02X}, got 0x{got:02X}")


def explain_error(code: int) -> str:
    m = {
        BL_ERR_TOO_BIG: "ERR_TOO_BIG",
        BL_ERR_RECEIVE: "ERR_RECEIVE",
        BL_ERR_CRC32: "ERR_CRC32",
        BL_ERR_WAIT_WRITE_PAGE: "ERR_WAIT_WRITE_PAGE",
        BL_ERR_WAIT_ERASE_PAGE: "ERR_WAIT_ERASE_PAGE",
    }
    return m.get(code, "UNKNOWN")


def main():
    p = argparse.ArgumentParser(description="Flash app.bin via UART bootloader")
    p.add_argument("--port", required=True, help="COM port, e.g. COM7")
    p.add_argument("--bin", required=True, help="Path to application .bin")
    p.add_argument("--baud", type=int, default=460800)
    p.add_argument("--chunk", type=int, default=256)
    p.add_argument("--timeout", type=float, default=5.0)
    p.add_argument("--wait-boot", type=float, default=15.0, help="Max time waiting for BL_WAITING")
    args = p.parse_args()

    bin_path = Path(args.bin)
    if not bin_path.exists():
        print(f"ERROR: file not found: {bin_path}", file=sys.stderr)
        sys.exit(1)

    payload = bin_path.read_bytes()
    if len(payload) == 0:
        print("ERROR: app.bin is empty", file=sys.stderr)
        sys.exit(1)

    size = len(payload)
    crc = crc32_bl(payload)
    header = struct.pack("<II", size, crc)

    print(f"BIN: {bin_path}")
    print(f"Size: {size} bytes")
    print(f"CRC32(BL): 0x{crc:08X}")

    try:
        with serial.Serial(args.port, args.baud, timeout=0.1) as ser:
            print("Waiting bootloader invitation (0x01)...")
            expect(ser, BL_WAITING, args.wait_boot, "BL_WAITING")

            print("Sending header <size, crc32>...")
            ser.write(header)
            ser.flush()

            expect(ser, BL_ACK, args.timeout, "Header ACK")
            print("Header ACK OK")

            sent = 0
            while sent < size:
                end = min(sent + args.chunk, size)
                ser.write(payload[sent:end])
                ser.flush()
                expect(ser, BL_ACK, args.timeout, f"Chunk ACK {end}/{size}")
                sent = end
                print(f"Sent {sent}/{size}")

            print("Waiting final status...")
            final = read_byte(ser, args.timeout, "final status")
            if final == BL_ACK:
                print("SUCCESS: app flashed via bootloader")
                return

            print(f"ERROR: bootloader returned 0x{final:02X} ({explain_error(final)})", file=sys.stderr)
            sys.exit(2)

    except Exception as e:
        print(f"ERROR: {e}", file=sys.stderr)
        sys.exit(3)


if __name__ == "__main__":
    main()