#!/usr/bin/env python3
"""
Code policy check used by the cpp/CMakeLists.txt build to guard the engine sources.

This script is invoked by the `code_policy_check` custom target configured in
`cpp/CMakeLists.txt` (and thus runs before every library build). It walks the
core `cpp/src` and `cpp/include` trees, ensures each file carries the required
Apache-2 SPDX header, and fails the build if any file references `std::`
directly. The goal is to keep the deterministic engine free of heap-bearing
std:: containers while still running on every local/CI build.
"""

import argparse
import os
import sys

ENGINE_DIRS = ["cpp/src", "cpp/include"]
ALLOWED_LICENSE_LINE = "SPDX-License-Identifier: Apache-2.0"


def has_license_header(path):
    with open(path, "r", encoding="utf-8", errors="ignore") as handle:
        for _ in range(5):
            line = handle.readline()
            if not line:
                break
            if ALLOWED_LICENSE_LINE in line:
                return True
    return False


def main(root):
    failures = []
    for rel_dir in ENGINE_DIRS:
        base_dir = os.path.join(root, rel_dir)
        if not os.path.isdir(base_dir):
            continue
        for dirpath, _, filenames in os.walk(base_dir):
            for filename in filenames:
                if not filename.endswith((".cpp", ".cc", ".c", ".h", ".hpp", ".inl")):
                    continue
                path = os.path.join(dirpath, filename)
                if not has_license_header(path):
                    failures.append(f"Missing Apache header in {path}")
                    continue
                with open(path, "r", encoding="utf-8", errors="ignore") as handle:
                    for line in handle:
                        if "std::" in line:
                            failures.append(
                                f"Forbidden std:: usage in {path} -> {line.strip()}"
                            )
                            break

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
