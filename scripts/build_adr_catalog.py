#!/usr/bin/env python3
"""
Build a structured ADR catalog from markdown files in adr/.

Usage:
  python3 scripts/build_adr_catalog.py
  python3 scripts/build_adr_catalog.py --with-stamped
"""

from __future__ import annotations

import argparse
import datetime as dt
import json
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional


ADR_FILENAME_RE = re.compile(r"^(?:IW_)?ADR-(\d{3})_(.+)\.md$")
ADR_TITLE_MD_RE = re.compile(r"^#+\s*(ADR-\d{3})\s*[:\-\u2014]\s*(.+?)\s*$", re.MULTILINE)
ADR_TITLE_PLAIN_RE = re.compile(r"^(ADR-\d{3})\s*[:\-\u2014]\s*(.+?)\s*$", re.MULTILINE)
ADR_REF_RE = re.compile(r"ADR-\d{3}")

KNOWN_STATUSES = ["Proposed", "Accepted", "Rejected", "Superseded", "Deprecated"]


@dataclass
class ADRRecord:
    adr_id: str
    number: int
    title: str
    status: str
    date: str
    decision_type: str
    path: str
    supersedes: List[str]
    superseded_by: List[str]

    def to_json(self) -> Dict[str, object]:
        return {
            "adrId": self.adr_id,
            "number": self.number,
            "title": self.title,
            "status": self.status,
            "date": self.date,
            "decisionType": self.decision_type,
            "path": self.path,
            "supersedes": self.supersedes,
            "supersededBy": self.superseded_by,
        }


def _clean_value(value: str) -> str:
    cleaned = value.strip().replace("**", "")
    return cleaned.strip("` ").rstrip()


def _extract_inline_field(text: str, field_name: str) -> Optional[str]:
    patterns = [
        rf"^\*\*{re.escape(field_name)}:\*\*\s*(.+?)\s*$",
        rf"^{re.escape(field_name)}\s*:\s*(.+?)\s*$",
        rf"^{re.escape(field_name)}\s+(.+?)\s*$",
    ]
    for pattern in patterns:
        match = re.search(pattern, text, flags=re.MULTILINE | re.IGNORECASE)
        if match:
            return _clean_value(match.group(1))
    return None


def _extract_block_field(text: str, field_name: str) -> Optional[str]:
    lines = text.splitlines()
    for i, line in enumerate(lines):
        if line.strip().lower() != field_name.lower():
            continue
        for candidate in lines[i + 1 :]:
            value = candidate.strip()
            if not value:
                continue
            if value.startswith("#") or value.startswith("|"):
                break
            if set(value) <= {"-", "_"}:
                continue
            return _clean_value(value)
    return None


def _extract_field(text: str, field_name: str) -> Optional[str]:
    return _extract_inline_field(text, field_name) or _extract_block_field(text, field_name)


def _extract_adr_refs(field_value: Optional[str]) -> List[str]:
    if not field_value:
        return []
    refs = ADR_REF_RE.findall(field_value)
    unique: List[str] = []
    for ref in refs:
        if ref not in unique:
            unique.append(ref)
    return unique


def _normalize_status(status: Optional[str]) -> str:
    if not status:
        return "Unknown"
    lowered = status.strip().lower()
    for known in KNOWN_STATUSES:
        if lowered.startswith(known.lower()):
            return known
    return status.strip()


def parse_adr(path: Path, repo_root: Path) -> Optional[ADRRecord]:
    filename_match = ADR_FILENAME_RE.match(path.name)
    if not filename_match:
        return None

    text = path.read_text(encoding="utf-8")
    number = int(filename_match.group(1))
    fallback_id = f"ADR-{number:03d}"
    fallback_title = filename_match.group(2).replace("_", " ")

    title_match = ADR_TITLE_MD_RE.search(text) or ADR_TITLE_PLAIN_RE.search(text)
    if title_match:
        adr_id = title_match.group(1).strip()
        title = title_match.group(2).strip()
    else:
        adr_id = fallback_id
        title = fallback_title

    status = _normalize_status(_extract_field(text, "Status"))
    date = _extract_field(text, "Date") or "Unknown"
    decision_type = _extract_field(text, "Decision Type") or "Unspecified"
    supersedes = _extract_adr_refs(_extract_field(text, "Supersedes"))
    superseded_by = _extract_adr_refs(_extract_field(text, "Superseded by"))

    relative_path = str(path.resolve().relative_to(repo_root.resolve()))

    return ADRRecord(
        adr_id=adr_id,
        number=number,
        title=title,
        status=status,
        date=date,
        decision_type=decision_type,
        path=relative_path,
        supersedes=supersedes,
        superseded_by=superseded_by,
    )


