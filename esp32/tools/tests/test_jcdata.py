import hashlib
import json
import struct
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))

from jcdata import (
    HEADER,
    build_image,
    canonical_inputs,
    decompress,
    draw_bitmap_sprite_rgb565,
    find_resource,
    parse_image,
    parse_ads,
    parse_bitmap,
    parse_palette,
    parse_screen,
    parse_ttm,
    render_rgb565_right,
    resources,
)


def max_layer_draw_commands(ttm):
    maximum = 0
    for _, start in ttm.bookmarks:
        offset = start
        current = 0
        while offset + 2 <= len(ttm.bytecode):
            opcode = struct.unpack_from("<H", ttm.bytecode, offset)[0]
            offset += 2
            arg_count = opcode & 0x0F
            if arg_count == 0x0F:
                end = ttm.bytecode.index(0, offset)
                offset = end + 1
                if offset & 1:
                    offset += 1
            else:
                offset += arg_count * 2
            if opcode == 0xA601:
                current = 0
            elif opcode in (0xA0A4, 0xA104, 0xA404, 0xA504, 0xA524):
                current += 1
                maximum = max(maximum, current)
            elif opcode == 0x0110:
                break
    return maximum


def ttm_commands_for_tag(ttm, tag):
    offset = dict(ttm.bookmarks)[tag]
    commands = []
    while offset + 2 <= len(ttm.bytecode):
        opcode = struct.unpack_from("<H", ttm.bytecode, offset)[0]
        offset += 2
        arg_count = opcode & 0x0F
        if arg_count == 0x0F:
            end = ttm.bytecode.index(0, offset)
            args = ttm.bytecode[offset:end].decode("ascii")
            offset = end + 1
            if offset & 1:
                offset += 1
        else:
            args = struct.unpack_from(
                "<" + "H" * arg_count, ttm.bytecode, offset
            ) if arg_count else ()
            offset += arg_count * 2
        commands.append((opcode, args))
        if opcode == 0x0110:
            break
    return commands


def signed16(value):
    return value if value < 0x8000 else value - 0x10000


