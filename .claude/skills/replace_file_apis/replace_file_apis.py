#!/usr/bin/env python3
"""Replace Win32 file APIs with std::filesystem equivalents in domain code.

Usage:
    python replace_file_apis.py              # Apply safe replacements
    python replace_file_apis.py --dry-run    # Report what would change
    python replace_file_apis.py --inventory  # Just list all occurrences

Run from the repository root (parent of code/).

Automated replacements:
    _waccess(path, 0) == 0          -> std::filesystem::exists(path)
    _waccess(path, 0) == -1         -> !std::filesystem::exists(path)
    _waccess(path, 4) == 0          -> std::filesystem::exists(path)
    DeleteFile(path)                -> std::filesystem::remove(path, ec)
    GetFileAttributes(f) != INVALID -> std::filesystem::exists(f)
    MAX_PATH                        -> 260  (or std::filesystem::path)

Complex cases (inventoried, not automated):
    FindFirstFile/FindNextFile/FindClose loops -> std::filesystem::directory_iterator
    _wfopen_s -> std::ifstream/std::ofstream
    _wsplitpath_s -> std::filesystem::path methods
    GetTempPath -> std::filesystem::temp_directory_path()
    GetCurrentDirectory -> std::filesystem::current_path()
    CreateDirectory -> std::filesystem::create_directories()
    wchar_t buf[MAX_PATH] -> std::filesystem::path
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
]
EXCLUDE_DIRS = {'mysql', 'restbed', 'png', 'libharu', 'minizip'}


def is_excluded(filename, rel_dir):
    parts = rel_dir.replace('\\', '/').split('/')
    if any(d in EXCLUDE_DIRS for d in parts if d != '.'):
        return True
    return any(p.match(filename) for p in EXCLUDE_PATTERNS)


def replace_waccess(line):
    """Replace _waccess(path, N) == 0/-1 with std::filesystem::exists."""
    stripped = line.lstrip()
    if stripped.startswith('//'):
        return line, None

    # _waccess(path, 0) == 0  ->  std::filesystem::exists(path)
    m = re.search(r'_waccess\(([^,]+),\s*\d+\)\s*==\s*0', line)
    if m:
        path_arg = m.group(1).strip()
        old = m.group(0)
        new = f'std::filesystem::exists({path_arg})'
        return line.replace(old, new), f'_waccess -> exists({path_arg})'

    # _waccess(path, 0) == -1  ->  !std::filesystem::exists(path)
    m = re.search(r'_waccess\(([^,]+),\s*\d+\)\s*==\s*-1', line)
    if m:
        path_arg = m.group(1).strip()
        old = m.group(0)
        new = f'!std::filesystem::exists({path_arg})'
        return line.replace(old, new), f'_waccess == -1 -> !exists({path_arg})'

    # _waccess(path, 0) != 0  ->  !std::filesystem::exists(path)
    m = re.search(r'_waccess\(([^,]+),\s*\d+\)\s*!=\s*0', line)
    if m:
        path_arg = m.group(1).strip()
        old = m.group(0)
        new = f'!std::filesystem::exists({path_arg})'
        return line.replace(old, new), f'_waccess != 0 -> !exists({path_arg})'

    return line, None


def replace_getfileattributes(line):
    """Replace GetFileAttributes(f) != INVALID_FILE_ATTRIBUTES."""
    stripped = line.lstrip()
    if stripped.startswith('//'):
        return line, None

    m = re.search(r'GetFileAttributes\(([^)]+)\)\s*!=\s*INVALID_FILE_ATTRIBUTES', line)
    if m:
        path_arg = m.group(1).strip()
        old = m.group(0)
        new = f'std::filesystem::exists({path_arg})'
        return line.replace(old, new), f'GetFileAttributes -> exists({path_arg})'

    return line, None


def replace_deletefile(line):
    """Replace DeleteFile(path) with std::filesystem::remove(path, ec)."""
    stripped = line.lstrip()
    if stripped.startswith('//'):
        return line, None

    m = re.search(r'DeleteFile\(([^)]+)\)', line)
    if m:
        path_arg = m.group(1).strip()
        indent = line[:len(line) - len(line.lstrip())]
        # Use error_code overload for safe removal (matches PRD recommendation)
        old = m.group(0)
        new = f'{{ std::error_code ec; std::filesystem::remove({path_arg}, ec); }}'
        # If the DeleteFile is the entire statement
        if stripped.startswith('DeleteFile(') and stripped.endswith(');'):
            return f'{indent}{{ std::error_code ec; std::filesystem::remove({path_arg}, ec); }}\n', \
                   f'DeleteFile({path_arg}) -> filesystem::remove'
        else:
            # Inline in expression — just replace function call
            return line.replace(old, f'std::filesystem::remove({path_arg}, ec)'), \
                   f'DeleteFile -> filesystem::remove (check ec scope)'

    return line, None


def find_complex_patterns(filepath, lines):
    """Find complex patterns needing manual replacement."""
    entries = []
    rel = os.path.relpath(filepath, CODE_DIR)

    patterns = [
        (r'FindFirstFile\b', 'Replace loop with std::filesystem::directory_iterator'),
        (r'FindNextFile\b', '(part of FindFirstFile loop)'),
        (r'FindClose\b', '(part of FindFirstFile loop)'),
        (r'WIN32_FIND_DATA', 'Replace with directory_iterator entry'),
        (r'_wfopen_s\b', 'Replace with std::ifstream/std::ofstream'),
        (r'_wsplitpath_s\b', 'Use std::filesystem::path: .stem(), .extension(), .parent_path()'),
        (r'_splitpath_s\b', 'Use std::filesystem::path: .stem(), .extension(), .parent_path()'),
        (r'GetTempPath\b', 'Replace with std::filesystem::temp_directory_path()'),
        (r'GetCurrentDirectory\b', 'Replace with std::filesystem::current_path()'),
        (r'CreateDirectory\b', 'Replace with std::filesystem::create_directories(path, ec)'),
        (r'wchar_t\s+\w+\s*\[\s*MAX_PATH\s*\]', 'Replace with std::filesystem::path'),
        (r'wchar_t\s+\w+\s*\[\s*_MAX_PATH\s*\]', 'Replace with std::filesystem::path'),
        (r'wcscpy_s\(.*MAX_PATH', 'Replace with std::filesystem::path operations'),
        (r'wcscat_s\(.*MAX_PATH', 'Replace with path /= or path::append'),
        (r'GetModuleFileName\b', 'Platform-specific — use #ifdef or argv[0]'),
    ]

    for line_num, line in enumerate(lines, 1):
        stripped = line.strip()
        if stripped.startswith('//'):
            continue
        for pattern, suggestion in patterns:
            if re.search(pattern, line):
                entries.append(f"  {rel}:{line_num}: {stripped[:120]}")
                entries.append(f"    -> {suggestion}")
                break

    return entries


def process_file(filepath, dry_run=False, inventory_only=False):
    """Process a file for file API replacements."""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            lines = f.readlines()
    except UnicodeDecodeError:
        with open(filepath, 'r', encoding='latin-1') as f:
            lines = f.readlines()

    if inventory_only:
        return [], find_complex_patterns(filepath, lines)

    replacements = []
    new_lines = []
    changed = False
    has_filesystem = any('#include <filesystem>' in l for l in lines)
    needs_filesystem = False
    rel = os.path.relpath(filepath, CODE_DIR)

    for line_num, line in enumerate(lines, 1):
        original = line

        for replacer in [replace_waccess, replace_getfileattributes, replace_deletefile]:
            new_line, desc = replacer(line)
            if desc:
                line = new_line
                replacements.append(f"  {rel}:{line_num}: {desc}")
                needs_filesystem = True

        if line != original:
            changed = True
        new_lines.append(line)

    if changed:
        if needs_filesystem and not has_filesystem:
            new_lines = add_include(new_lines, '<filesystem>')

        if not dry_run:
            with open(filepath, 'w', encoding='utf-8') as f:
                f.writelines(new_lines)

    complex_entries = find_complex_patterns(filepath, lines)
    return replacements, complex_entries


def add_include(lines, header):
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
    inventory_only = '--inventory' in sys.argv

    if not os.path.isdir(CODE_DIR):
        print(f"Error: '{CODE_DIR}' directory not found.")
        sys.exit(1)

    all_replacements = []
    all_complex = []

    for root, dirs, files in os.walk(CODE_DIR):
        dirs[:] = [d for d in dirs if d not in EXCLUDE_DIRS]
        rel_dir = os.path.relpath(root, CODE_DIR)

        for f in sorted(files):
            if not f.endswith(('.cpp', '.h', '.hpp')):
                continue
            if is_excluded(f, rel_dir):
                continue

            filepath = os.path.join(root, f)
            replacements, complex_entries = process_file(
                filepath, dry_run=dry_run, inventory_only=inventory_only
            )
            all_replacements.extend(replacements)
            all_complex.extend(complex_entries)

    if not inventory_only and all_replacements:
        mode = "Would replace" if dry_run else "Replaced"
        print(f"{mode} {len(all_replacements)} file API call(s):")
        for r in all_replacements:
            print(r)
        print()

    if all_complex:
        print(f"Complex patterns requiring manual replacement "
              f"({len(all_complex) // 2} occurrence(s)):")
        for entry in all_complex:
            print(entry)
    elif not all_replacements:
        print("No Win32 file APIs found in domain files.")

    if dry_run and all_replacements:
        sys.exit(1)


if __name__ == '__main__':
    main()
