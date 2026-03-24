#!/usr/bin/env python3
import argparse
import re
import struct
import sys
import time
from pathlib import Path

import serial
from serial.tools import list_ports

BL_WAITING = 0x01
BL_ACK = 0x02

BL_ERR_TOO_BIG = 0x03
BL_ERR_RECEIVE = 0x04
BL_ERR_CRC32 = 0x05
BL_ERR_WAIT_WRITE_PAGE = 0x06
BL_ERR_WAIT_ERASE_PAGE = 0x07
CH340_VID_PID = {(0x1A86, 0x7523), (0x1A86, 0x5523)}
KNOWN_USB_UART_VID_PID = {
    # WCH CH340/CH341/CH910x
    (0x1A86, 0x7523), (0x1A86, 0x5523), (0x1A86, 0x55D4),
    # Silicon Labs CP210x
    (0x10C4, 0xEA60),
    # FTDI FT232/FT2232
    (0x0403, 0x6001), (0x0403, 0x6010),
    # Prolific PL2303
    (0x067B, 0x2303),
    # Some HL-340 variants
    (0x4348, 0x5523),
}
USB_UART_HINTS = (
    "ch340", "ch341", "ch910", "cp210", "ftdi", "ft232",
    "pl2303", "usb serial", "usb-serial", "uart", "cdc",
)


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
    t0 = time.time()
    while time.time() - t0 < timeout_s:
        got = read_byte(ser, timeout_s, what)
        # Bootloader can keep sending WAITING while entering RX state.
        # Ignore extra WAITING bytes when we already expect ACK.
        if expected == BL_ACK and got == BL_WAITING:
            continue
        if got == expected:
            return
        raise RuntimeError(f"{what}: expected 0x{expected:02X}, got 0x{got:02X}")
    raise TimeoutError(f"Timeout waiting for {what}")


def explain_error(code: int) -> str:
    m = {
        BL_ERR_TOO_BIG: "ERR_TOO_BIG",
        BL_ERR_RECEIVE: "ERR_RECEIVE",
        BL_ERR_CRC32: "ERR_CRC32",
        BL_ERR_WAIT_WRITE_PAGE: "ERR_WAIT_WRITE_PAGE",
        BL_ERR_WAIT_ERASE_PAGE: "ERR_WAIT_ERASE_PAGE",
    }
    return m.get(code, "UNKNOWN")


def _port_sort_key(port_info):
    dev = (port_info.device or "").upper()
    if dev.startswith("COM"):
        try:
            return (0, int(dev[3:]))
        except ValueError:
            return (1, dev)
    return (1, dev)


def _port_score(port_info) -> int:
    score = 0
    if (port_info.vid, port_info.pid) in CH340_VID_PID:
        score += 200
    if (port_info.vid, port_info.pid) in KNOWN_USB_UART_VID_PID:
        score += 150
    if port_info.vid is not None and port_info.pid is not None:
        score += 50

    text = " ".join(
        [
            port_info.description or "",
            port_info.manufacturer or "",
            port_info.product or "",
            port_info.hwid or "",
            port_info.interface or "",
        ]
    ).lower()
    if any(h in text for h in USB_UART_HINTS):
        score += 80
    if "bluetooth" in text:
        score -= 100
    if "debug" in text or "jtag" in text:
        score -= 40
    return score


def _port_line(port_info) -> str:
    return (
        f"{port_info.device}: "
        f"{port_info.description or 'n/a'} | "
        f"VID:PID=0x{(port_info.vid or 0):04X}:0x{(port_info.pid or 0):04X} | "
        f"score={_port_score(port_info)}"
    )


def list_serial_ports() -> None:
    ports = list(list_ports.comports())
    if not ports:
        print("No serial ports found.")
        return
    ports.sort(key=_port_sort_key)
    print("Available serial ports:")
    for p in ports:
        print(" - " + _port_line(p))


def _filter_ports(ports, pattern: str):
    rx = re.compile(pattern, re.IGNORECASE)
    out = []
    for p in ports:
        text = " ".join(
            [p.device or "", p.description or "", p.manufacturer or "", p.product or "", p.hwid or ""]
        )
        if rx.search(text):
            out.append(p)
    return out


def detect_serial_port(pattern: str = "") -> str:
    ports = list(list_ports.comports())
    if not ports:
        raise RuntimeError("No serial ports found")

    if pattern:
        ports = _filter_ports(ports, pattern)
        if not ports:
            raise RuntimeError(
                f"No serial ports matched --port-filter '{pattern}'. "
                "Use --list-ports to inspect available ports."
            )

    if len(ports) == 1:
        return ports[0].device

    ports.sort(key=lambda p: (-_port_score(p), _port_sort_key(p)))
    best = ports[0]
    second = ports[1]
    # If best candidate is clearly stronger than next one, auto-pick.
    if _port_score(best) >= 150 and (_port_score(best) - _port_score(second)) >= 40:
        return best.device

    print("Multiple serial ports detected. Select port:")
    for i, p in enumerate(ports, start=1):
        print(f"  {i}) {_port_line(p)}")
    print("  0) Cancel")

    while True:
        choice = input("Enter number: ").strip()
        if choice == "0":
            raise RuntimeError("Canceled by user")
        if choice.isdigit():
            idx = int(choice)
            if 1 <= idx <= len(ports):
                return ports[idx - 1].device
        print("Invalid choice, try again.")


def main():
    p = argparse.ArgumentParser(description="Flash app.bin via UART bootloader")
    p.add_argument("--port", default="auto", help="COM port (e.g. COM7) or auto")
    p.add_argument(
        "--port-filter",
        default="",
        help="Regex filter for auto mode (match device/description/manufacturer/hwid), e.g. CH340|CP210",
    )
    p.add_argument("--list-ports", action="store_true", help="List serial ports and exit")
    p.add_argument("--bin", required=True, help="Path to application .bin")
    p.add_argument("--baud", type=int, default=460800)
    p.add_argument("--chunk", type=int, default=256)
    p.add_argument("--timeout", type=float, default=30.0)
    p.add_argument("--wait-boot", type=float, default=15.0, help="Max time waiting for BL_WAITING")
    args = p.parse_args()

    if args.list_ports:
        list_serial_ports()
        return

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
        port = args.port
        if port.lower() == "auto":
            port = detect_serial_port(args.port_filter)
            print(f"Auto-detected port: {port}")
        with serial.Serial(port, args.baud, timeout=0.1) as ser:
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