#!/usr/bin/env python3
"""Fail if a screenshot shows no rendered geometry -- catching a blank frame or a
frame that is just the clear color (which a plain non-black check would pass).

Reads an uncompressed BMP (the snapshot format the in-process helper writes) with
the standard library only, so it runs anywhere python3 does without Pillow. Finds
the dominant (background/clear) color and asserts that a minimum fraction of
pixels differ from it -- i.e. that something was actually drawn on top of the
clear. A single flat color (nothing rendered, or only glClear ran) fails.

--region x,y,w,h limits the check to that rectangle (pixels, top-left origin),
e.g. a GL canvas inside toolkit chrome that would otherwise count as content.

Usage: assert-image-content.py <image.bmp> [min-foreground-fraction]
                               [--region x,y,w,h]
"""

import struct
import sys


def parse_args(argv):
    usage = (
        "usage: assert-image-content.py <image> [min-foreground-fraction] "
        "[--region x,y,w,h]"
    )
    region = None
    if "--region" in argv:
        i = argv.index("--region")
        try:
            region = tuple(int(v) for v in argv[i + 1].split(","))
        except (IndexError, ValueError):
            sys.exit(usage)
        if len(region) != 4 or region[2] <= 0 or region[3] <= 0:
            sys.exit(usage)
        argv = argv[:i] + argv[i + 2 :]
    if len(argv) < 2:
        sys.exit(usage)
    threshold = float(argv[2]) if len(argv) > 2 else 0.02
    return argv[1], threshold, region


def main():
    path, threshold, region = parse_args(sys.argv)

    with open(path, "rb") as handle:
        data = handle.read()
    if data[:2] != b"BM":
        sys.exit(f"{path}: not a BMP (got {data[:2]!r}); snapshot format changed?")

    offset = struct.unpack_from("<I", data, 10)[0]
    width = struct.unpack_from("<i", data, 18)[0]
    height = abs(struct.unpack_from("<i", data, 22)[0])
    bpp = struct.unpack_from("<H", data, 28)[0]
    if bpp not in (24, 32) or width <= 0 or height <= 0:
        sys.exit(f"{path}: unsupported BMP ({width}x{height} {bpp}bpp)")

    bytes_per_pixel = bpp // 8
    row_stride = ((width * bytes_per_pixel + 3) // 4) * 4
    # A positive BMP height stores rows bottom-up; region rows are top-down.
    bottom_up = struct.unpack_from("<i", data, 22)[0] > 0

    x0, y0, x1, y1 = 0, 0, width, height
    if region:
        x0, y0 = max(region[0], 0), max(region[1], 0)
        x1, y1 = min(x0 + region[2], width), min(y0 + region[3], height)
        if x0 >= x1 or y0 >= y1:
            sys.exit(f"{path}: region {region} lies outside {width}x{height}")

    # Sample a coarse grid, quantize to tame AA/dither noise, and count colors.
    counts = {}
    samples = []
    for y in range(y0, y1, 4):
        row = height - 1 - y if bottom_up else y
        base = offset + row * row_stride
        for x in range(x0, x1, 4):
            pixel = base + x * bytes_per_pixel
            q = (data[pixel] >> 4, data[pixel + 1] >> 4, data[pixel + 2] >> 4)
            counts[q] = counts.get(q, 0) + 1
            samples.append(q)

    total = len(samples)
    if not total:
        sys.exit(f"{path}: empty image")
    background = max(counts, key=counts.get)
    foreground = sum(1 for q in samples if q != background)
    fraction = foreground / total

    if fraction < threshold:
        sys.exit(
            f"{path}: {fraction:.0%} of pixels differ from the background "
            f"color {background} (< {threshold:.0%}) -- nothing rendered "
            "(blank or only the clear color)?"
        )
    where = f" in region {region}" if region else ""
    print(
        f"{path}: {fraction:.0%} foreground over background {background}{where} -- OK"
    )


if __name__ == "__main__":
    main()