def build_catalog(records: List[ADRRecord], adr_dir: Path) -> Dict[str, object]:
    by_status: Dict[str, int] = {}
    for record in records:
        by_status[record.status] = by_status.get(record.status, 0) + 1

    dated_records = [record.date for record in records if re.match(r"^\d{4}-\d{2}-\d{2}$", record.date)]
    generated_at = max(dated_records) if dated_records else "Unknown"

    return {
        "schemaVersion": "adr_catalog.v1",
        "reportType": "ArchitectureDecisionIndex",
        "type": "governance_observational_synthesis",
        "generatedAt": generated_at,
        "sourceDirectory": str(adr_dir),
        "decisionCount": len(records),
        "statusBreakdown": by_status,
        "decisions": [record.to_json() for record in records],
    }


def build_markdown(catalog: Dict[str, object]) -> str:
    rows = []
    decisions = catalog.get("decisions", [])
    for decision in decisions:
        supersedes = ", ".join(decision.get("supersedes", [])) or "-"
        superseded_by = ", ".join(decision.get("supersededBy", [])) or "-"
        rows.append(
            "| {adr} | {status} | {date} | {dtype} | {path} | {supersedes} | {superseded_by} |".format(
                adr=decision.get("adrId", ""),
                status=decision.get("status", ""),
                date=decision.get("date", ""),
                dtype=decision.get("decisionType", ""),
                path=decision.get("path", ""),
                supersedes=supersedes,
                superseded_by=superseded_by,
            )
        )

    status_breakdown = catalog.get("statusBreakdown", {})
    status_lines = [f"- `{status}`: {count}" for status, count in sorted(status_breakdown.items())]
    if not status_lines:
        status_lines = ["- (none)"]

    lines = [
        "# Architecture Decision Index",
        "",
        f"Generated at: `{catalog.get('generatedAt', 'Unknown')}`",
        f"Source directory: `{catalog.get('sourceDirectory', '')}`",
        f"Decision count: `{catalog.get('decisionCount', 0)}`",
        "",
        "## Status Breakdown",
        "",
        *status_lines,
        "",
        "## Decisions",
        "",
        "| ADR | Status | Date | Decision Type | Path | Supersedes | Superseded By |",
        "| --- | --- | --- | --- | --- | --- | --- |",
        *rows,
        "",
    ]
    return "\n".join(lines)


def write_outputs(
    catalog: Dict[str, object],
    markdown: str,
    out_json: Path,
    out_md: Path,
    with_stamped: bool,
) -> None:
    out_json.parent.mkdir(parents=True, exist_ok=True)
    out_md.parent.mkdir(parents=True, exist_ok=True)

    out_json.write_text(json.dumps(catalog, indent=2, ensure_ascii=True) + "\n", encoding="utf-8")
    out_md.write_text(markdown, encoding="utf-8")

    if not with_stamped:
        return

    stamp = dt.datetime.now().strftime("%Y%m%d_%H%M%S")
    stamped_json = out_json.with_name(out_json.stem.replace(".latest", "") + f"_{stamp}.json")
    stamped_md = out_md.with_name(out_md.stem.replace(".latest", "") + f"_{stamp}.md")
    stamped_json.write_text(json.dumps(catalog, indent=2, ensure_ascii=True) + "\n", encoding="utf-8")
    stamped_md.write_text(markdown, encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Build structured ADR catalog artifacts.")
    parser.add_argument("--adr-dir", default="adr", help="Directory containing canonical ADR markdown files.")
    parser.add_argument(
        "--out-json",
        default="reports/architecture/ArchitectureDecisionIndex.latest.json",
        help="Output JSON path.",
    )
    parser.add_argument(
        "--out-md",
        default="reports/architecture/ArchitectureDecisionIndex.latest.md",
        help="Output Markdown path.",
    )
    parser.add_argument(
        "--with-stamped",
        action="store_true",
        help="Also write timestamped copies next to latest artifacts.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    repo_root = Path.cwd()
    adr_dir = Path(args.adr_dir)
    out_json = Path(args.out_json)
    out_md = Path(args.out_md)

    if not adr_dir.exists() or not adr_dir.is_dir():
        raise SystemExit(f"ADR directory not found: {adr_dir}")

    records: List[ADRRecord] = []
    for path in sorted(adr_dir.iterdir()):
        if not path.is_file():
            continue
        record = parse_adr(path, repo_root)
        if record:
            records.append(record)

    records.sort(key=lambda item: item.number)

    seen_ids: Dict[str, str] = {}
    seen_numbers: Dict[int, str] = {}
    for record in records:
        if record.adr_id in seen_ids:
            raise SystemExit(
                f"Duplicate ADR id detected: {record.adr_id} in {record.path} and {seen_ids[record.adr_id]}"
            )
        seen_ids[record.adr_id] = record.path

        if record.number in seen_numbers:
            raise SystemExit(
                f"Duplicate ADR number detected: ADR-{record.number:03d} in {record.path} and {seen_numbers[record.number]}"
            )
        seen_numbers[record.number] = record.path

    catalog = build_catalog(records, adr_dir)
    markdown = build_markdown(catalog)
    write_outputs(catalog, markdown, out_json, out_md, args.with_stamped)

    print(f"[ADR Catalog] parsed={len(records)} out_json={out_json} out_md={out_md}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
