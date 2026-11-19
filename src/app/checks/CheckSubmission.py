"""Check ZIP contents and local document links without extracting or changing it.

Run from the source root:
    python src/app/checks/CheckSubmission.py build/package/24I-3071_24I-3000_BSE-5B.zip
Add --source-root PATH to compare source, data, report and results bytes against
the reviewed tree. This does not replace a clean build or gameplay acceptance.
"""
import argparse
import hashlib
from pathlib import Path, PurePosixPath
import posixpath
import re
from urllib.parse import unquote, urlsplit
from zipfile import ZipFile


def inspect(archive, source_root=None):
    errors = []
    required = {
        "CMakeLists.txt", "README.md", "bin/smartrace.exe",
        "third_party/raylib/lib/raylib.lib", "third_party/raylib/LICENSE",
        "third_party/raygui/raygui.h", "third_party/raygui/LICENSE",
        "report/24I-3071_24I-3000_BSE-5B_Report.pdf",
        "report/diagrams/architecture.png", "report/diagrams/architecture.puml",
        "docs/gui-verification.md", "docs/gui-partner-review.md",
        "docs/gui-next-implementation-prompt.md",
    }
    for name in ("exp1_bfs_dfs", "exp2_routing", "exp3_knapsack",
                 "exp4_event_queue", "exp5_sorting", "exp5_topk"):
        required.add(f"results/raw/{name}_raw.csv")
    for chapter in range(1, 11):
        # Chapter filenames include their descriptive titles.
        required.add(f"report/{chapter:02d}_*.md")
    with ZipFile(archive) as package:
        infos = [info for info in package.infolist() if not info.is_dir()]
        names = {info.filename for info in infos}
        if len(names) != len(infos):
            errors.append("Duplicate archive member names")
        for name in sorted(names):
            parts = PurePosixPath(name).parts
            if "\\" in name or name.startswith("/") or ".." in parts or ":" in name:
                errors.append(f"Unsafe archive path: {name}")
            if any(part in (".git", ".vs", ".vscode", "__pycache__") for part in parts):
                errors.append(f"Unwanted cache/repository member: {name}")
        bad = package.testzip()
        if bad:
            errors.append(f"CRC failure: {bad}")
        for name in sorted(required):
            if "*" in name:
                matches = [member for member in names if PurePosixPath(member).match(name)]
                if len(matches) != 1:
                    errors.append(f"Expected exactly one chapter: {name}, found {len(matches)}")
            elif name not in names:
                errors.append(f"Missing required member: {name}")
        tracks = [name for name in names if name.startswith("data/") and name.endswith(".trk")]
        if len(tracks) < 3:
            errors.append("Fewer than three packaged circuits")
        for name in sorted(names):
            if name.startswith("data/"):
                companion = "bin/" + name
                if companion not in names:
                    errors.append(f"Missing runtime data: {companion}")
                elif package.read(name) != package.read(companion):
                    errors.append(f"Runtime data differs: {companion}")
        for name in sorted(names):
            if not name.endswith(".md"):
                continue
            text = package.read(name).decode("utf-8-sig")
            for link in re.findall(r"!?\[[^\]]*\]\(([^)]+)\)", text):
                target = link.strip().strip("<>")
                url = urlsplit(target)
                if url.scheme or url.netloc or not url.path:
                    continue
                resolved = posixpath.normpath(posixpath.join(posixpath.dirname(name), unquote(url.path)))
                if resolved not in names and not any(member.startswith(resolved.rstrip("/") + "/") for member in names):
                    errors.append(f"Broken document link: {name} -> {target}")
        if source_root:
            # Require reviewed files, including newly added files absent from an
            # old ZIP. Never accept a CRC-only match as current-source evidence.
            roots = ("src", "cli", "tests", "bench", "tools", "data", "results", "report", "docs", "third_party")
            files = [source_root / name for name in ("CMakeLists.txt", "README.md")]
            for root in roots:
                files.extend(path for path in (source_root / root).rglob("*") if path.is_file())
            for path in files:
                if any(part in ("__pycache__", ".vs", ".vscode") for part in path.parts):
                    continue
                name = path.relative_to(source_root).as_posix()
                if name not in names:
                    errors.append(f"Reviewed file absent: {name}")
                elif hashlib.sha256(package.read(name)).digest() != hashlib.sha256(path.read_bytes()).digest():
                    errors.append(f"Reviewed bytes differ: {name}")
            executable = source_root / "build/Release/smartrace.exe"
            if not executable.is_file():
                errors.append("Cannot compare GUI binary: build/Release/smartrace.exe is absent")
            elif "bin/smartrace.exe" in names and package.read("bin/smartrace.exe") != executable.read_bytes():
                errors.append("Packaged GUI binary differs from reviewed Release build")
        print(f"Archive: {archive}; {len(names)} files; CRC {'FAILED' if bad else 'OK'}")
    for error in errors:
        print("FAIL:", error)
    print(f"Submission contents: {len(errors)} failure(s)")
    return 1 if errors else 0


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    parser.add_argument("--source-root", type=Path)
    args = parser.parse_args()
    raise SystemExit(inspect(args.archive, args.source_root.resolve() if args.source_root else None))
