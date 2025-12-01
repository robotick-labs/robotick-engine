# Copyright Robotick Labs
# SPDX-License-Identifier: Apache-2.0

"""
Code policy check used by the cpp/CMakeLists.txt build to guard the engine sources.

This script is invoked by the `code_policy_check` custom target configured in
`cpp/CMakeLists.txt` (and thus runs before every library build). It walks the
core `cpp/src` and `cpp/include` trees, ensures the Robotick header occupies the
first two lines of every file, and fails the build if any file references
`std::` directly. The goal is to keep the deterministic engine free of
heap-bearing std:: containers while still running on every local/CI build.
"""

import argparse
import os
import sys

DEFAULT_SOURCE_DIRS = ["cpp/src", "cpp/include", "cpp/tests"]
DEFAULT_SUFFIXES = (".cpp", ".cc", ".c", ".h", ".hpp", ".inl")
DEFAULT_TOOL_FILES = ["tools/code_policy_check.py"]

CPP_LICENSE_HEADER = [
    "// Copyright Robotick Labs",
    "// SPDX-License-Identifier: Apache-2.0",
]

PYTHON_LICENSE_HEADER = [
    "# Copyright Robotick Labs",
    "# SPDX-License-Identifier: Apache-2.0",
]

LICENSE_HEADERS = {
    ".py": PYTHON_LICENSE_HEADER,
}
SPDX_IDENTIFIER = "SPDX-License-Identifier: Apache-2.0"


def _expected_header(path):
    _, ext = os.path.splitext(path)
    return LICENSE_HEADERS.get(ext.lower(), CPP_LICENSE_HEADER)


def _normalize_header_line(line, line_index):
    normalized = line.rstrip("\r\n")
    if line_index == 1 and normalized.startswith("\ufeff"):
        normalized = normalized.lstrip("\ufeff")
    return normalized


def _has_spdx_header(path):
    with open(path, "r", encoding="utf-8", errors="ignore") as handle:
        for _ in range(5):
            line = handle.readline()
            if not line:
                break
            if SPDX_IDENTIFIER in line:
                return True
    return False


def check_file(path, check_std_usage, header_mode):
    if header_mode == "spdx":
        if not _has_spdx_header(path):
            return f"Missing Apache header in {path}"
    else:
        expected_header = _expected_header(path)
        with open(path, "r", encoding="utf-8", errors="ignore") as handle:
            for index, expected_line in enumerate(expected_header, start=1):
                line = handle.readline()
                if not line:
                    return (
                        f"Missing header line {index} in {path}. "
                        f"Expected '{expected_line}'."
                    )
                normalized = _normalize_header_line(line, index)
                if normalized != expected_line:
                    return (
                        f"Incorrect header line {index} in {path}. "
                        f"Expected '{expected_line}', found '{normalized}'."
                    )
            if not check_std_usage:
                return None
            for line in handle:
                if "std::" in line:
                    return f"Forbidden std:: usage in {path} -> {line.strip()}"
        return None

    if not check_std_usage:
        return None

    with open(path, "r", encoding="utf-8", errors="ignore") as handle:
        # Skip up to the header lines already checked.
        for _ in range(5):
            if not handle.readline():
                break
        for line in handle:
            if "std::" in line:
                return f"Forbidden std:: usage in {path} -> {line.strip()}"
    return None


def _normalize_excludes(root, exclude_dirs):
    if not exclude_dirs:
        return []
    normalized = []
    for rel in exclude_dirs:
        path = os.path.normpath(os.path.join(root, rel))
        normalized.append(path)
    return normalized


def _is_excluded(path, excludes):
    if not excludes:
        return False
    norm = os.path.normpath(path)
    for ex in excludes:
        if norm == ex or norm.startswith(ex + os.sep):
            return True
    return False


def run_policy_check(
    root,
    source_dirs=None,
    file_suffixes=None,
    tool_files=None,
    check_std_usage=True,
    header_mode="exact",
    exclude_dirs=None,
):
    failures = []
    dirs_to_scan = source_dirs if source_dirs is not None else DEFAULT_SOURCE_DIRS
    suffixes = file_suffixes if file_suffixes is not None else DEFAULT_SUFFIXES
    tools = tool_files if tool_files is not None else DEFAULT_TOOL_FILES
    excludes = _normalize_excludes(root, exclude_dirs)

    for rel_dir in dirs_to_scan:
        base_dir = os.path.join(root, rel_dir)
        if not os.path.isdir(base_dir):
            continue
        for dirpath, dirnames, filenames in os.walk(base_dir):
            if _is_excluded(dirpath, excludes):
                continue
            dirnames[:] = [
                d
                for d in dirnames
                if not _is_excluded(os.path.join(dirpath, d), excludes)
            ]
            for filename in filenames:
                if not filename.endswith(suffixes):
                    continue
                path = os.path.join(dirpath, filename)
                error = check_file(
                    path, check_std_usage=check_std_usage, header_mode=header_mode
                )
                if error:
                    failures.append(error)

    for rel_path in tools:
        path = os.path.join(root, rel_path)
        if not os.path.isfile(path):
            continue
        error = check_file(path, check_std_usage=False, header_mode=header_mode)
        if error:
            failures.append(error)

    if failures:
        print("Code policy violations detected:", file=sys.stderr)
        for failure in failures:
            print(f"  {failure}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Check engine sources for policy compliance"
    )
    parser.add_argument(
        "source_root", help="Root directory containing the sources to check"
    )
    parser.add_argument(
        "--source-dir",
        dest="source_dirs",
        action="append",
        help="Relative directory (from source_root) to scan (defaults to engine dirs). Can be supplied multiple times.",
    )
    parser.add_argument(
        "--tool-file",
        dest="tool_files",
        action="append",
        help="Relative file to check for headers without STL checks (defaults to the policy script itself).",
    )
    parser.add_argument(
        "--suffix",
        dest="suffixes",
        action="append",
        help="File suffix to include while scanning (defaults to engine C/C++ suffixes).",
    )
    parser.add_argument(
        "--allow-std",
        dest="allow_std",
        action="store_true",
        help="If specified, skips the std:: usage check (only verifies headers).",
    )
    parser.add_argument(
        "--header-mode",
        choices=("exact", "spdx"),
        default="exact",
        help="Header validation strictness. Default matches the engine's exact header.",
    )
    parser.add_argument(
        "--exclude-dir",
        dest="exclude_dirs",
        action="append",
        help="Relative directory to skip while scanning (may be supplied multiple times).",
    )
    args = parser.parse_args()
    run_policy_check(
        args.source_root,
        source_dirs=args.source_dirs,
        file_suffixes=tuple(args.suffixes) if args.suffixes else None,
        tool_files=args.tool_files,
        check_std_usage=not args.allow_std,
        header_mode=args.header_mode,
        exclude_dirs=args.exclude_dirs,
    )
