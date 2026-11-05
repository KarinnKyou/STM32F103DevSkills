#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""Extract GPIO pin defines from Wildfire (野火小智) F103 HAL example projects.

The example sources are GBK-encoded, so everything is decoded with gb18030.
Scans: <root>/*/2-???/野火小智STM32F103C8核心板/HAL/User/**/*.{h,c}
and prints, per module, the GPIO port/pin macros found.
"""
import os
import re
import sys

ROOT = r"D:\DSH_Workspace\04_Embedded\f103_doc"

MACRO = re.compile(
    r"^\s*#\s*define\s+([A-Z0-9_]+)\s+([^/\r\n]+?)\s*(?:/\*.*)?$", re.M
)
INTEREST = re.compile(r"GPIO|PIN|PORT|CLK|USART|I2C|SPI|TIM|ADC|CHANNEL", re.I)


def decode(path):
    with open(path, "rb") as fh:
        raw = fh.read()
    for enc in ("gb18030", "utf-8-sig", "utf-8"):
        try:
            return raw.decode(enc)
        except UnicodeDecodeError:
            continue
    return raw.decode("latin-1")


def find_modules(root):
    for name in sorted(os.listdir(root)):
        d = os.path.join(root, name)
        if not os.path.isdir(d) or not name.startswith("野火小智"):
            continue
        yield name


def core_hal_user(module_dir):
    for dirpath, dirnames, _ in os.walk(module_dir):
        if os.path.basename(dirpath) == "User" and os.path.basename(os.path.dirname(dirpath)) == "HAL":
            if "STM32F103C8核心板" in dirpath:
                return dirpath
    return None


def main():
    for mod in find_modules(ROOT):
        mdir = os.path.join(ROOT, mod)
        user = core_hal_user(mdir)
        if not user:
            print(f"\n## {mod}: (no C8-core HAL example)")
            continue
        print(f"\n## {mod}")
        print(f"   user dir: {user[len(ROOT) + 1:]}")
        hits = {}
        for dirpath, _, files in os.walk(user):
            for fn in files:
                if not fn.lower().endswith((".h", ".c")):
                    continue
                if not fn.lower().startswith("bsp_"):
                    continue
                text = decode(os.path.join(dirpath, fn))
                for name, value in MACRO.findall(text):
                    if INTEREST.search(name) and value.strip():
                        hits.setdefault(name, (value.strip(), fn))
        for name in sorted(hits):
            value, fn = hits[name]
            print(f"   {name:34s} = {value:34s}   [{fn}]")
        if not hits:
            print("   (no GPIO macros; project may use another naming scheme)")


if __name__ == "__main__":
    main()
