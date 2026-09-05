import pathlib
import re
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
ICON_SOURCE = (
    ROOT
    / "johnny-esp32"
    / "components"
    / "jcgfx"
    / "weather_pixel_icons.h"
)
GFX_SOURCE = (
    ROOT / "johnny-esp32" / "components" / "jcgfx" / "jcgfx.c"
)


def rgb565(red, green, blue):
    return ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)


def load_masks():
    source = ICON_SOURCE.read_text(encoding="utf-8")
    masks = {}
    pattern = re.compile(
        r"static const uint32_t (WEATHER_[A-Z_]+)_ROWS\[32\] = \{(.*?)\};",
        re.DOTALL,
    )
    for name, body in pattern.findall(source):
        masks[name] = [int(value, 16) for value in re.findall(r"0x([0-9a-f]+)U", body)]
    return source, masks


def icon_kind(code):
    if code == 0:
        return "clear"
    if code in (1, 2):
        return "partly"
    if code in (3, 45, 48):
        return "cloudy"
    if 51 <= code <= 67 or 80 <= code <= 82:
        return "rain"
    if 71 <= code <= 77 or code in (85, 86):
        return "snow"
    if 95 <= code <= 99:
        return "storm"
    return "cloudy"


def render_icon(masks, code, night=False, muted=False):
    pixels = [0] * (64 * 64)
    dull = rgb565(125, 145, 160)
    blue = dull if muted else rgb565(86, 190, 255)
    yellow = dull if muted else rgb565(255, 213, 74)
    white = dull if muted else rgb565(235, 240, 248)

    def draw(name, color):
        for source_y, row in enumerate(masks[name]):
            for source_x in range(32):
                if not row & (1 << source_x):
                    continue
                for offset_y in range(2):
                    for offset_x in range(2):
                        target = (source_y * 2 + offset_y) * 64 + source_x * 2 + offset_x
                        pixels[target] = color

    kind = icon_kind(code)
    if kind == "clear":
        draw("WEATHER_MOON" if night else "WEATHER_SUN", white if night else yellow)
        return pixels
    if kind == "cloudy":
        draw("WEATHER_CLOUDS", blue)
        return pixels

    draw("WEATHER_CLOUD", blue)
    if kind in ("partly", "rain", "snow"):
        draw("WEATHER_MOON_ACCENT" if night else "WEATHER_SUN_ACCENT", white if night else yellow)
    if kind == "rain":
        draw("WEATHER_RAIN_ACCENT", white)
    elif kind == "snow":
        draw("WEATHER_SNOW_ACCENT", white)
    elif kind == "storm":
        draw("WEATHER_STORM_RAIN_ACCENT", white)
        draw("WEATHER_LIGHTNING_ACCENT", yellow)
    return pixels


def fnv1a_pixels(pixels):
    value = 2166136261
    for pixel in pixels:
        value = ((value ^ pixel) * 16777619) & 0xFFFFFFFF
    return value


class WeatherPixelIconTests(unittest.TestCase):
    def test_upstream_attribution_and_compact_masks_are_present(self):
        source, masks = load_masks()
        self.assertIn("https://github.com/Dhole/weather-pixel-icons", source)
        self.assertIn("CC BY-SA 4.0", source)
        self.assertEqual(len(masks), 10)
        self.assertTrue(all(len(rows) == 32 for rows in masks.values()))

    def test_condition_day_night_stale_and_fallback_framebuffers(self):
        _, masks = load_masks()
        fixtures = {
            "sunny": (0, False, False, 0xF6BEA6E5),
            "night": (0, True, False, 0x2CD13FE5),
            "partly": (2, False, False, 0x2816D4A1),
            "cloudy": (3, False, False, 0xB6B541C5),
            "fog": (45, False, False, 0xB6B541C5),
            "rain": (61, False, False, 0xF0FB2D7D),
            "night_rain": (61, True, False, 0x1BCC03D9),
            "snow": (71, False, False, 0x60B3C33D),
            "storm": (95, False, False, 0x5E1156A1),
            "stale": (61, False, True, 0x8CB1E245),
            "unavailable": (255, False, True, 0x31CF3045),
            "unknown": (255, False, False, 0xB6B541C5),
        }
        for name, (code, night, muted, expected) in fixtures.items():
            with self.subTest(name=name):
                pixels = render_icon(masks, code, night, muted)
                self.assertEqual(fnv1a_pixels(pixels), expected)

    def test_scaling_is_exact_and_sidebar_cannot_overlap_scene(self):
        _, masks = load_masks()
        pixels = render_icon(masks, 2)
        for y in range(0, 64, 2):
            for x in range(0, 64, 2):
                block = {
                    pixels[y * 64 + x],
                    pixels[y * 64 + x + 1],
                    pixels[(y + 1) * 64 + x],
                    pixels[(y + 1) * 64 + x + 1],
                }
                self.assertEqual(len(block), 1)
        self.assertGreaterEqual(688, 640)
        self.assertLessEqual(688 + 64, 800)
        gfx = GFX_SOURCE.read_text(encoding="utf-8")
        self.assertIn("draw_weather_icon(destination, width, height, 688, 142", gfx)

    def test_weather_mapping_and_boot_fixture_are_compiled_into_firmware(self):
        gfx = GFX_SOURCE.read_text(encoding="utf-8")
        for token in (
            "WEATHER_ICON_CLEAR",
            "WEATHER_ICON_PARTLY_CLOUDY",
            "WEATHER_ICON_CLOUDY",
            "WEATHER_ICON_RAIN",
            "WEATHER_ICON_SNOW",
            "WEATHER_ICON_STORM",
            "jcgfx_verify_weather_icon_fixtures",
        ):
            self.assertIn(token, gfx)


if __name__ == "__main__":
    unittest.main()
