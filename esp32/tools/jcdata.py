"""Build-time support for canonical Johnny Castaway Sierra resources."""

from __future__ import annotations

import hashlib
import struct
import zlib
from dataclasses import dataclass
from pathlib import Path

MAGIC = b"JCDT"
VERSION = 1
HEADER = struct.Struct("<4sB3xIII")
MAP_MD5 = "374e6d05c5e0acd88fb5af748948c899"
ARCHIVE_MD5 = "8bb6c99e9129806b5089a39d24228a36"


@dataclass(frozen=True)
class Resource:
    name: str
    payload: bytes


@dataclass(frozen=True)
class ScriptTag:
    id: int
    description: str


@dataclass(frozen=True)
class TtmResource:
    version: str
    bytecode: bytes
    tags: tuple[ScriptTag, ...]
    bookmarks: tuple[tuple[int, int], ...]


@dataclass(frozen=True)
class AdsResource:
    version: str
    resources: tuple[tuple[int, str], ...]
    bytecode: bytes
    tags: tuple[ScriptTag, ...]


@dataclass(frozen=True)
class BitmapResource:
    widths: tuple[int, ...]
    heights: tuple[int, ...]
    packed_pixels: bytes


def canonical_inputs(source: Path) -> tuple[bytes, bytes]:
    map_data = (source / "RESOURCE.MAP").read_bytes()
    archive_data = (source / "RESOURCE.001").read_bytes()
    actual_map = hashlib.md5(map_data).hexdigest()
    actual_archive = hashlib.md5(archive_data).hexdigest()
    if actual_map != MAP_MD5 or actual_archive != ARCHIVE_MD5:
        raise ValueError(
            "unsupported or mixed Johnny data: "
            f"RESOURCE.MAP={actual_map}, RESOURCE.001={actual_archive}"
        )
    return map_data, archive_data


def build_image(map_data: bytes, archive_data: bytes) -> bytes:
    payload = map_data + archive_data
    total_len = HEADER.size + len(payload)
    return HEADER.pack(MAGIC, VERSION, len(map_data), total_len, zlib.crc32(payload)) + payload


def parse_image(image: bytes) -> tuple[bytes, bytes]:
    if len(image) < HEADER.size:
        raise ValueError("jcdata header is truncated")
    magic, version, map_len, total_len, expected_crc = HEADER.unpack_from(image)
    if magic != MAGIC or version != VERSION or total_len != len(image):
        raise ValueError("invalid jcdata header")
    payload = image[HEADER.size:]
    if map_len > len(payload) or zlib.crc32(payload) != expected_crc:
        raise ValueError("invalid jcdata payload")
    return payload[:map_len], payload[map_len:]


def resources(map_data: bytes, archive_data: bytes):
    if len(map_data) < 21:
        raise ValueError("RESOURCE.MAP is truncated")
    count = struct.unpack_from("<H", map_data, 19)[0]
    if 21 + count * 8 > len(map_data):
        raise ValueError("RESOURCE.MAP entries are truncated")
    for index in range(count):
        _, offset = struct.unpack_from("<II", map_data, 21 + index * 8)
        if offset + 17 > len(archive_data):
            raise ValueError(f"resource {index} offset is outside RESOURCE.001")
        raw_name = archive_data[offset : offset + 13]
        name = raw_name.split(b"\0", 1)[0].decode("ascii").rstrip()
        size = struct.unpack_from("<I", archive_data, offset + 13)[0]
        start = offset + 17
        if start + size > len(archive_data):
            raise ValueError(f"resource {name} is truncated")
        yield Resource(name, archive_data[start : start + size])


def find_resource(map_data: bytes, archive_data: bytes, name: str) -> Resource:
    wanted = name.upper()
    for resource in resources(map_data, archive_data):
        if resource.name.upper() == wanted:
            return resource
    raise KeyError(name)


def _tag(data: bytes, offset: int, expected: bytes) -> int:
    if data[offset : offset + 4] != expected:
        raise ValueError(f"expected tag {expected.decode()} at {offset}")
    return offset + 4


def _cstring(data: bytes, offset: int, limit: int = 40) -> tuple[str, int]:
    end = data.find(b"\0", offset, min(len(data), offset + limit + 1))
    if end < 0:
        raise ValueError("unterminated script string")
    return data[offset:end].decode("ascii"), end + 1


