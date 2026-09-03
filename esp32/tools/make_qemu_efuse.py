"""Write ESP-IDF's default ESP32-S3 eFuse image for QEMU."""

import argparse
import sys
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("idf_path", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    sys.path.insert(0, str(args.idf_path / "tools"))
    from idf_py_actions.qemu_ext import QEMU_TARGETS

    args.output.write_bytes(QEMU_TARGETS["esp32s3"].default_efuse)


if __name__ == "__main__":
    main()
