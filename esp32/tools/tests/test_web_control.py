import ast
import pathlib
import re
import subprocess
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
NET_SOURCE = ROOT / "johnny-esp32" / "components" / "jcnet" / "jcnet.c"
CONTROL_SOURCE = ROOT / "johnny-esp32" / "components" / "jccontrol" / "jccontrol.c"
GFX_SOURCE = ROOT / "johnny-esp32" / "components" / "jcgfx" / "jcgfx.c"
MAIN_SOURCE = ROOT / "johnny-esp32" / "main" / "main.c"
FAVICON_SOURCE = ROOT / "johnny-esp32" / "components" / "jcnet" / "favicon.svg"
WINDOWS_FAVICON_SOURCE = ROOT.parent / "assets" / "icons" / "candidates" / "castaway-lookout.svg"


class WebControlSourceTests(unittest.TestCase):
    def test_required_api_routes_are_registered(self):
        source = NET_SOURCE.read_text(encoding="utf-8")
        routes = set(re.findall(r'\.uri = "([^"]+)"', source))
        self.assertTrue(
            {
                "/",
                "/favicon.svg",
                "/api/v1/setup",
                "/api/v1/session",
                "/api/v1/status",
                "/api/v1/scenes",
                "/api/v1/settings",
                "/api/v1/weather/search",
                "/api/v1/playback/scene",
                "/api/v1/playback/random",
                "/api/v1/playback/review",
                "/api/v1/bugs",
                "/api/v1/bugs/resolve",
            }.issubset(routes)
        )

    def test_esp32_favicon_matches_windows_build_icon(self):
        source = NET_SOURCE.read_text(encoding="utf-8")
        self.assertEqual(
            FAVICON_SOURCE.read_text(encoding="utf-8"),
            WINDOWS_FAVICON_SOURCE.read_text(encoding="utf-8"),
        )
        self.assertIn("<link rel=icon href=/favicon.svg type=image/svg+xml>", source)
        self.assertIn('httpd_resp_set_type(request, "image/svg+xml")', source)

    def test_page_exposes_all_requested_controls(self):
        source = NET_SOURCE.read_text(encoding="utf-8")
        for value in (
            "automatic",
            "day",
            "night",
            "cycle",
            "normal",
            "review",
            "Clock &amp; weather",
            "Reviewer",
            "City or postal code",
            "off",
            "halloween",
            "st_patrick",
            "christmas",
            "new_year",
            "Random now",
            "Looks OK",
            "Bug Log",
            "Copy All",
        ):
            self.assertIn(value, source)

    def test_authentication_uses_pbkdf2_and_hashed_session(self):
        source = NET_SOURCE.read_text(encoding="utf-8")
        self.assertIn("mbedtls_pkcs5_pbkdf2_hmac_ext", source)
        self.assertIn('nvs_set_blob(handle, "verifier"', source)
        self.assertIn('nvs_set_blob(handle, "session"', source)
        self.assertNotIn('nvs_set_str(handle, "admin_password"', source)

    def test_login_form_has_unambiguous_dom_bindings_and_visible_errors(self):
        source = NET_SOURCE.read_text(encoding="utf-8")
        self.assertIn("id=loginPanel", source)
        self.assertIn("id=loginMsg", source)
        self.assertIn("function submitLogin()", source)
        self.assertIn("el('loginMsg').textContent=e.message", source)
        self.assertNotIn("id=login class=card", source)
        self.assertNotIn("function login()", source)

    def test_embedded_control_page_javascript_parses(self):
        source = NET_SOURCE.read_text(encoding="utf-8")
        segment = source.split("static const char CONTROL_PAGE[] =", 1)[1]
        segment = segment.split("static const char SETUP_PAGE[] =", 1)[0]
        page = "".join(
            ast.literal_eval(token)
            for token in re.findall(r'"(?:\\.|[^"\\])*"', segment)
        )
        script = page.split("<script>", 1)[1].split("</script>", 1)[0]
        result = subprocess.run(
            ["node", "--check", "-"],
            input=script,
            text=True,
            capture_output=True,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_shuffle_is_full_catalog_fisher_yates(self):
        source = CONTROL_SOURCE.read_text(encoding="utf-8")
        self.assertIn("JCENGINE_STORY_SCENE_COUNT", source)
        self.assertIn("for (size_t count = JCENGINE_STORY_SCENE_COUNT; count > 1; --count)", source)
        self.assertIn("shuffle->order[0] == (uint8_t)shuffle->last_scene", source)

    def test_bug_log_is_separate_persistent_and_sanitized(self):
        control = CONTROL_SOURCE.read_text(encoding="utf-8")
        page = NET_SOURCE.read_text(encoding="utf-8")
        self.assertIn('nvs_open("jc_bug"', control)
        self.assertIn("jccontrol_bug_capture", control)
        self.assertIn("catalog_fingerprint", page)
        self.assertIn("firmware", page)
        self.assertNotIn("wifi_password", page[page.index("static esp_err_t bugs_handler"):])

    def test_settings_schema_migrates_reviewer_to_sidebar_mode(self):
        source = CONTROL_SOURCE.read_text(encoding="utf-8")
        self.assertIn("SETTINGS_SCHEMA = 4", source)
        self.assertIn("schema >= 1 && schema <= SETTINGS_SCHEMA", source)
        self.assertIn('nvs_set_u8(handle, "playback"', source)
        self.assertIn('nvs_get_u8(handle, "reviewer"', source)
        self.assertIn('nvs_set_u8(handle, "sidebar"', source)
        self.assertIn("reviewer_visible ? JCCONTROL_SIDEBAR_REVIEW", source)
        self.assertIn(".sidebar_mode = JCCONTROL_SIDEBAR_OFF", source)

    def test_sidebar_mode_is_authenticated_string_setting(self):
        source = NET_SOURCE.read_text(encoding="utf-8")
        settings = source[source.index("static esp_err_t settings_handler"):]
        self.assertIn('cJSON_GetObjectItem(root, "sidebar_mode")', settings)
        self.assertIn("cJSON_IsString(sidebar_mode)", settings)
        self.assertIn("jccontrol_parse_sidebar_mode", settings)
        self.assertNotIn('cJSON_GetObjectItem(root, "reviewer_visible")', settings)

    def test_off_and_clock_modes_disable_reviewer_touch_controls(self):
        main = MAIN_SOURCE.read_text(encoding="utf-8")
        gfx = GFX_SOURCE.read_text(encoding="utf-8")
        sidebar = main[main.index("static void draw_live_story_sidebar"):]
        self.assertLess(sidebar.index("!network.provisioned"),
                        sidebar.index("JCCONTROL_SIDEBAR_OFF"))
        self.assertIn("jcgfx_clear_sidebar", sidebar)
        self.assertIn("live_story.sidebar_mode == JCCONTROL_SIDEBAR_REVIEW", main)
        self.assertIn(": JCGFX_VALIDATION_CONTROL_NONE", main)
        self.assertIn("VALIDATION_SIDEBAR_X, 0, 160, 480", gfx)

    def test_weather_search_and_forecast_are_bounded_and_authenticated(self):
        source = NET_SOURCE.read_text(encoding="utf-8")
        search = source[source.index("static esp_err_t weather_search_handler"):]
        self.assertIn("request_authenticated(request)", search)
        self.assertIn("WEATHER_JSON_MAX = 6144", source)
        self.assertIn("count=3", source)
        self.assertIn("geocoding-api.open-meteo.com", source)
        self.assertIn("api.open-meteo.com/v1/forecast", source)
        self.assertIn("WEATHER_REFRESH_SECONDS = 45 * 60", source)

    def test_forecast_parser_fixture_and_clock_renderer_are_present(self):
        net = NET_SOURCE.read_text(encoding="utf-8")
        gfx = GFX_SOURCE.read_text(encoding="utf-8")
        main = MAIN_SOURCE.read_text(encoding="utf-8")
        self.assertIn("Open-Meteo forecast parser fixture PASS", net)
        self.assertIn('"temperature_2m_max"', net)
        self.assertIn('"temperature_2m_min"', net)
        self.assertIn("jcgfx_draw_clock_weather_sidebar", gfx)
        for label in ("SYNCING TIME", "WAITING", "STALE DATA", "CURRENT"):
            self.assertIn(label, gfx)
        self.assertIn('"UPDATED %02u:%02u"', gfx)
        self.assertIn('"UPDATED --:--"', gfx)
        self.assertNotIn('"OPEN METEO"', gfx)
        self.assertIn('"DATA FROM METEO"', gfx)
        self.assertIn("weather.updated_at", main)
        self.assertIn("weather_updated_valid", main)

    def test_city_results_are_rendered_as_text_not_provider_markup(self):
        source = NET_SOURCE.read_text(encoding="utf-8")
        self.assertIn("b.textContent=x.name", source)
        self.assertIn("box.appendChild(b)", source)
        self.assertNotIn("r.locations.map(x=>`<button", source)


if __name__ == "__main__":
    unittest.main()