def _script_tags(data: bytes, offset: int) -> tuple[tuple[ScriptTag, ...], int]:
    offset = _tag(data, offset, b"TAG:")
    if offset + 6 > len(data):
        raise ValueError("truncated TAG header")
    chunk_size, count = struct.unpack_from("<IH", data, offset)
    offset += 6
    chunk_end = min(len(data), offset + max(0, chunk_size - 2))
    tags = []
    for _ in range(count):
        if offset + 2 > chunk_end:
            raise ValueError("truncated TAG entry")
        tag_id = struct.unpack_from("<H", data, offset)[0]
        description, offset = _cstring(data, offset + 2)
        if offset > chunk_end:
            raise ValueError("TAG entry exceeds chunk")
        tags.append(ScriptTag(tag_id, description))
    return tuple(tags), offset


def ttm_bookmarks(bytecode: bytes) -> tuple[tuple[int, int], ...]:
    bookmarks = []
    offset = 0
    while offset < len(bytecode):
        if offset + 2 > len(bytecode):
            raise ValueError("truncated TTM opcode")
        opcode = struct.unpack_from("<H", bytecode, offset)[0]
        offset += 2
        arg_count = opcode & 0x0F
        if arg_count == 0x0F:
            _, offset = _cstring(bytecode, offset, len(bytecode) - offset)
            if offset & 1:
                offset += 1
        else:
            byte_count = arg_count * 2
            if offset + byte_count > len(bytecode):
                raise ValueError("truncated TTM arguments")
            if opcode in (0x1101, 0x1111):
                bookmarks.append((struct.unpack_from("<H", bytecode, offset)[0], offset + 2))
            offset += byte_count
    return tuple(bookmarks)


def parse_ttm(resource: Resource) -> TtmResource:
    data = resource.payload
    pos = _tag(data, 0, b"VER:")
    if pos + 4 > len(data):
        raise ValueError("truncated TTM version")
    version_size = struct.unpack_from("<I", data, pos)[0]
    pos += 4
    if pos + version_size > len(data) or version_size < 4:
        raise ValueError("invalid TTM version")
    version = data[pos:pos + 4].decode("ascii")
    pos += version_size
    pos = _tag(data, pos, b"PAG:")
    if pos + 6 > len(data):
        raise ValueError("truncated PAG chunk")
    pos += 6
    pos = _tag(data, pos, b"TT3:")
    if pos + 9 > len(data):
        raise ValueError("truncated TT3 chunk")
    chunk_size = struct.unpack_from("<I", data, pos)[0]
    method = data[pos + 4]
    output_size = struct.unpack_from("<I", data, pos + 5)[0]
    pos += 9
    if chunk_size < 5 or pos + chunk_size - 5 > len(data):
        raise ValueError("invalid TT3 chunk size")
    bytecode = decompress(method, data[pos:pos + chunk_size - 5], output_size)
    pos += chunk_size - 5
    pos = _tag(data, pos, b"TTI:")
    if pos + 4 > len(data):
        raise ValueError("truncated TTI chunk")
    pos += 4
    tags, _ = _script_tags(data, pos)
    return TtmResource(version, bytecode, tags, ttm_bookmarks(bytecode))


def parse_ads(resource: Resource) -> AdsResource:
    data = resource.payload
    pos = _tag(data, 0, b"VER:")
    if pos + 4 > len(data):
        raise ValueError("truncated ADS version")
    version_size = struct.unpack_from("<I", data, pos)[0]
    pos += 4
    if pos + version_size > len(data) or version_size < 4:
        raise ValueError("invalid ADS version")
    version = data[pos:pos + 4].decode("ascii")
    pos += version_size
    pos = _tag(data, pos, b"ADS:")
    if pos + 4 > len(data):
        raise ValueError("truncated ADS header")
    pos += 4
    pos = _tag(data, pos, b"RES:")
    if pos + 6 > len(data):
        raise ValueError("truncated RES header")
    chunk_size, count = struct.unpack_from("<IH", data, pos)
    pos += 6
    resources_list = []
    for _ in range(count):
        if pos + 2 > len(data):
            raise ValueError("truncated ADS resource entry")
        slot = struct.unpack_from("<H", data, pos)[0]
        name, pos = _cstring(data, pos + 2)
        resources_list.append((slot, name))
    pos = _tag(data, pos, b"SCR:")
    if pos + 9 > len(data):
        raise ValueError("truncated ADS script")
    script_size = struct.unpack_from("<I", data, pos)[0]
    method = data[pos + 4]
    output_size = struct.unpack_from("<I", data, pos + 5)[0]
    pos += 9
    if script_size < 5 or pos + script_size - 5 > len(data):
        raise ValueError("invalid ADS script size")
    bytecode = decompress(method, data[pos:pos + script_size - 5], output_size)
    pos += script_size - 5
    tags, _ = _script_tags(data, pos)
    return AdsResource(version, tuple(resources_list), bytecode, tags)


