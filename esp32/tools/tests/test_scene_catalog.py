import importlib.util
import re
import subprocess
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
GENERATOR_PATH = ROOT / "tools" / "gen_scene_names.py"
SPEC = importlib.util.spec_from_file_location("gen_scene_names", GENERATOR_PATH)
gen_scene_names = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = gen_scene_names
SPEC.loader.exec_module(gen_scene_names)


class SceneCatalogTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.scene_rows = gen_scene_names.validate_scene_rows(
            gen_scene_names.read_rows(
                ROOT / "scene_catalog.csv", gen_scene_names.SCENE_FIELDS
            )
        )
        cls.day_rows = gen_scene_names.validate_special_days(
            gen_scene_names.read_rows(
                ROOT / "special_days.csv", gen_scene_names.SPECIAL_DAY_FIELDS
            )
        )

    def test_html_fixture_extracts_title_and_revision(self):
        audit = gen_scene_names.extract_source_audit(
            "<html><head><title> Johnny  Special Days </title></head>"
            "<body>Last updated on 29 January 2004 at 18:57</body></html>"
        )
        self.assertEqual(audit.title, "Johnny Special Days")
        self.assertEqual(audit.revision, "29 January 2004")

    def test_exact_source_inventory_and_named_coverage(self):
        references = [r for r in self.scene_rows if r["mapping_status"] == "reference"]
        named = [r for r in self.scene_rows
                 if r["mapping_status"] in {"primary", "technical"}]
        identities = [identity for row in named
                      for identity in gen_scene_names.split_mappings(row["mapped_ads"])]
        self.assertEqual(len(references), 14)
        self.assertEqual(len(identities), 63)
        self.assertEqual(len(set(identities)), 63)

    def test_named_identities_match_firmware_story_order(self):
        source = (ROOT / "johnny-esp32/components/jcengine/jcengine_story.c").read_text()
        body = source.split(
            "static const jcengine_story_scene_t STORY_SCENES[] = {", 1
        )[1].split("};", 1)[0]
        firmware = [f"{name}#{tag}" for name, tag in re.findall(
            r'SCENE\("([A-Z0-9_.]+)",\s*(\d+)', body
        )]
        curated = [identity for row in self.scene_rows
                   if row["mapping_status"] in {"primary", "technical"}
                   for identity in gen_scene_names.split_mappings(row["mapped_ads"])]
        self.assertEqual(curated, firmware)

    def test_special_day_boundaries_and_year_wrap(self):
        active = [row for row in self.day_rows if row["mapping_status"] == "active"]
        found = lambda month, day: [row["entry_id"] for row in active if
            gen_scene_names.day_in_range(
                month, day, int(row["start_month"]), int(row["start_day"]),
                int(row["end_month"]), int(row["end_day"])
            )]
        self.assertEqual(found(3, 15), ["ST_PATRICK"])
        self.assertEqual(found(3, 18), [])
        self.assertEqual(found(10, 31), ["HALLOWEEN"])
        self.assertEqual(found(12, 31), ["NEW_YEAR"])
        self.assertEqual(found(1, 1), ["NEW_YEAR"])
        self.assertEqual(found(1, 2), [])

    def test_generated_outputs_are_current(self):
        result = subprocess.run(
            [sys.executable, str(GENERATOR_PATH), "--check"],
            cwd=ROOT, text=True, capture_output=True
        )
        self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
