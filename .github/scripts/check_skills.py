#!/usr/bin/env python3
"""Validate the skill packages in this repository.

Checks, for every directory under skills/:
  * SKILL.md exists and starts with a YAML front matter block
  * front matter declares name, description and license
  * name matches the directory name
  * LICENSE and agents/openai.yaml exist
  * every relative Markdown link inside the package resolves

Exits non-zero on the first class of failure, listing all offenders.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SKILLS = ROOT / "skills"

FRONT_MATTER = re.compile(r"^---\r?\n(.*?)\r?\n---\r?\n", re.S)
LINK = re.compile(r"\]\((\.{0,2}[^)\s]+\.md)\)")

errors: list[str] = []


def front_matter(text: str) -> dict[str, str] | None:
    m = FRONT_MATTER.match(text)
    if not m:
        return None
    fields: dict[str, str] = {}
    for line in m.group(1).splitlines():
        if ":" in line and not line.startswith((" ", "\t")):
            key, _, value = line.partition(":")
            fields[key.strip()] = value.strip()
    return fields


def check_links(md: Path) -> None:
    for target in LINK.findall(md.read_text(encoding="utf-8")):
        resolved = (md.parent / target).resolve()
        if not resolved.exists():
            errors.append(f"{md.relative_to(ROOT)}: broken link -> {target}")


def main() -> int:
    if not SKILLS.is_dir():
        print("skills/ directory is missing", file=sys.stderr)
        return 1

    dirs = sorted(p for p in SKILLS.iterdir() if p.is_dir())
    if not dirs:
        print("no skill directories found", file=sys.stderr)
        return 1

    for skill in dirs:
        name = skill.name
        skill_md = skill / "SKILL.md"
        if not skill_md.is_file():
            errors.append(f"{name}: SKILL.md missing")
            continue

        meta = front_matter(skill_md.read_text(encoding="utf-8"))
        if meta is None:
            errors.append(f"{name}: SKILL.md has no front matter")
        else:
            for field in ("name", "description", "license"):
                if not meta.get(field):
                    errors.append(f"{name}: front matter missing '{field}'")
            declared = meta.get("name")
            if declared and declared != name:
                errors.append(f"{name}: front matter name is '{declared}'")

        for required in ("LICENSE", "agents/openai.yaml"):
            if not (skill / required).is_file():
                errors.append(f"{name}: {required} missing")

        for md in skill.rglob("*.md"):
            check_links(md)

    print(f"checked {len(dirs)} skill(s)")
    if errors:
        print("\n".join(f"  ERROR {e}" for e in errors), file=sys.stderr)
        return 1
    print("all checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
