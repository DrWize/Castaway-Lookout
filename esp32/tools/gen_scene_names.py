#!/usr/bin/env python3
"""Validate curated Johnny metadata and generate offline ESP32 catalog data."""

from __future__ import annotations

import argparse
import csv
import html
import re
import sys
from dataclasses import dataclass
from datetime import date
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCENE_CSV = ROOT / "scene_catalog.csv"
SPECIAL_DAY_CSV = ROOT / "special_days.csv"
GENERATED_C = ROOT / "johnny-esp32/components/jcengine/scene_names.c"
GENERATED_QA = ROOT / "SCENE_CATALOG.md"

SCENE_FIELDS = (
    "entry_id", "category", "event_name", "display_title", "source_url",
    "source_fragment", "mapped_ads", "mapped_ttm", "mapping_status", "notes",
)
SPECIAL_DAY_FIELDS = (
    "entry_id", "title", "start_month", "start_day", "end_month", "end_day",
    "overlay_id", "mapping_status", "source_url", "notes",
)
ADS_RE = re.compile(r"^[A-Z0-9_]+\.ADS#[1-9][0-9]*$")
TTM_RE = re.compile(r"^[A-Z0-9_]+\.TTM#[1-9][0-9]*$")
DISPLAY_RE = re.compile(r"^[A-Z0-9 +./-]+$")
REVISION_RE = re.compile(
    r"Last\s+updated\s+on\s+([0-9]{1,2}\s+[A-Za-z]+\s+[0-9]{4})",
    re.IGNORECASE,
)
TITLE_RE = re.compile(r"<title[^>]*>(.*?)</title>", re.IGNORECASE | re.DOTALL)


class CatalogError(ValueError):
    pass


@dataclass(frozen=True)
class SourceAudit:
    title: str
    revision: str


def extract_source_audit(document: str) -> SourceAudit:
    """Extract the small auditable facts used by the one-time source inventory."""
    title_match = TITLE_RE.search(document)
    revision_match = REVISION_RE.search(document)
    if title_match is None or revision_match is None:
        raise CatalogError("source page lacks a title or Last updated date")
    title = re.sub(r"\s+", " ", html.unescape(title_match.group(1))).strip()
    return SourceAudit(title, revision_match.group(1))


def read_rows(path: Path, expected_fields: tuple[str, ...]) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8-sig") as stream:
        reader = csv.DictReader(stream)
        if tuple(reader.fieldnames or ()) != expected_fields:
            raise CatalogError(f"{path.name}: expected columns {','.join(expected_fields)}")
        rows = [{key: (value or "").strip() for key, value in row.items()} for row in reader]
    if not rows:
        raise CatalogError(f"{path.name}: no rows")
    return rows


def split_mappings(value: str) -> list[str]:
    return [item.strip() for item in value.split(";") if item.strip()]


