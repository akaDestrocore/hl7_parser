#!/usr/bin/env python3
# tools/gen_test_frame.py
"""Generates a Renode .resc snippet that injects an MLLP-framed HL7 message
into a UART peripheral byte-by-byte via WriteChar. Lets you test framing/
parsing without the PC emulator script.

Usage: python3 tools/gen_test_frame.py <scenario> > .renode/test_frames/<scenario>.resc
"""
import sys

VT, FS, CR = 0x0B, 0x1C, 0x0D
UART = "sysbus.uart4"

GOOD_MESSAGE = (
    "MSH|^~\\&|MONITOR|ICU|HIS|HOSPITAL|202607291130||ORU^R01|12345|P|2.5\r"
    "PID|1||123456||Ivanov^Ivan\r"
    "OBX|1|NM|8867-4^Heart Rate||72|bpm\r"
    "OBX|2|NM|59408-5^SpO2||98|%\r"
)

UNKNOWN_OBX_MESSAGE = GOOD_MESSAGE + "OBX|3|NM|99999-9^Made Up Code||42|xyz\r"

def frame(payload: str) -> bytes:
    return bytes([VT]) + payload.encode("ascii") + bytes([FS, CR])

SCENARIOS = {
    "good": frame(GOOD_MESSAGE),
    "unknown_obx": frame(UNKNOWN_OBX_MESSAGE),
    "no_msh": frame("PID|1||123456||Ivanov^Ivan\rOBX|1|NM|8867-4^Heart Rate||72|bpm\r"),
    "truncated": bytes([VT]) + GOOD_MESSAGE.encode("ascii"),        # no FS/CR at all
    "fs_no_cr": frame(GOOD_MESSAGE)[:-1] + bytes([0x41]),            # FS followed by 'A' not CR
    "double_vt": bytes([VT]) + b"MSH|garbage" + bytes([VT]) + GOOD_MESSAGE.encode("ascii") + bytes([FS, CR]),
}

if __name__ == "__main__":
    name = sys.argv[1] if len(sys.argv) > 1 else "good"
    for b in SCENARIOS[name]:
        print(f"{UART} WriteChar {b}")