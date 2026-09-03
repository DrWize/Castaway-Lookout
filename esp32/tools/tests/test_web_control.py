import pathlib
import re
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
NET_SOURCE = ROOT / "johnny-esp32" / "components" / "jcnet" / "jcnet.c"
CONTROL_SOURCE = ROOT / "johnny-esp32" / "components" / "jccontrol" / "jccontrol.c"


class WebControlSourceTests(unittest.TestCase):
    def test_required_api_routes_are_registered(self):
        source = NET_SOURCE.read_text(encoding="utf-8")
        routes = set(re.findall(r'\.uri = "([^"]+)"', source))
        self.assertTrue(
            {
                "/",
                "/api/v1/setup",
                "/api/v1/session",
                "/api/v1/status",
                "/api/v1/scenes",
                "/api/v1/settings",
                "/api/v1/playback/scene",
                "/api/v1/playback/random",
            }.issubset(routes)
        )

    def test_page_exposes_all_requested_controls(self):
        source = NET_SOURCE.read_text(encoding="utf-8")
        for value in (
            "automatic",
            "day",
            "night",
            "off",
            "halloween",
            "st_patrick",
            "christmas",
            "new_year",
            "Random now",
        ):
            self.assertIn(value, source)

    def test_authentication_uses_pbkdf2_and_hashed_session(self):
        source = NET_SOURCE.read_text(encoding="utf-8")
        self.assertIn("mbedtls_pkcs5_pbkdf2_hmac_ext", source)
        self.assertIn('nvs_set_blob(handle, "verifier"', source)
        self.assertIn('nvs_set_blob(handle, "session"', source)
        self.assertNotIn('nvs_set_str(handle, "admin_password"', source)

    def test_shuffle_is_full_catalog_fisher_yates(self):
        source = CONTROL_SOURCE.read_text(encoding="utf-8")
        self.assertIn("JCENGINE_STORY_SCENE_COUNT", source)
        self.assertIn("for (size_t count = JCENGINE_STORY_SCENE_COUNT; count > 1; --count)", source)
        self.assertIn("shuffle->order[0] == (uint8_t)shuffle->last_scene", source)


if __name__ == "__main__":
    unittest.main()