def validate_scene_rows(rows: list[dict[str, str]]) -> list[dict[str, str]]:
    entry_ids: set[str] = set()
    ads_titles: dict[str, str] = {}
    source_pages: set[str] = set()
    allowed_status = {"primary", "technical", "checklist-only", "reference"}
    for number, row in enumerate(rows, 2):
        entry_id = row["entry_id"]
        if not entry_id or entry_id in entry_ids:
            raise CatalogError(f"scene_catalog.csv:{number}: duplicate or empty entry_id")
        entry_ids.add(entry_id)
        if row["mapping_status"] not in allowed_status:
            raise CatalogError(f"scene_catalog.csv:{number}: invalid mapping_status")
        if row["mapping_status"] == "reference":
            if row["mapped_ads"] or row["mapped_ttm"] or row["display_title"]:
                raise CatalogError(f"scene_catalog.csv:{number}: reference row has scene data")
            source_pages.add(row["source_url"])
            continue
        for identity in split_mappings(row["mapped_ads"]):
            if ADS_RE.fullmatch(identity) is None:
                raise CatalogError(f"scene_catalog.csv:{number}: invalid ADS identity {identity}")
        for identity in split_mappings(row["mapped_ttm"]):
            if TTM_RE.fullmatch(identity) is None:
                raise CatalogError(f"scene_catalog.csv:{number}: invalid TTM identity {identity}")
        if row["mapping_status"] in {"primary", "technical"}:
            title = row["display_title"]
            identities = split_mappings(row["mapped_ads"])
            if not identities or not title:
                raise CatalogError(f"scene_catalog.csv:{number}: named row lacks ADS identity or title")
            if len(title) > 31 or DISPLAY_RE.fullmatch(title) is None:
                raise CatalogError(f"scene_catalog.csv:{number}: title is not panel-safe: {title}")
            for identity in identities:
                if identity in ads_titles:
                    raise CatalogError(f"scene_catalog.csv:{number}: duplicate primary ADS {identity}")
                ads_titles[identity] = title
        elif row["display_title"]:
            raise CatalogError(f"scene_catalog.csv:{number}: checklist-only row has display title")
    if len(ads_titles) != 63:
        raise CatalogError(f"scene_catalog.csv: expected 63 named ADS identities, found {len(ads_titles)}")
    expected_sources = {
        f"https://johnny-castaway.com/{name}"
        for name in (
            "list.html", "annivers.html", "bugs.html", "common.html",
            "fishing.html", "leaving.html", "mermaid.html", "pirates.html",
            "reading.html", "seagull.html", "story.html", "swimming.html",
            "unusual.html", "visitors.html",
        )
    }
    if source_pages != expected_sources:
        missing = sorted(expected_sources - source_pages)
        extra = sorted(source_pages - expected_sources)
        raise CatalogError(f"scene source inventory mismatch; missing={missing} extra={extra}")
    return rows


def valid_day(month: int, day: int) -> bool:
    try:
        date(2000, month, day)
        return True
    except ValueError:
        return False


def day_in_range(month: int, day: int, start_month: int, start_day: int,
                 end_month: int, end_day: int) -> bool:
    value = month * 100 + day
    start = start_month * 100 + start_day
    end = end_month * 100 + end_day
    return start <= value <= end if start <= end else value >= start or value <= end


def validate_special_days(rows: list[dict[str, str]]) -> list[dict[str, str]]:
    active: list[tuple[dict[str, str], tuple[int, int, int, int]]] = []
    ids: set[str] = set()
    overlays: set[int] = set()
    for number, row in enumerate(rows, 2):
        if not row["entry_id"] or row["entry_id"] in ids:
            raise CatalogError(f"special_days.csv:{number}: duplicate or empty entry_id")
        ids.add(row["entry_id"])
        status = row["mapping_status"]
        if status == "checklist-only":
            if any(row[field] for field in SPECIAL_DAY_FIELDS[2:7]):
                raise CatalogError(f"special_days.csv:{number}: unresolved day has date or overlay")
            continue
        if status != "active":
            raise CatalogError(f"special_days.csv:{number}: invalid mapping_status")
        try:
            values = tuple(int(row[field]) for field in SPECIAL_DAY_FIELDS[2:7])
        except ValueError as exc:
            raise CatalogError(f"special_days.csv:{number}: non-numeric date or overlay") from exc
        sm, sd, em, ed, overlay = values
        if not valid_day(sm, sd) or not valid_day(em, ed):
            raise CatalogError(f"special_days.csv:{number}: invalid date boundary")
        if overlay < 1 or overlay > 4 or overlay in overlays:
            raise CatalogError(f"special_days.csv:{number}: invalid or duplicate overlay")
        overlays.add(overlay)
        active.append((row, (sm, sd, em, ed)))
    if len(active) != 4 or overlays != {1, 2, 3, 4}:
        raise CatalogError("special_days.csv: expected four active overlays numbered 1..4")
    for month in range(1, 13):
        for day in range(1, 32):
            if not valid_day(month, day):
                continue
            matches = [row["entry_id"] for row, limits in active
                       if day_in_range(month, day, *limits)]
            if len(matches) > 1:
                raise CatalogError(f"special_days.csv: overlapping rules on {month:02d}-{day:02d}: {matches}")
    return rows