def parse_bitmap(resource: Resource) -> BitmapResource:
    data = resource.payload
    pos = _tag(data, 0, b"BMP:")
    if pos + 4 > len(data):
        raise ValueError("truncated BMP dimensions")
    pos += 4  # Aggregate dimensions are not used by the sprite renderer.
    pos = _tag(data, pos, b"INF:")
    if pos + 6 > len(data):
        raise ValueError("truncated BMP info")
    info_size, count = struct.unpack_from("<IH", data, pos)
    pos += 6
    if count == 0 or count > 256 or info_size < 2 + count * 4:
        raise ValueError("invalid BMP sprite table")
    info_end = pos - 2 + info_size
    if info_end > len(data):
        raise ValueError("truncated BMP sprite table")
    widths = struct.unpack_from("<" + "H" * count, data, pos)
    pos += count * 2
    heights = struct.unpack_from("<" + "H" * count, data, pos)
    pos = info_end
    if any(width == 0 or width & 1 for width in widths) or any(height == 0 for height in heights):
        raise ValueError("invalid BMP sprite dimensions")
    pos = _tag(data, pos, b"BIN:")
    if pos + 9 > len(data):
        raise ValueError("truncated BMP pixels")
    chunk_size = struct.unpack_from("<I", data, pos)[0]
    method = data[pos + 4]
    output_size = struct.unpack_from("<I", data, pos + 5)[0]
    pos += 9
    if chunk_size < 5 or pos + chunk_size - 5 > len(data):
        raise ValueError("invalid BMP pixel chunk")
    expected_size = sum(width * height // 2 for width, height in zip(widths, heights))
    if output_size != expected_size:
        raise ValueError("BMP pixel size does not match sprite dimensions")
    packed_pixels = decompress(method, data[pos:pos + chunk_size - 5], output_size)
    return BitmapResource(tuple(widths), tuple(heights), packed_pixels)


def decompress(method: int, source: bytes, output_size: int) -> bytes:
    if output_size <= 0:
        raise ValueError("invalid output size")
    if method == 1:
        output = bytearray()
        pos = 0
        while len(output) < output_size:
            if pos >= len(source):
                raise ValueError("RLE input ended early")
            control = source[pos]
            pos += 1
            length = control & 0x7F
            if control & 0x80:
                if pos >= len(source):
                    raise ValueError("RLE run is truncated")
                output.extend([source[pos]] * length)
                pos += 1
            else:
                output.extend(source[pos : pos + length])
                pos += length
            if len(output) > output_size:
                raise ValueError("RLE output overflow")
        if pos != len(source):
            raise ValueError("RLE input was not consumed")
        return bytes(output)
    if method != 2:
        raise ValueError(f"unknown compression method {method}")

    byte_pos = 0
    bit_pos_in_byte = 0

    def bits(count: int) -> int:
        nonlocal byte_pos, bit_pos_in_byte
        value = 0
        for bit in range(count):
            if byte_pos >= len(source):
                current = 0
            else:
                current = source[byte_pos]
            if current & (1 << bit_pos_in_byte):
                value |= 1 << bit
            bit_pos_in_byte += 1
            if bit_pos_in_byte == 8:
                bit_pos_in_byte = 0
                byte_pos += 1
        return value

    width = 9
    free_entry = 257
    stream_bit_pos = 0
    table: list[tuple[int, int]] = [(0, 0)] * 4096
    old_code = bits(width)
    last_byte = old_code
    output = bytearray([old_code & 0xFF])

    while byte_pos < len(source) and len(output) < output_size:
        new_code = bits(width)
        stream_bit_pos += width
        if new_code == 256:
            block_bits = width * 8
            skip = block_bits - ((stream_bit_pos - 1) % block_bits) - 1
            bits(skip)
            width = 9
            free_entry = 256
            stream_bit_pos = 0
            continue

        code = new_code
        stack = []
        if code >= free_entry:
            if code > free_entry:
                raise ValueError("invalid LZW dictionary code")
            stack.append(last_byte)
            code = old_code
        while code > 255:
            if code >= 4096:
                raise ValueError("invalid LZW dictionary index")
            prefix, appended = table[code]
            stack.append(appended)
            code = prefix
        stack.append(code)
        last_byte = code
        output.extend(reversed(stack))
        if free_entry < 4096:
            table[free_entry] = (old_code, last_byte)
            free_entry += 1
            if free_entry >= (1 << width) and width < 12:
                width += 1
                stream_bit_pos = 0
        old_code = new_code

    if len(output) < output_size:
        raise ValueError("LZW output ended early")
    return bytes(output[:output_size])


def parse_screen(resource: Resource) -> tuple[int, int, bytes]:
    data = resource.payload
    pos = _tag(data, 0, b"SCR:")
    pos += 4  # total size and flags
    pos = _tag(data, pos, b"DIM:")
    pos += 4  # DIM chunk size
    width, height = struct.unpack_from("<HH", data, pos)
    pos += 4
    pos = _tag(data, pos, b"BIN:")
    chunk_size = struct.unpack_from("<I", data, pos)[0]
    pos += 4
    if chunk_size < 5:
        raise ValueError("invalid BIN chunk size")
    method = data[pos]
    output_size = struct.unpack_from("<I", data, pos + 1)[0]
    pos += 5
    packed = data[pos : pos + chunk_size - 5]
    if len(packed) != chunk_size - 5:
        raise ValueError("truncated BIN chunk")
    return width, height, decompress(method, packed, output_size)


def parse_palette(resource: Resource) -> list[tuple[int, int, int]]:
    data = resource.payload
    pos = _tag(data, 0, b"PAL:")
    pos += 4  # size and two unknown bytes
    pos = _tag(data, pos, b"VGA:")
    pos += 4
    if pos + 256 * 3 > len(data):
        raise ValueError("truncated palette")
    colors = []
    for index in range(256):
        red, green, blue = data[pos + index * 3 : pos + index * 3 + 3]
        colors.append((red << 2, green << 2, blue << 2))
    return colors


def render_rgb565_right(width: int, height: int, packed: bytes,
                        palette: list[tuple[int, int, int]]) -> bytes:
    """Match the milestone firmware's 640x480 right-layout framebuffer."""
    if width <= 0 or width > 640 or width % 2 or height <= 0 or height > 480:
        raise ValueError("invalid screen dimensions")
    if len(packed) < width * height // 2 or len(palette) < 16:
        raise ValueError("screen fixture is truncated")
    bytes_per_row = width // 2
    counts = [0] * 16
    bottom_row = (height - 1) * bytes_per_row
    for x in range(width):
        packed_value = packed[bottom_row + x // 2]
        index = packed_value >> 4 if x % 2 == 0 else packed_value & 0x0F
        counts[index] += 1
    fill_index = max(range(16), key=counts.__getitem__)
    fill_red, fill_green, fill_blue = palette[fill_index]
    fill = ((fill_red & 0xF8) << 8) | ((fill_green & 0xFC) << 3) | (fill_blue >> 3)
    framebuffer = bytearray(fill.to_bytes(2, "little") * 640 * 480)
    # The right layout reserves the final 160 columns as a black side bar.
    framed = bytearray(800 * 480 * 2)
    for y in range(480):
        source = y * 640 * 2
        destination = y * 800 * 2
        framed[destination:destination + 640 * 2] = framebuffer[source:source + 640 * 2]
    framebuffer = framed
    for y in range(height):
        for x in range(width):
            value = packed[y * bytes_per_row + x // 2]
            index = value >> 4 if x % 2 == 0 else value & 0x0F
            red, green, blue = palette[index]
            rgb565 = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)
            offset = (y * 800 + x) * 2
            framebuffer[offset : offset + 2] = rgb565.to_bytes(2, "little")
    return bytes(framebuffer)


def draw_bitmap_sprite_rgb565(framebuffer: bytearray, destination_width: int,
                              bitmap: BitmapResource,
                              palette: list[tuple[int, int, int]], sprite: int,
                              x: int, y: int, flip: bool = False) -> None:
    """Draw one transparent 4-bpp sprite using the desktop TTM semantics."""
    if sprite < 0 or sprite >= len(bitmap.widths) or destination_width <= 0:
        raise ValueError("invalid bitmap sprite")
    width = bitmap.widths[sprite]
    height = bitmap.heights[sprite]
    packed_offset = sum(
        item_width * item_height // 2
        for item_width, item_height in zip(bitmap.widths[:sprite], bitmap.heights[:sprite])
    )
    pixels = bitmap.packed_pixels[packed_offset:packed_offset + width * height // 2]
    destination_height = len(framebuffer) // (destination_width * 2)
    for source_y in range(height):
        for source_x in range(width):
            packed = pixels[source_y * (width // 2) + source_x // 2]
            index = packed >> 4 if source_x % 2 == 0 else packed & 0x0F
            red, green, blue = palette[index]
            if (red, green, blue) == (0xA8, 0x00, 0xA8):
                continue
            destination_x = x + (width - 1 - source_x if flip else source_x)
            destination_y = y + source_y
            if not (0 <= destination_x < destination_width and
                    0 <= destination_y < destination_height):
                continue
            rgb565 = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)
            offset = (destination_y * destination_width + destination_x) * 2
            framebuffer[offset:offset + 2] = rgb565.to_bytes(2, "little")
