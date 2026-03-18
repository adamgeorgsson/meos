#!/usr/bin/env python3
"""Replace Win32 time APIs with std::chrono equivalents in domain code.

Usage:
    python replace_time_apis.py                    # Apply all safe replacements
    python replace_time_apis.py --dry-run          # Report what would change
    python replace_time_apis.py --inventory        # Just list all occurrences

Run from the repository root (parent of code/).

Automated replacements:
    GetTickCount64()                    -> meos_steady_clock_ms()
    SYSTEMTIME st; GetLocalTime(&st);   -> std::tm st; meos_localtime_now(&st);
    st.wYear                            -> (st.tm_year + 1900)
    st.wMonth                           -> (st.tm_mon + 1)
    st.wDay                             -> st.tm_mday
    st.wHour                            -> st.tm_hour
    st.wMinute                          -> st.tm_min
    st.wSecond                          -> st.tm_sec
    st.wDayOfWeek                       -> st.tm_wday
    SYSTEMTIME <var>;                   -> std::tm <var> = {};

Complex cases (inventoried, not automated):
    SystemTimeToFileTime / FileTimeToLocalFileTime / FILETIME
    SystemTimeToInt64TenthSecond / Int64TenthSecondToSystemTime
    convertDateYMD with SYSTEMTIME param
    convertSystem* functions

These require structural changes — the script inventories them for manual work.
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


# SYSTEMTIME field -> std::tm field mapping
SYSTEMTIME_FIELDS = {
    'wYear': '(%%VAR%%.tm_year + 1900)',
    'wMonth': '(%%VAR%%.tm_mon + 1)',
    'wDay': '%%VAR%%.tm_mday',
    'wHour': '%%VAR%%.tm_hour',
    'wMinute': '%%VAR%%.tm_min',
    'wSecond': '%%VAR%%.tm_sec',
    'wDayOfWeek': '%%VAR%%.tm_wday',
    'wMilliseconds': '0 /* TODO: std::tm has no milliseconds */',
}


def replace_gettickcount64(line):
    """Replace GetTickCount64() with meos_steady_clock_ms()."""
    if 'GetTickCount64()' not in line:
        return line, None
    stripped = line.lstrip()
    if stripped.startswith('//'):
        return line, None
    new_line = line.replace('GetTickCount64()', 'meos_steady_clock_ms()')
    if new_line != line:
        return new_line, 'GetTickCount64() -> meos_steady_clock_ms()'
    return line, None


def replace_getlocaltime(line):
    """Replace GetLocalTime(&var) with meos_localtime_now(&var)."""
    if 'GetLocalTime(' not in line:
        return line, None
    stripped = line.lstrip()
    if stripped.startswith('//'):
        return line, None
    m = re.search(r'GetLocalTime\(&(\w+)\)', line)
    if m:
        var = m.group(1)
        new_line = line.replace(f'GetLocalTime(&{var})', f'meos_localtime_now(&{var})')
        return new_line, f'GetLocalTime(&{var}) -> meos_localtime_now(&{var})'
    return line, None


def replace_systemtime_decl(line):
    """Replace 'SYSTEMTIME var;' with 'std::tm var = {};'."""
    stripped = line.lstrip()
    if stripped.startswith('//'):
        return line, None
    # Match standalone SYSTEMTIME declaration
    m = re.match(r'^(\s*)SYSTEMTIME\s+(\w+)\s*;\s*$', line)
    if m:
        indent = m.group(1)
        var = m.group(2)
        return f'{indent}std::tm {var} = {{}};\n', f'SYSTEMTIME {var} -> std::tm {var}'

    # Match SYSTEMTIME in memset pattern (often paired with memset)
    # Just replace the type declaration, memset will need manual update
    m = re.match(r'^(\s*)SYSTEMTIME\s+(\w+)\s*=\s*\{', line)
    if m:
        indent = m.group(1)
        var = m.group(2)
        # Preserve the rest of the initializer
        rest = line[m.end() - 1:]  # from { onwards
        return f'{indent}std::tm {var} = {{}};\n', f'SYSTEMTIME {var} init -> std::tm {var}'

    return line, None


def replace_systemtime_fields(line, known_vars):
    """Replace SYSTEMTIME field accesses like st.wYear -> (st.tm_year + 1900)."""
    if not known_vars:
        return line, None
    stripped = line.lstrip()
    if stripped.startswith('//'):
        return line, None

    changes = []
    for var in known_vars:
        for old_field, new_expr_template in SYSTEMTIME_FIELDS.items():
            pattern = f'{var}.{old_field}'
            if pattern in line:
                new_expr = new_expr_template.replace('%%VAR%%', var)
                line = line.replace(pattern, new_expr)
                changes.append(f'{var}.{old_field}')

    if changes:
        return line, f"fields: {', '.join(changes)}"
    return line, None


def replace_memset_systemtime(line, known_vars):
    """Replace memset(&st, 0, sizeof(SYSTEMTIME)) with st = {}."""
    if 'memset' not in line or 'SYSTEMTIME' not in line:
        return line, None
    for var in known_vars:
        pattern = rf'memset\(&{var},\s*0,\s*sizeof\(SYSTEMTIME\)\)\s*;'
        m = re.search(pattern, line)
        if m:
            indent = line[:len(line) - len(line.lstrip())]
            return f'{indent}{var} = {{}};\n', f'memset(&{var},...SYSTEMTIME) -> {var} = {{}}'
    return line, None


def find_complex_patterns(filepath, lines):
    """Find complex patterns that need manual replacement. Returns inventory entries."""
    entries = []
    rel = os.path.relpath(filepath, CODE_DIR)

    complex_patterns = [
        (r'SystemTimeToFileTime', 'Replace with chrono time_point -> time_t conversion'),
        (r'FileTimeToLocalFileTime', 'Replace with chrono + localtime'),
        (r'FileTimeToSystemTime', 'Replace with chrono conversion'),
        (r'SystemTimeToInt64TenthSecond', 'Rewrite using std::tm + mktime'),
        (r'Int64TenthSecondToSystemTime', 'Rewrite using localtime'),
        (r'TzSpecificLocalTimeToSystemTime', 'Replace with mktime + gmtime for local-to-UTC conversion'),
        (r'\bFILETIME\b', 'Replace with std::chrono::system_clock::time_point or time_t'),
        (r'convertSystemTime[^N]', 'Update signature from SYSTEMTIME to std::tm'),
        (r'convertSystemTimeN', 'Update signature from SYSTEMTIME to std::tm'),
        (r'convertSystemDate', 'Update signature from SYSTEMTIME to std::tm'),
        (r'convertSystemTimeOnly', 'Update signature from SYSTEMTIME to std::tm'),
        (r'convertDateYMD.*SYSTEMTIME', 'Update signature from SYSTEMTIME to std::tm'),
    ]

    for line_num, line in enumerate(lines, 1):
        stripped = line.strip()
        if stripped.startswith('//'):
            continue
        for pattern, suggestion in complex_patterns:
            if re.search(pattern, line):
                entries.append(f"  {rel}:{line_num}: {stripped[:100]}")
                entries.append(f"    -> {suggestion}")
                break  # One entry per line

    return entries


def process_file(filepath, dry_run=False, inventory_only=False):
    """Process a file for time API replacements."""
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
    known_tm_vars = set()
    changed = False
    has_chrono = any('#include <chrono>' in l for l in lines)
    has_ctime = any('#include <ctime>' in l for l in lines)
    needs_chrono = False
    needs_ctime = False
    rel = os.path.relpath(filepath, CODE_DIR)

    for line_num, line in enumerate(lines, 1):
        original = line

        # Replace SYSTEMTIME declarations (track variable names)
        new_line, desc = replace_systemtime_decl(line)
        if desc:
            # Extract variable name for field tracking
            m = re.search(r'std::tm\s+(\w+)', new_line)
            if m:
                known_tm_vars.add(m.group(1))
            line = new_line
            replacements.append(f"  {rel}:{line_num}: {desc}")
            needs_ctime = True

        # Also track SYSTEMTIME vars we see in function signatures (param types)
        for m in re.finditer(r'SYSTEMTIME\s*&?\s*(\w+)', original):
            known_tm_vars.add(m.group(1))

        # Replace GetLocalTime
        new_line, desc = replace_getlocaltime(line)
        if desc:
            line = new_line
            replacements.append(f"  {rel}:{line_num}: {desc}")
            needs_chrono = True

        # Replace GetTickCount64
        new_line, desc = replace_gettickcount64(line)
        if desc:
            line = new_line
            replacements.append(f"  {rel}:{line_num}: {desc}")
            needs_chrono = True

        # Replace memset(&st, 0, sizeof(SYSTEMTIME))
        new_line, desc = replace_memset_systemtime(line, known_tm_vars)
        if desc:
            line = new_line
            replacements.append(f"  {rel}:{line_num}: {desc}")

        # Replace SYSTEMTIME field accesses
        new_line, desc = replace_systemtime_fields(line, known_tm_vars)
        if desc:
            line = new_line
            replacements.append(f"  {rel}:{line_num}: {desc}")

        if line != original:
            changed = True
        new_lines.append(line)

    # Add includes
    if changed:
        if needs_chrono and not has_chrono:
            new_lines = add_include(new_lines, '<chrono>')
        if needs_ctime and not has_ctime:
            new_lines = add_include(new_lines, '<ctime>')

    if changed and not dry_run:
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
        print(f"{mode} {len(all_replacements)} time API call(s):")
        for r in all_replacements:
            print(r)
        print()

    if all_complex:
        print(f"Complex patterns requiring manual replacement "
              f"({len(all_complex) // 2} occurrence(s)):")
        for entry in all_complex:
            print(entry)
    elif not all_replacements:
        print("No Win32 time APIs found in domain files.")

    if dry_run and all_replacements:
        sys.exit(1)


if __name__ == '__main__':
    main()
