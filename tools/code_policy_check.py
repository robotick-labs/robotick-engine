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

ENGINE_DIRS = ["cpp/src", "cpp/include"]
ENGINE_SUFFIXES = (".cpp", ".cc", ".c", ".h", ".hpp", ".inl")
TOOL_FILES = ["tools/code_policy_check.py"]

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


def _expected_header(path):
    _, ext = os.path.splitext(path)
    return LICENSE_HEADERS.get(ext.lower(), CPP_LICENSE_HEADER)


def _normalize_header_line(line, line_index):
    normalized = line.rstrip("\r\n")
    if line_index == 1 and normalized.startswith("\ufeff"):
        normalized = normalized.lstrip("\ufeff")
    return normalized


def check_file(path, check_std_usage):
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


def main(root):
    failures = []
    for rel_dir in ENGINE_DIRS:
        base_dir = os.path.join(root, rel_dir)
        if not os.path.isdir(base_dir):
            continue
        for dirpath, _, filenames in os.walk(base_dir):
            for filename in filenames:
                if not filename.endswith(ENGINE_SUFFIXES):
                    continue
                path = os.path.join(dirpath, filename)
                error = check_file(path, check_std_usage=True)
                if error:
                    failures.append(error)

    for rel_path in TOOL_FILES:
        path = os.path.join(root, rel_path)
        if not os.path.isfile(path):
            continue
        error = check_file(path, check_std_usage=False)
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
        "source_root", help="Root directory containing the engine sources"
    )
    args = parser.parse_args()
    main(args.source_root)
