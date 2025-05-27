#!/usr/bin/env python3

import os
import sys
from pathlib import Path

LICENSE_HEADER = '''\
// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0
'''

SUFFIXES = ('.cpp', '.h', '.hpp', '.cxx', '.cc', '.c', '.inl')

def has_license(lines):
    return any(
        "Licensed under the Apache License" in line
        or "SPDX-License-Identifier" in line
        for line in lines[:10]
    )

def process_file(file_path):
    with open(file_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    if has_license(lines):
        return

    print(f"Adding license to: {file_path}")
    with open(file_path, 'w', encoding='utf-8') as f:
        f.write(LICENSE_HEADER + "\n\n" + ''.join(lines))

def scan_directory(directory):
    for root, _, files in os.walk(directory):
        for file in files:
            if file.endswith(SUFFIXES):
                process_file(os.path.join(root, file))

if __name__ == "__main__":
    base_dir = sys.argv[1] if len(sys.argv) > 1 else "."
    scan_directory(base_dir)