def c_string(value: str) -> str:
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def identity_parts(identity: str) -> tuple[str, int]:
    name, tag = identity.split("#", 1)
    return name, int(tag)


def generate_c(scene_rows: list[dict[str, str]], day_rows: list[dict[str, str]]) -> str:
    named = [row for row in scene_rows if row["mapping_status"] in {"primary", "technical"}]
    menu = named
    days = [row for row in day_rows if row["mapping_status"] == "active"]
    lines = [
        "/* Generated by tools/gen_scene_names.py. Do not edit. */",
        '#include "jcengine.h"',
        '#include "sdkconfig.h"',
        "",
        "#include <string.h>",
        "",
        "typedef struct {",
        "    const char *ads_name;",
        "    uint16_t ads_tag;",
        "    const char *title;",
        "} scene_name_record_t;",
        "",
        "#if CONFIG_JC_SCENE_NAMES",
        "static const scene_name_record_t SCENE_NAMES[] = {",
    ]
    for row in named:
        for identity in split_mappings(row["mapped_ads"]):
            name, tag = identity_parts(identity)
            lines.append(f"    {{{c_string(name)}, {tag}, {c_string(row['display_title'])}}},")
    lines += [
        "};",
        "",
        "static const jcengine_scene_menu_entry_t SCENE_MENU[] = {",
    ]
    for row in menu:
        lines.append(
            "    {" + ", ".join(c_string(row[field]) for field in
                                  ("entry_id", "category", "event_name", "mapped_ads", "mapped_ttm")) + "},"
        )
    lines += [
        "};",
        "#endif",
        "",
        "static const jcengine_special_day_t SPECIAL_DAYS[] = {",
    ]
    for row in days:
        lines.append(
            f"    {{{c_string(row['entry_id'])}, {c_string(row['title'])}, "
            f"{row['start_month']}, {row['start_day']}, {row['end_month']}, "
            f"{row['end_day']}, {row['overlay_id']}}},"
        )
    lines += [
        "};",
        "",
        "const char *jcengine_scene_title(const char *ads_name, uint16_t ads_tag)",
        "{",
        "#if CONFIG_JC_SCENE_NAMES",
        "    if (ads_name == NULL) return NULL;",
        "    for (size_t index = 0; index < sizeof(SCENE_NAMES) / sizeof(SCENE_NAMES[0]); ++index) {",
        "        if (SCENE_NAMES[index].ads_tag == ads_tag &&",
        "            strcmp(SCENE_NAMES[index].ads_name, ads_name) == 0) {",
        "            return SCENE_NAMES[index].title;",
        "        }",
        "    }",
        "#else",
        "    (void)ads_name;",
        "    (void)ads_tag;",
        "#endif",
        "    return NULL;",
        "}",
        "",
        "size_t jcengine_scene_menu_count(void)",
        "{",
        "#if CONFIG_JC_SCENE_NAMES",
        "    return sizeof(SCENE_MENU) / sizeof(SCENE_MENU[0]);",
        "#else",
        "    return 0;",
        "#endif",
        "}",
        "",
        "const jcengine_scene_menu_entry_t *jcengine_scene_menu_entry(size_t index)",
        "{",
        "#if CONFIG_JC_SCENE_NAMES",
        "    return index < jcengine_scene_menu_count() ? &SCENE_MENU[index] : NULL;",
        "#else",
        "    (void)index;",
        "    return NULL;",
        "#endif",
        "}",
        "",
        "size_t jcengine_special_day_count(void)",
        "{",
        "    return sizeof(SPECIAL_DAYS) / sizeof(SPECIAL_DAYS[0]);",
        "}",
        "",
        "const jcengine_special_day_t *jcengine_special_day(size_t index)",
        "{",
        "    return index < jcengine_special_day_count() ? &SPECIAL_DAYS[index] : NULL;",
        "}",
        "",
        "const jcengine_special_day_t *jcengine_special_day_for_date(uint8_t month, uint8_t day)",
        "{",
        "    uint16_t value = (uint16_t)month * 100U + day;",
        "    for (size_t index = 0; index < jcengine_special_day_count(); ++index) {",
        "        const jcengine_special_day_t *item = &SPECIAL_DAYS[index];",
        "        uint16_t start = (uint16_t)item->start_month * 100U + item->start_day;",
        "        uint16_t end = (uint16_t)item->end_month * 100U + item->end_day;",
        "        bool match = start <= end ? value >= start && value <= end",
        "                                  : value >= start || value <= end;",
        "        if (match) return item;",
        "    }",
        "    return NULL;",
        "}",
        "",
    ]
    return "\n".join(lines)


