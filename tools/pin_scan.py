#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""Permissive GPIO macro scan for one Wildfire F103 example directory.

Unlike extract_pins.py, this walks any path containing 核心板 and prints every
GPIO/ADC/TIM/SPI/I2C macro it finds in bsp_*.h and main.c. Sources are GBK.
"""
import os
import re
import sys

MACRO = re.compile(r"^\s*#\s*define\s+([A-Z0-9_]+)\s+([^/\r\n]+?)\s*(?:/\*.*)?$", re.M)
INTEREST = re.compile(r"GPIO|PIN|ADC|TIM|SPI|I2C|USART|CHANNEL", re.I)


def decode(path):
    with open(path, "rb") as fh:
        raw = fh.read()
    for enc in ("gb18030", "utf-8-sig", "utf-8"):
        try:
            return raw.decode(enc)
        except UnicodeDecodeError:
            continue
    return raw.decode("latin-1")


def main():
    root = sys.argv[1]
    for dirpath, dirnames, files in os.walk(root):
        if "核心板" not in dirpath:
            dirnames[:] = [d for d in dirnames if "核心板" in d or True]
            continue
        for fn in files:
            if not (fn.lower().startswith("bsp_") or fn == "main.c"):
                continue
            if not fn.lower().endswith((".h", ".c")):
                continue
            text = decode(os.path.join(dirpath, fn))
            seen = set()
            for name, value in MACRO.findall(text):
                if not INTEREST.search(name) or name in seen:
                    continue
                seen.add(name)
                if not re.search(r"GPIO_PIN_\d+|GPIO[A-C]|ADC_\d|ADC_CHANNEL|USART\d|TIM\d|SPI\d", value):
                    continue
                print(f"  {name:32s} = {value.strip():30s} [{fn}]")


if __name__ == "__main__":
    main()
