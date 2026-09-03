#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path

from jcdata import (
    canonical_inputs,
    find_resource,
    parse_palette,
    parse_screen,
    render_rgb565_right,
)


def main() -> int:
    parser = argparse.ArgumentParser(description="Inspect the first ESP32 scene fixture")
    parser.add_argument("--source", type=Path, required=True)
    args = parser.parse_args()
    map_data, archive_data = canonical_inputs(args.source)
    palette = parse_palette(find_resource(map_data, archive_data, "JOHNCAST.PAL"))
    width, height, pixels = parse_screen(find_resource(map_data, archive_data, "INTRO.SCR"))
    print(f"INTRO.SCR {width}x{height} packed4_size={len(pixels)} sha256={hashlib.sha256(pixels).hexdigest()}")
    print(f"right_rgb565_sha256={hashlib.sha256(render_rgb565_right(width, height, pixels, palette)).hexdigest()}")
    print(f"JOHNCAST.PAL first16={palette[:16]}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
