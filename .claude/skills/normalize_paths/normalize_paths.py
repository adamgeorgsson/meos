#!/usr/bin/env python3
"""Normalize backslash path separators to forward slashes in domain code.

Usage:
    python normalize_paths.py              # Apply safe replacements
    python normalize_paths.py --dry-run    # Report what would change
    python normalize_paths.py --inventory  # Just list all backslash occurrences

Run from the repository root (parent of code/).

Automated replacements:
    Hardcoded backslash in path literals:
      L"\\\\subdir\\\\"        -> L"/subdir/"
      L"..\\\\Lists\\\\"      -> L"../Lists/"
      + L"\\\\"               -> + L"/"
      += L"\\\\"              -> += L"/"

Skips:
    - Escape sequences: \\n, \\t, \\r, \\0, \\", \\\\, \\x, \\u
    - SQL strings and query patterns
    - MeosSQL.cpp entirely (heavy SQL escaping)
    - regex/pattern strings
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
    re.compile(r'^MeosSQL\.(cpp|h)$'),  # Heavy SQL escaping
]
EXCLUDE_DIRS = {'mysql', 'restbed', 'png', 'libharu', 'minizip'}


def is_excluded(filename, rel_dir):
    parts = rel_dir.replace('\\', '/').split('/')
    if any(d in EXCLUDE_DIRS for d in parts if d != '.'):
        return True
    return any(p.match(filename) for p in EXCLUDE_PATTERNS)


def find_backslash_in_strings(line, line_num, rel_path):
    """Find backslash occurrences in string literals and classify them."""
    entries = []
    stripped = line.strip()
    if stripped.startswith('//'):
        return entries

    # Find all string literals in the line
    i = 0
    while i < len(line):
        # Find start of string literal
        if line[i] == '"' or (line[i] == 'L' and i + 1 < len(line) and line[i + 1] == '"'):
            is_wide = line[i] == 'L'
            start = i
            i += 2 if is_wide else 1  # skip L" or "

            # Collect string content
            string_content = []
            while i < len(line) and line[i] != '"':
                if line[i] == '\\' and i + 1 < len(line):
                    string_content.append(line[i:i + 2])
                    i += 2
                else:
                    string_content.append(line[i])
                    i += 1

            if i < len(line):
                i += 1  # skip closing "

            # Check if this string contains backslashes that look like path separators
            content = ''.join(string_content)
            if '\\\\' in content:
                # \\\\  in source = \\ in the string = actual backslash
                # This is a path separator candidate
                # Skip known non-path patterns
                is_path = True
                if any(sql in line.lower() for sql in ['select ', 'insert ', 'update ', 'create ', 'drop ', 'alter ']):
                    is_path = False
                if '\\n' in content and '\\\\' not in content:
                    is_path = False  # Likely just escape sequences

                if is_path:
                    entries.append({
                        'file': rel_path,
                        'line': line_num,
                        'content': stripped[:120],
                        'string': line[start:i],
                        'is_path': True,
                    })
        else:
            i += 1

    return entries


def replace_path_backslashes_in_string(s):
    """Replace backslashes with forward slashes in a path string literal.

    In C source code, '\\\\' (two chars: backslash backslash) is an escape
    sequence that produces a single literal backslash in the string. This
    function replaces such escaped backslashes with '/' ONLY when they
    appear to be path separators.

    NOT replaced (kept as-is):
    - '\\\\' followed by n, t, r, 0, a, b, f, v — these produce text like
      '\\n' in the actual string, used as line-break markers in localizer
    - '\\\\' followed by '/' — regex escape for forward slash
    - '\\\\' followed by regex metacharacters
    """
    is_wide = s.startswith('L')
    prefix = 'L"' if is_wide else '"'
    inner = s[2:-1] if is_wide else s[1:-1]

    # Characters that indicate the \\ is NOT a path separator
    # (the \\ + next char forms a text escape like \n, \t, etc.)
    non_path_followers = set('ntrab0fvx/u\'\"')

    result = []
    i = 0
    changed = False

    while i < len(inner):
        if inner[i] == '\\' and i + 1 < len(inner) and inner[i + 1] == '\\':
            # Source '\\' = literal backslash in the C string
            next_i = i + 2
            if next_i >= len(inner):
                # '\\' at end of string literal — path separator
                result.append('/')
                changed = True
                i = next_i
            elif inner[next_i] in non_path_followers:
                # '\\n', '\\t', '\\/', etc. — NOT a path separator
                result.append(inner[i:i + 2])
                i += 2
            else:
                # '\\' followed by letter/digit/dot — path separator
                result.append('/')
                changed = True
                i = next_i
        else:
            result.append(inner[i])
            i += 1

    if not changed:
        return s

    return f'{prefix}{"".join(result)}"'


def replace_path_strings(line):
    """Replace backslash path separators in string literals on a single line."""
    stripped = line.strip()
    if stripped.startswith('//'):
        return line, None

    # Skip SQL-looking lines
    lower = line.lower()
    if any(sql in lower for sql in ['select ', 'insert ', 'update ', 'create ', 'drop ', 'query']):
        return line, None

    changes = []
    result = []
    i = 0

    while i < len(line):
        # Find string literal
        if line[i] == '"' or (line[i] == 'L' and i + 1 < len(line) and line[i + 1] == '"'):
            is_wide = line[i] == 'L'
            start = i
            i += 2 if is_wide else 1

            while i < len(line) and line[i] != '"':
                if line[i] == '\\' and i + 1 < len(line):
                    i += 2
                else:
                    i += 1
            if i < len(line):
                i += 1  # closing "

            original_string = line[start:i]

            # Only process strings that contain \\ (path backslash)
            if '\\\\' in original_string:
                new_string = replace_path_backslashes_in_string(original_string)
                if new_string != original_string:
                    result.append(new_string)
                    changes.append(f'{original_string} -> {new_string}')
                    continue

            result.append(original_string)
        else:
            result.append(line[i])
            i += 1

    if changes:
        return ''.join(result), f"path strings: {'; '.join(changes[:3])}"
    return line, None


def process_file(filepath, dry_run=False, inventory_only=False):
    """Process a file for path normalization."""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            lines = f.readlines()
    except UnicodeDecodeError:
        with open(filepath, 'r', encoding='latin-1') as f:
            lines = f.readlines()

    rel = os.path.relpath(filepath, CODE_DIR)

    if inventory_only:
        entries = []
        for line_num, line in enumerate(lines, 1):
            entries.extend(find_backslash_in_strings(line, line_num, rel))
        return [], entries

    replacements = []
    new_lines = []
    changed = False

    for line_num, line in enumerate(lines, 1):
        new_line, desc = replace_path_strings(line)
        if desc:
            replacements.append(f"  {rel}:{line_num}: {desc}")
            changed = True
        new_lines.append(new_line)

    if changed and not dry_run:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.writelines(new_lines)

    return replacements, []


def main():
    dry_run = '--dry-run' in sys.argv
    inventory_only = '--inventory' in sys.argv

    if not os.path.isdir(CODE_DIR):
        print(f"Error: '{CODE_DIR}' directory not found.")
        sys.exit(1)

    all_replacements = []
    all_inventory = []

    for root, dirs, files in os.walk(CODE_DIR):
        dirs[:] = [d for d in dirs if d not in EXCLUDE_DIRS]
        rel_dir = os.path.relpath(root, CODE_DIR)

        for f in sorted(files):
            if not f.endswith(('.cpp', '.h', '.hpp')):
                continue
            if is_excluded(f, rel_dir):
                continue

            filepath = os.path.join(root, f)
            replacements, inventory = process_file(
                filepath, dry_run=dry_run, inventory_only=inventory_only
            )
            all_replacements.extend(replacements)
            all_inventory.extend(inventory)

    if not inventory_only and all_replacements:
        mode = "Would replace" if dry_run else "Replaced"
        print(f"{mode} backslash path separators in {len(all_replacements)} location(s):")
        for r in all_replacements:
            print(r)

    if inventory_only and all_inventory:
        print(f"Found {len(all_inventory)} string literal(s) with backslashes:")
        for entry in all_inventory:
            print(f"  {entry['file']}:{entry['line']}: {entry['string'][:80]}")

    if not all_replacements and not all_inventory:
        print("No backslash path separators found in domain files.")

    if dry_run and all_replacements:
        sys.exit(1)


if __name__ == '__main__':
    main()
