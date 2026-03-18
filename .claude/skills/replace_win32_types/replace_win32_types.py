#!/usr/bin/env python3
"""Replace Win32 types (DWORD, BOOL, TRUE, FALSE) with standard C++ equivalents.

Usage:
    python replace_win32_types.py              # Apply replacements
    python replace_win32_types.py --dry-run    # Report what would change

Run from the repository root (parent of code/).

Replacements:
    DWORD   -> uint32_t
    BOOL    -> bool      (Win32 BOOL, not C++ bool)
    TRUE    -> true
    FALSE   -> false

Skips:
    - UI files: Tab*.cpp/h, gdioutput*, meos.cpp, Table.*, progress.cpp, printer.cpp
    - Win32 API-heavy files: SportIdent.cpp/h, download.cpp/h, listeditor.cpp
    - Third-party dirs: mysql/, restbed/, png/, libharu/, minizip/
    - Occurrences inside string literals (e.g., SQL strings in MeosSQL.cpp)
    - Occurrences inside // comments
    - Adds #include <cstdint> where DWORD was replaced

IMPORTANT: On MSVC, DWORD (unsigned long) and uint32_t (unsigned int) are different
types. Variables passed to Win32 APIs via LPDWORD or to gdioutput::getData(DWORD&)
must remain DWORD. After running this script, manually verify that domain files
calling Win32 APIs (GetComputerName, etc.) still use DWORD for those specific variables.
"""

import os
import re
import sys

CODE_DIR = 'code'

EXCLUDE_PATTERNS = [
    re.compile(r'^Tab[A-Z]\w*\.(cpp|h)$'),
    re.compile(r'^gdioutput\.(cpp|h)$'),
    re.compile(r'^gdistructures\.h$'),
    re.compile(r'^gdiimpl\.h$'),
    re.compile(r'^meos\.cpp$'),
    re.compile(r'^Table\.(cpp|h)$'),
    re.compile(r'^progress\.cpp$'),
    re.compile(r'^printer\.cpp$'),
    # Non-domain files that heavily use Win32 APIs (WriteFile, ReadFile,
    # InternetReadFile, HttpQueryInfo, GetAdaptersAddresses, WaitCommEvent, etc.)
    # DWORD and uint32_t are different types on MSVC (unsigned long vs unsigned int),
    # so Win32 functions expecting LPDWORD will reject uint32_t*.
    re.compile(r'^SportIdent\.(cpp|h)$'),
    re.compile(r'^download\.(cpp|h)$'),
    re.compile(r'^listeditor\.cpp$'),
]
EXCLUDE_DIRS = {'mysql', 'restbed', 'png', 'libharu', 'minizip'}


def is_excluded(filename, rel_dir):
    parts = rel_dir.replace('\\', '/').split('/')
    if any(d in EXCLUDE_DIRS for d in parts if d != '.'):
        return True
    return any(p.match(filename) for p in EXCLUDE_PATTERNS)


def replace_token_outside_strings(line, old_token, new_token):
    """Replace a word token only outside string literals and comments."""
    result = []
    i = 0
    in_string = False
    string_char = None
    old_len = len(old_token)

    while i < len(line):
        if not in_string:
            # Check for // comment — rest of line is untouched
            if line[i] == '/' and i + 1 < len(line) and line[i + 1] == '/':
                result.append(line[i:])
                break

            # Check for start of string literal
            if line[i] == '"':
                in_string = True
                string_char = '"'
                result.append(line[i])
                i += 1
                continue
            if line[i] == "'":
                in_string = True
                string_char = "'"
                result.append(line[i])
                i += 1
                continue

            # Check for token match with word boundaries
            if line[i:i + old_len] == old_token:
                before_ok = (i == 0 or not (line[i - 1].isalnum() or line[i - 1] == '_'))
                after_pos = i + old_len
                after_ok = (after_pos >= len(line) or
                            not (line[after_pos].isalnum() or line[after_pos] == '_'))
                if before_ok and after_ok:
                    result.append(new_token)
                    i += old_len
                    continue

            result.append(line[i])
            i += 1
        else:
            # Inside string literal
            result.append(line[i])
            if line[i] == '\\' and i + 1 < len(line):
                i += 1
                result.append(line[i])
                i += 1
                continue
            if line[i] == string_char:
                in_string = False
            i += 1

    return ''.join(result)


def process_file(filepath, dry_run=False):
    """Process a single file. Returns list of replacement descriptions."""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            lines = f.readlines()
    except UnicodeDecodeError:
        with open(filepath, 'r', encoding='latin-1') as f:
            lines = f.readlines()

    replacements_made = []
    new_lines = []
    had_dword = False
    has_cstdint = False

    for line_num, line in enumerate(lines, 1):
        if '#include <cstdint>' in line:
            has_cstdint = True

        original = line

        # Apply replacements outside strings/comments
        for old, new in [('DWORD', 'uint32_t'), ('BOOL', 'bool'),
                         ('TRUE', 'true'), ('FALSE', 'false')]:
            line = replace_token_outside_strings(line, old, new)

        if line != original:
            rel = os.path.relpath(filepath, CODE_DIR)
            # Summarize what changed
            changes = []
            if 'DWORD' in original and 'uint32_t' in line:
                changes.append('DWORD->uint32_t')
                had_dword = True
            if replace_token_outside_strings(original, 'BOOL', '') != original and 'bool' in line:
                changes.append('BOOL->bool')
            if 'TRUE' in original and 'true' in line and original != line:
                changes.append('TRUE->true')
            if 'FALSE' in original and 'false' in line and original != line:
                changes.append('FALSE->false')
            if changes:
                replacements_made.append(
                    f"  {rel}:{line_num}: {', '.join(changes)}"
                )

        new_lines.append(line)

    # Add #include <cstdint> if DWORD was replaced and not already present
    if had_dword and not has_cstdint:
        new_lines = add_include(new_lines, '<cstdint>')

    if replacements_made and not dry_run:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.writelines(new_lines)

    return replacements_made


def add_include(lines, header):
    """Insert #include after the last existing #include."""
    last_include_idx = -1
    for i, line in enumerate(lines):
        if line.strip().startswith('#include'):
            last_include_idx = i

    if last_include_idx == -1:
        last_include_idx = 0

    lines.insert(last_include_idx + 1, f'#include {header}\n')
    return lines


def main():
    dry_run = '--dry-run' in sys.argv

    if not os.path.isdir(CODE_DIR):
        print(f"Error: '{CODE_DIR}' directory not found. "
              "Run from the repository root.")
        sys.exit(1)

    all_replacements = []
    files_modified = 0

    for root, dirs, files in os.walk(CODE_DIR):
        dirs[:] = [d for d in dirs if d not in EXCLUDE_DIRS]
        rel_dir = os.path.relpath(root, CODE_DIR)

        for f in sorted(files):
            if not f.endswith(('.cpp', '.h', '.hpp')):
                continue
            if is_excluded(f, rel_dir):
                continue

            filepath = os.path.join(root, f)
            replacements = process_file(filepath, dry_run=dry_run)
            if replacements:
                all_replacements.extend(replacements)
                files_modified += 1

    if all_replacements:
        mode = "Would replace" if dry_run else "Replaced"
        print(f"{mode} in {files_modified} file(s), "
              f"{len(all_replacements)} replacement(s):")
        for r in all_replacements:
            print(r)
        if dry_run:
            sys.exit(1)
    else:
        print("No Win32 types found in domain files.")


if __name__ == '__main__':
    main()
