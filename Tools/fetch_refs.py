"""Fetches the reference recordings of Tools/ref_sets.txt (PLAN 13.4): the audio track only, with yt-dlp.

The files go to a folder outside the repository (default %TEMP%/totality_refs, or --dir), named after the video id, and
stay there (the user's decision of 27.09.2026), so later calibration rounds do not fetch again: a file already present
is skipped. Nothing of the audio enters the repository; the measurements (Tools/analyze_ref.py) keep statistics only.

    python Tools/fetch_refs.py [--dir D:/somewhere] [--only hypnotic]
"""
from __future__ import annotations

import argparse
import os
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent


def default_dir() -> Path:
    return Path(os.environ.get("TEMP", tempfile.gettempdir())) / "totality_refs"


def read_sets(path: Path) -> list[dict]:
    """The rows of ref_sets.txt: profile, name, Beatport tempo and key (or None), URL, video id."""
    rows = []
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = [p.strip() for p in line.split("|")]
        if len(parts) != 4:
            continue
        profile, name, tag, url = parts
        bpm, key = None, None
        if tag != "-":
            t = tag.split()
            bpm = float(t[0])
            key = t[1] if len(t) > 1 else None
        vid = url.split("v=")[-1]
        rows.append({"profile": profile, "name": name, "bpm": bpm, "key": key, "url": url, "id": vid})
    return rows


def local_file(folder: Path, vid: str) -> Path | None:
    for p in folder.glob(vid + ".*"):
        if p.suffix not in (".part", ".ytdl", ".json"):
            return p
    return None


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dir", type=Path, default=default_dir())
    ap.add_argument("--only", default="", help="a profile (ostgut, dub, raw, hypnotic)")
    args = ap.parse_args()
    args.dir.mkdir(parents=True, exist_ok=True)
    rows = [r for r in read_sets(HERE / "ref_sets.txt") if not args.only or r["profile"] == args.only]
    failed = 0
    for r in rows:
        if local_file(args.dir, r["id"]) is not None:
            print(f"have  {r['name']}")
            continue
        print(f"fetch {r['name']}", flush=True)
        cmd = ["yt-dlp", "--no-warnings", "--no-playlist", "-f", "bestaudio", "-o", str(args.dir / "%(id)s.%(ext)s"), r["url"]]
        if subprocess.run(cmd).returncode != 0 or local_file(args.dir, r["id"]) is None:
            print(f"  FAILED {r['url']}", file=sys.stderr)
            failed += 1
    print(f"{len(rows) - failed} of {len(rows)} present in {args.dir}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