class JcdataTests(unittest.TestCase):
    def test_shared_settings_contract(self):
        contract = json.loads((TOOLS.parent / "settings-contract.json").read_text())
        self.assertEqual(contract["layout"], {
            "values": ["left", "center", "right"], "default": "right"
        })
        self.assertEqual(contract["sidebar_mode"], {
            "values": ["off", "clock", "review"], "default": "off"
        })
        self.assertEqual(contract["weather_location"]["default"], None)
        self.assertEqual(contract["crt"], {
            "values": ["off", "soft"], "default": "soft"
        })

    def test_container_round_trip(self):
        map_data = b"map fixture"
        archive_data = b"archive fixture"
        image = build_image(map_data, archive_data)
        self.assertEqual(HEADER.size, 20)
        self.assertEqual(parse_image(image), (map_data, archive_data))

    def test_container_rejects_corruption(self):
        image = bytearray(build_image(b"map", b"archive"))
        image[-1] ^= 0x80
        with self.assertRaisesRegex(ValueError, "payload"):
            parse_image(bytes(image))

    def test_rle_literal_and_run(self):
        self.assertEqual(decompress(1, bytes([3]) + b"ABC" + bytes([0x82, 0x5A]), 5), b"ABCZZ")

    def test_unknown_compression_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "unknown"):
            decompress(9, b"x", 1)

    def test_canonical_intro_matches_desktop_fixture(self):
        data_dir = TOOLS.parent / "data"
        if not (data_dir / "RESOURCE.MAP").exists():
            self.skipTest("licensed canonical Sierra inputs are not present")
        map_data, archive_data = canonical_inputs(data_dir)
        palette = parse_palette(find_resource(map_data, archive_data, "JOHNCAST.PAL"))
        width, height, pixels = parse_screen(
            find_resource(map_data, archive_data, "INTRO.SCR")
        )
        self.assertEqual((width, height), (640, 480))
        self.assertEqual(
            hashlib.sha256(pixels).hexdigest(),
            "6eb9bed1fa948b537652cc4f37da9f4733e828d174a52900dfea98932eafaea1",
        )
        self.assertEqual(
            hashlib.sha256(render_rgb565_right(width, height, pixels, palette)).hexdigest(),
            "be6b74850333a3b2e9f1ecfadbfea7ada47d5b17b0e92b4ba09ff2f02e25ee40",
        )

    def test_all_canonical_screens_match_windows_fixtures(self):
        data_dir = TOOLS.parent / "data"
        if not (data_dir / "RESOURCE.MAP").exists():
            self.skipTest("licensed canonical Sierra inputs are not present")
        map_data, archive_data = canonical_inputs(data_dir)
        palette = parse_palette(find_resource(map_data, archive_data, "JOHNCAST.PAL"))
        fixtures = {
            "INTRO.SCR": (480, "6eb9bed1fa948b537652cc4f37da9f4733e828d174a52900dfea98932eafaea1", "be6b74850333a3b2e9f1ecfadbfea7ada47d5b17b0e92b4ba09ff2f02e25ee40"),
            "ISLETEMP.SCR": (350, "656b9ac4e65fe5baf1d980b5c35fdcd966094f24123e1cf34d988eaefa14bf2d", "a68119006596c3a6402f4fadadc945df10fb32481d2996b7d8b81cdb1c8be4f1"),
            "ISLAND2.SCR": (350, "17034a76ecb842216ad7034db5727c8b9bd727a8c8f857334371dc88225a993b", "00e902698f0d6c463c918e104898c2d319d1ff44d07dc780d63542e461885fc9"),
            "OCEAN00.SCR": (480, "377867e13fa5683d36d5bdedd1e55b0f1a26fe4e1271a7eab7401f2a7a452491", "cb30a9dae07ea6ff7f65c4947d1fa7b36597ac9be26e60cada5c89fbf4342e95"),
            "OCEAN01.SCR": (480, "06551ef375aaf9eb31d2d48a22aedfb2448727ffd4827e47d0bc154c3961422a", "4f8ded42e26a31c0060b3fa79eb2a520bfb2323e4ca4556bcdec6b44f7415d49"),
            "OCEAN02.SCR": (480, "9657e21fdc3bc36e847031cd5f743c9e36adb42b0cb9fe9b59be6ba815fe82db", "ef093942ba7505e4cea948cf3dc2b9697dfd90228338b14c9e54cc9682e221aa"),
            "NIGHT.SCR": (480, "55da5ef584269b47aa49d5c97e747c26834cd1a45c2f75da1fecaf227b368271", "a5881483ec418f8bc34d9c3b2e9abaa1fc7a6ef92819d8a0552a9a5bb3f3908e"),
            "SUZBEACH.SCR": (480, "7d49470cdc0eb22d79d800323de98c15032a358c4b31e27ef596909109f82737", "ec8aa7e92ca5b077b96747c8f7cf839000f8429a960765975eeac77919287347"),
            "JOFFICE.SCR": (350, "c5969c78e0698c76fc4a832beb3a77ddb072ec8d4247d356f019ed8d34915dab", "6b8082aee6cfab77c9e1a4fe16017bf5473fd3277604909c5cc672f12cd662ab"),
            "THEEND.SCR": (350, "03382824f994608e01ee6cd6c7335d2b33a621de4f62b3bcc6c09b1d0df86a1c", "25a66d88dcb80a836440e793d564e07c447c2a2a8a98012a6c6287683366250a"),
        }
        for name, (expected_height, decoded_hash, framebuffer_hash) in fixtures.items():
            with self.subTest(name=name):
                width, height, pixels = parse_screen(
                    find_resource(map_data, archive_data, name)
                )
                self.assertEqual((width, height), (640, expected_height))
                self.assertEqual(hashlib.sha256(pixels).hexdigest(), decoded_hash)
                framebuffer = render_rgb565_right(width, height, pixels, palette)
                self.assertEqual(hashlib.sha256(framebuffer).hexdigest(), framebuffer_hash)
                if height == 350:
                    padding = framebuffer[350 * 1600 : 351 * 1600][:1280]
                    self.assertEqual(len({padding[index:index + 2] for index in range(0, 1280, 2)}), 1)
                    self.assertEqual(framebuffer[350 * 1600 + 1280 : 351 * 1600], bytes(320))

    def test_canonical_script_catalog_parses(self):
        data_dir = TOOLS.parent / "data"
        if not (data_dir / "RESOURCE.MAP").exists():
            self.skipTest("licensed canonical Sierra inputs are not present")
        map_data, archive_data = canonical_inputs(data_dir)
        resource_list = list(resources(map_data, archive_data))
        ttms = [parse_ttm(resource) for resource in resource_list if resource.name.endswith(".TTM")]
        ads = [parse_ads(resource) for resource in resource_list if resource.name.endswith(".ADS")]
        self.assertEqual(len(ttms), 41)
        self.assertEqual(len(ads), 10)
        self.assertEqual(sum(len(item.tags) for item in ttms), 616)
        self.assertEqual(sum(len(item.bookmarks) for item in ttms), 616)
        self.assertEqual(sum(len(item.tags) for item in ads), 66)
        self.assertTrue(all(item.bytecode and item.tags for item in ttms))
        self.assertTrue(all(item.bytecode and item.tags for item in ads))

    def test_canonical_ttm_layers_fit_firmware_draw_bound(self):
        data_dir = TOOLS.parent / "data"
        if not (data_dir / "RESOURCE.MAP").exists():
            self.skipTest("licensed canonical Sierra inputs are not present")
        map_data, archive_data = canonical_inputs(data_dir)
        maxima = {
            resource.name: max_layer_draw_commands(parse_ttm(resource))
            for resource in resources(map_data, archive_data)
            if resource.name.endswith(".TTM")
        }
        self.assertEqual(max(maxima.values()), 44)
        self.assertEqual(maxima["GJNAT1.TTM"], 44)
        self.assertEqual(maxima["SJGLIMPS.TTM"], 19)
        self.assertLessEqual(maxima["SJGLIMPS.TTM"], 32)
        self.assertLessEqual(max(maxima.values()), 48)

    def test_first_mermaid_glimpse_geometry_and_replacement_bound(self):
        data_dir = TOOLS.parent / "data"
        if not (data_dir / "RESOURCE.MAP").exists():
            self.skipTest("licensed canonical Sierra inputs are not present")
        map_data, archive_data = canonical_inputs(data_dir)
        mary = parse_ads(find_resource(map_data, archive_data, "MARY.ADS"))
        self.assertEqual(dict(mary.resources)[2], "SJGLIMPS.TTM")
        ttm = parse_ttm(find_resource(map_data, archive_data, "SJGLIMPS.TTM"))
        commands = ttm_commands_for_tag(ttm, 108)
        self.assertEqual(sum(opcode == 0xA404 for opcode, _ in commands), 105)
        self.assertEqual(sum(opcode == 0xA601 for opcode, _ in commands), 96)
        self.assertEqual(max_layer_draw_commands(ttm), 19)
        self.assertIn((0xA504, (520, 231, 4, 0)), commands)
        self.assertIn((0xA504, (524, 210, 27, 0)), commands)

    def test_tanker_traversal_and_signed_store_area_coordinates(self):
        data_dir = TOOLS.parent / "data"
        if not (data_dir / "RESOURCE.MAP").exists():
            self.skipTest("licensed canonical Sierra inputs are not present")
        map_data, archive_data = canonical_inputs(data_dir)
        visitor = parse_ads(find_resource(map_data, archive_data, "VISITOR.ADS"))
        self.assertEqual(dict(visitor.resources)[3], "GJVIS6.TTM")
        ttm = parse_ttm(find_resource(map_data, archive_data, "GJVIS6.TTM"))
        traversal = [
            tuple(signed16(value) for value in args)
            for opcode, args in ttm_commands_for_tag(ttm, 9)
            if opcode in (0xA504, 0xA524) and args[3] == 2
        ]
        self.assertGreater(len(traversal), 70)
        self.assertEqual(traversal[0][0], -53)
        self.assertGreater(max(item[0] for item in traversal), 175)
        stores = [
            tuple(signed16(value) for value in args)
            for opcode, args in ttm_commands_for_tag(ttm, 8)
            if opcode == 0x4204
        ]
        self.assertEqual(len(stores), 28)
        self.assertEqual(stores[0], (-2, 43, 17, 302))

    def test_coconut_plane_chain_declares_slot_one_dependency(self):
        data_dir = TOOLS.parent / "data"
        if not (data_dir / "RESOURCE.MAP").exists():
            self.skipTest("licensed canonical Sierra inputs are not present")
        map_data, archive_data = canonical_inputs(data_dir)
        visitor = parse_ads(find_resource(map_data, archive_data, "VISITOR.ADS"))
        self.assertEqual(dict(visitor.resources)[5], "GJVIS5.TTM")
        ttm = parse_ttm(find_resource(map_data, archive_data, "GJVIS5.TTM"))
        slot_one_draws = [
            tuple(signed16(value) for value in args)
            for opcode, args in ttm_commands_for_tag(ttm, 8)
            if opcode in (0xA504, 0xA524) and args[3] == 1
        ]
        self.assertEqual(slot_one_draws, [(-185, 216, 0, 1)])
        self.assertTrue(all(tag in dict(ttm.bookmarks)
                            for tag in (1, 8, 7, 9, 10, 3, 5)))

    def test_canonical_stand_ads_matches_first_scheduler_fixture(self):
        data_dir = TOOLS.parent / "data"
        if not (data_dir / "RESOURCE.MAP").exists():
            self.skipTest("licensed canonical Sierra inputs are not present")
        map_data, archive_data = canonical_inputs(data_dir)
        ads = parse_ads(find_resource(map_data, archive_data, "STAND.ADS"))
        self.assertEqual(ads.version, "1.09")
        self.assertEqual(ads.resources, ((1, "MJAMBWLK.TTM"), (2, "MJTELE.TTM")))
        self.assertEqual(len(ads.tags), 15)
        self.assertEqual(len(ads.bytecode), 2396)
        self.assertEqual(
            hashlib.sha256(ads.bytecode).hexdigest(),
            "ae2af07205b1f276ba3f5e80f86d8c1da82d49333154d0d672459f3da0cc0680",
        )
        self.assertEqual(
            ads.bytecode[:80],
            bytes.fromhex(
                "010000f20e006013010001002014601301000200201460130100030020146013"
                "0100350010300520010001000000020005200100020003000200052001003500"
                "0000050005200100030000000200ff30"
            ),
        )

    def test_canonical_activity_ads_matches_completion_fixtures(self):
        data_dir = TOOLS.parent / "data"
        if not (data_dir / "RESOURCE.MAP").exists():
            self.skipTest("licensed canonical Sierra inputs are not present")
        map_data, archive_data = canonical_inputs(data_dir)
        ads = parse_ads(find_resource(map_data, archive_data, "ACTIVITY.ADS"))
        self.assertEqual(ads.version, "1.09")
        self.assertEqual(len(ads.resources), 6)
        self.assertEqual(len(ads.tags), 10)
        self.assertEqual(len(ads.bytecode), 2558)
        self.assertEqual(
            hashlib.sha256(ads.bytecode).hexdigest(),
            "61f45e0c9c092a9eecf0d3344b41d232376e3ecd997fd64934a80cf2779cbab9",
        )
        self.assertEqual(
            ads.bytecode[:30],
            bytes.fromhex(
                "0100301301000c00052001000c0000000100052001000d00000001001015"
            ),
        )
        self.assertEqual(
            ads.bytecode[0x4B2:0x4F0],
            bytes.fromhex(
                "040030130200010005200200010000000100101550130200010005200200"
                "03000000010010155013020003000520020002000000010010f0ffff1015"
                "ffff"
            ),
        )

    def test_canonical_activity_ads_materializes_ordered_ttm_layers(self):
        data_dir = TOOLS.parent / "data"
        if not (data_dir / "RESOURCE.MAP").exists():
            self.skipTest("licensed canonical Sierra inputs are not present")
        map_data, archive_data = canonical_inputs(data_dir)
        ads = parse_ads(find_resource(map_data, archive_data, "ACTIVITY.ADS"))
        self.assertEqual(ads.resources[0], (1, "GJDIVE.TTM"))
        ttm = parse_ttm(find_resource(map_data, archive_data, "GJDIVE.TTM"))
        self.assertEqual(dict(ttm.bookmarks)[12], 42)
        self.assertEqual(dict(ttm.bookmarks)[13], 126)
        palette = parse_palette(find_resource(map_data, archive_data, "JOHNCAST.PAL"))
        width, height, pixels = parse_screen(
            find_resource(map_data, archive_data, "ISLETEMP.SCR")
        )
        framebuffer = bytearray(render_rgb565_right(width, height, pixels, palette))
        bitmap = parse_bitmap(find_resource(map_data, archive_data, "JOHNWALK.BMP"))
        draw_bitmap_sprite_rgb565(framebuffer, 800, bitmap, palette, 9, 395, 214)
        self.assertEqual(
            hashlib.sha256(framebuffer).hexdigest(),
            "0085c76f798dd4d065caba2fcd294281a3c18a2b1b7664ef8e15767bfad1db30",
        )

    def test_canonical_activity_complete_event_resources_and_tags(self):
        data_dir = TOOLS.parent / "data"
        if not (data_dir / "RESOURCE.MAP").exists():
            self.skipTest("licensed canonical Sierra inputs are not present")
        map_data, archive_data = canonical_inputs(data_dir)
        ads = parse_ads(find_resource(map_data, archive_data, "ACTIVITY.ADS"))
        self.assertEqual(
            ads.resources[:2],
            ((1, "GJDIVE.TTM"), (2, "MJDIVE.TTM")),
        )
        self.assertEqual(dict(ads.resources)[4], "MJREAD.TTM")
        tags_by_slot = {
            slot: {tag.id for tag in parse_ttm(
                find_resource(map_data, archive_data, name)
            ).tags}
            for slot, name in ads.resources
        }
        completion_chain = (
            (1, 12), (1, 13), (1, 8), (1, 14),
            (2, 2), (1, 13), (1, 9), (1, 14),
            (2, 2), (1, 13), (1, 9), (1, 11),
        )
        self.assertTrue(all(tag in tags_by_slot[slot]
                            for slot, tag in completion_chain))
        tag10_completion_chain = (
            (4, 110), (4, 24), (4, 114), (4, 108), (4, 92),
            (4, 93), (4, 109), (4, 84), (4, 85), (4, 119),
            (4, 96), (4, 118), (4, 116),
        )
        self.assertTrue(all(tag in tags_by_slot[slot]
                            for slot, tag in tag10_completion_chain))
        batch_activity_chains = (
            ((6, 7), (6, 20), (6, 22), (6, 23), (6, 29), (6, 25),
             (6, 26), (6, 30), (6, 31), (6, 32), (6, 33), (6, 28)),
            ((4, 1), (4, 110), (4, 24), (4, 5), (4, 16), (4, 26),
             (4, 20), (4, 21)),
            ((6, 30), (6, 31), (7, 9), (7, 13), (7, 12), (7, 11),
             (7, 16), (7, 17), (7, 14), (7, 15), (7, 19)),
        )
        for chain in batch_activity_chains:
            self.assertTrue(all(tag in tags_by_slot[slot]
                                for slot, tag in chain))

        building = parse_ads(find_resource(map_data, archive_data, "BUILDING.ADS"))
        self.assertEqual(
            building.resources,
            ((1, "MJSAND.TTM"), (2, "GJGULIVR.TTM"), (3, "MJFIRE.TTM")),
        )
        building_tags_by_slot = {
            slot: {tag.id for tag in parse_ttm(
                find_resource(map_data, archive_data, name)
            ).tags}
            for slot, name in building.resources
        }
        building_tag4_chain = (
            (2, 14), (2, 15), (2, 2), (2, 6), (2, 12), (2, 52),
            (2, 53), (2, 54), (2, 66), (2, 65), (2, 63), (2, 64),
            (2, 67),
        )
        self.assertTrue(all(tag in building_tags_by_slot[slot]
                            for slot, tag in building_tag4_chain))
        for name in ("LILIPUTS.BMP", "STNDLAY.BMP", "SLEEP.BMP"):
            self.assertGreater(
                len(find_resource(map_data, archive_data, name).payload), 0
            )

    def test_canonical_ambient_bitmap_matches_desktop_fixture(self):
        data_dir = TOOLS.parent / "data"
        if not (data_dir / "RESOURCE.MAP").exists():
            self.skipTest("licensed canonical Sierra inputs are not present")
        map_data, archive_data = canonical_inputs(data_dir)
        bitmap = parse_bitmap(find_resource(map_data, archive_data, "MJ_AMB.BMP"))
        self.assertEqual(len(bitmap.widths), 35)
        self.assertEqual(bitmap.widths[16:18], (40, 40))
        self.assertEqual(bitmap.heights[16:18], (56, 56))
        self.assertEqual(len(bitmap.packed_pixels), 27484)
        self.assertEqual(
            hashlib.sha256(bitmap.packed_pixels).hexdigest(),
            "c7150307b7cb3b008c46522ab4aeaedf9be9e4504c03ec318660d54912d307ac",
        )
        palette = parse_palette(find_resource(map_data, archive_data, "JOHNCAST.PAL"))
        width, height, pixels = parse_screen(
            find_resource(map_data, archive_data, "ISLETEMP.SCR")
        )
        framebuffer = bytearray(render_rgb565_right(width, height, pixels, palette))
        draw_bitmap_sprite_rgb565(framebuffer, 800, bitmap, palette, 16, 293, 262, True)
        draw_bitmap_sprite_rgb565(framebuffer, 800, bitmap, palette, 3, 299, 243)
        self.assertEqual(
            hashlib.sha256(framebuffer).hexdigest(),
            "c073f88c2dfeb087d09d86097833704c2d3169f375794076d2c1c51071cb275a",
        )

    def test_canonical_ambient_tag_one_frame_sequence(self):
        data_dir = TOOLS.parent / "data"
        if not (data_dir / "RESOURCE.MAP").exists():
            self.skipTest("licensed canonical Sierra inputs are not present")
        map_data, archive_data = canonical_inputs(data_dir)
        ttm = parse_ttm(find_resource(map_data, archive_data, "MJAMBWLK.TTM"))
        bitmap = parse_bitmap(find_resource(map_data, archive_data, "MJ_AMB.BMP"))
        palette = parse_palette(find_resource(map_data, archive_data, "JOHNCAST.PAL"))
        width, height, pixels = parse_screen(
            find_resource(map_data, archive_data, "ISLETEMP.SCR")
        )
        background = render_rgb565_right(width, height, pixels, palette)
        offset = dict(ttm.bookmarks)[1]
        delay = 4
        elapsed_centiseconds = 0
        frames = []
        update_offsets = []
        framebuffer = bytearray(background)

        while offset < len(ttm.bytecode):
            instruction_offset = offset
            opcode = struct.unpack_from("<H", ttm.bytecode, offset)[0]
            offset += 2
            arg_count = opcode & 0x0F
            self.assertNotEqual(arg_count, 0x0F, "tag 1 unexpectedly gained a string command")
            args = struct.unpack_from("<" + "H" * arg_count, ttm.bytecode, offset)
            offset += arg_count * 2
            if opcode == 0xA601:
                framebuffer = bytearray(background)
            elif opcode in (0xA504, 0xA524):
                draw_bitmap_sprite_rgb565(
                    framebuffer, 800, bitmap, palette, args[2], args[0], args[1],
                    opcode == 0xA524,
                )
            elif opcode == 0x1021:
                delay = max(args[0], 4)
            elif opcode == 0x0FF0:
                frames.append(hashlib.sha256(framebuffer).hexdigest())
                update_offsets.append(instruction_offset)
                elapsed_centiseconds += delay
            elif opcode == 0x0110:
                frames.append(hashlib.sha256(framebuffer).hexdigest())
                break

        self.assertEqual(update_offsets, [120, 146, 172, 198, 224, 250, 276, 302])
        self.assertEqual(elapsed_centiseconds, 80)
        self.assertEqual(frames, [
            "c073f88c2dfeb087d09d86097833704c2d3169f375794076d2c1c51071cb275a",
            "11ddb76221a6a2ae88c2cae76a76d56bbcb1167ecb33a0f3e4f6cf392d9a5bf8",
            "9bd67bfb39cd0ff6cf9829c05888de77a86c781045b5b3e0646d31e9e4ad32bd",
            "6eba0b3f3a2a57197c1f1543c47d9b1c5f2dfa568aac2afc02432cfa18eaffc8",
            "9bd67bfb39cd0ff6cf9829c05888de77a86c781045b5b3e0646d31e9e4ad32bd",
            "6eba0b3f3a2a57197c1f1543c47d9b1c5f2dfa568aac2afc02432cfa18eaffc8",
            "9bd67bfb39cd0ff6cf9829c05888de77a86c781045b5b3e0646d31e9e4ad32bd",
            "2f28d021124373f4ca67c43666c05071dabe7ff93561ee27d4998d4aa5aecfb0",
            "8c4834d84d432f3880ffadbfa1392bd91d54038c91269b8e86488b207178901a",
        ])


if __name__ == "__main__":
    unittest.main()
