#!/usr/bin/env python3
from __future__ import annotations

import argparse
import sys
from pathlib import Path

from jcdata import build_image, canonical_inputs


def main() -> int:
    parser = argparse.ArgumentParser(description="Build the Johnny raw flash data image")
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        map_data, archive_data = canonical_inputs(args.source)
        image = build_image(map_data, archive_data)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_bytes(image)
    except (OSError, ValueError) as exc:
        print(f"make_jcdata: {exc}", file=sys.stderr)
        return 1
    print(f"jcdata: wrote {len(image)} bytes to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
