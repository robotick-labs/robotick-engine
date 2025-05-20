#!/usr/bin/env python3

import os
import sys
from pathlib import Path

LICENSE_HEADER = '''\
// Copyright 2025 Robotick Labs CIC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
'''

SUFFIXES = ('.cpp', '.h', '.hpp', '.cxx', '.cc', '.c', '.inl')

def has_license(lines):
    return any("Licensed under the Apache License" in line for line in lines[:10])

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
