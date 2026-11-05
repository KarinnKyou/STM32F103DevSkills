#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""Render PDF pages to PNG, or extract text.

Usage:
    python pdf2png.py <pdf> <outdir> [--pages 1,2] [--dpi 150]
    python pdf2png.py <pdf> --text [--pages 1]

Avoids inline Chinese literals in shell -c commands (PowerShell mangles them).
"""
import argparse
import os
import sys

import pymupdf


def parse_pages(spec, total):
    if not spec:
        return list(range(total))
    out = []
    for part in spec.split(","):
        part = part.strip()
        if "-" in part:
            a, b = part.split("-", 1)
            out.extend(range(int(a) - 1, min(int(b), total)))
        elif part:
            out.append(int(part) - 1)
    return [i for i in out if 0 <= i < total]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("pdf")
    ap.add_argument("outdir", nargs="?")
    ap.add_argument("--pages", default="")
    ap.add_argument("--dpi", type=int, default=150)
    ap.add_argument("--text", action="store_true")
    ap.add_argument("--prefix", default="")
    args = ap.parse_args()

    if not os.path.isfile(args.pdf):
        sys.exit(f"not a file: {args.pdf}")

    doc = pymupdf.open(args.pdf)
    total = len(doc)
    print(f"pages: {total}")

    if args.text:
        for i in parse_pages(args.pages, total):
            txt = doc[i].get_text()
            print(f"\n===== page {i + 1} ({len(txt.strip())} chars) =====")
            print(txt)
        doc.close()
        return

    outdir = args.outdir or os.path.dirname(os.path.abspath(args.pdf))
    os.makedirs(outdir, exist_ok=True)
    stem = os.path.splitext(os.path.basename(args.pdf))[0]
    if args.prefix:
        stem = args.prefix
    for i in parse_pages(args.pages, total):
        pix = doc[i].get_pixmap(dpi=args.dpi)
        dst = os.path.join(outdir, f"{stem}_p{i + 1}.png")
        pix.save(dst)
        print(f"{dst}  {pix.width}x{pix.height}")
    doc.close()


if __name__ == "__main__":
    main()