def markdown_cell(value: str) -> str:
    return value.replace("|", "\\|")


def generate_qa(scene_rows: list[dict[str, str]], day_rows: list[dict[str, str]]) -> str:
    sources = [row for row in scene_rows if row["mapping_status"] == "reference"]
    entries = [row for row in scene_rows if row["mapping_status"] != "reference"]
    named = [row for row in entries if row["mapping_status"] in {"primary", "technical"}]
    lines = [
        "# Scene Catalog Integration QA",
        "",
        "Generated from `scene_catalog.csv` and `special_days.csv` by `tools/gen_scene_names.py`.",
        "No website HTML or images are archived.",
        "",
        f"- Named ESP32 ADS identities: **{sum(len(split_mappings(r['mapped_ads'])) for r in named)} / 63**",
        f"- Audited source pages: **{len(sources)}** (`list.html` plus 13 reference/detail pages)",
        f"- Catalog/checklist entries: **{len(entries)}**",
        "",
        "## Source inventory",
        "",
        "| Page | Classification | Site revision |",
        "| --- | --- | --- |",
    ]
    for row in sources:
        lines.append(f"| [{markdown_cell(row['event_name'])}]({row['source_url']}) | {row['category']} | {markdown_cell(row['notes'])} |")
    lines += [
        "",
        "## Scene mappings",
        "",
        "| Entry | Category | Friendly title | ADS mapping | TTM mapping | Status |",
        "| --- | --- | --- | --- | --- | --- |",
    ]
    for row in entries:
        lines.append("| " + " | ".join(markdown_cell(row[field]) for field in
                     ("entry_id", "category", "display_title", "mapped_ads", "mapped_ttm", "mapping_status")) + " |")
    lines += [
        "",
        "## Special Day date rules",
        "",
        "These are calendar overlays, not playable ADS scenes.",
        "",
        "| Entry | Title | Range | Overlay | Status | Notes |",
        "| --- | --- | --- | ---: | --- | --- |",
    ]
    for row in day_rows:
        date_range = "UNRESOLVED" if row["mapping_status"] != "active" else (
            f"{int(row['start_month']):02d}-{int(row['start_day']):02d} to "
            f"{int(row['end_month']):02d}-{int(row['end_day']):02d}"
        )
        lines.append(f"| {row['entry_id']} | {row['title']} | {date_range} | {row['overlay_id']} | {row['mapping_status']} | {markdown_cell(row['notes'])} |")
    lines.append("")
    return "\n".join(lines)


def write_or_check(path: Path, content: str, check: bool) -> bool:
    content = content.replace("\r\n", "\n")
    current = path.read_text(encoding="utf-8").replace("\r\n", "\n") if path.exists() else None
    if current == content:
        return True
    if check:
        print(f"stale generated file: {path.relative_to(ROOT)}", file=sys.stderr)
        return False
    path.write_text(content, encoding="utf-8", newline="\n")
    return True


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true", help="fail if generated outputs are stale")
    args = parser.parse_args()
    try:
        scenes = validate_scene_rows(read_rows(SCENE_CSV, SCENE_FIELDS))
        days = validate_special_days(read_rows(SPECIAL_DAY_CSV, SPECIAL_DAY_FIELDS))
    except (CatalogError, OSError) as exc:
        print(exc, file=sys.stderr)
        return 1
    ok = write_or_check(GENERATED_C, generate_c(scenes, days), args.check)
    ok = write_or_check(GENERATED_QA, generate_qa(scenes, days), args.check) and ok
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
