#!/usr/bin/env python3
"""Replace OutputDebugString() with std::cerr and inventory MessageBox() calls.

Usage:
    python replace_msgbox_debug.py              # Apply OutputDebugString replacements
    python replace_msgbox_debug.py --dry-run    # Report what would change
    python replace_msgbox_debug.py --inventory  # Just list MessageBox calls needing manual work

Run from the repository root (parent of code/).

Automated replacements:
    OutputDebugString("literal\\n")     -> std::cerr << "literal" << '\\n'
    OutputDebugString(L"literal\\n")    -> std::cerr << "literal" << '\\n'
    OutputDebugString(expr.c_str())     -> std::cerr << expr
    OutputDebugString(expr)             -> std::cerr << expr

MessageBox() is NOT automated — it requires context-dependent replacement (throw, callback, or cerr).
Use --inventory to see all MessageBox calls that need manual replacement.
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


def replace_output_debug_string(line):
    """Replace OutputDebugString(...) with std::cerr << ... on a single line.

    Returns (new_line, description) or (line, None) if no replacement.
    """
    stripped = line.lstrip()

    # Skip commented-out lines
    if stripped.startswith('//'):
        return line, None

    if 'OutputDebugString(' not in line:
        return line, None

    indent = line[:len(line) - len(line.lstrip())]

    # Pattern 1: OutputDebugString("literal") or OutputDebugString(L"literal")
    # Extract the argument between the parens
    m = re.match(r'(\s*)OutputDebugString\((.*)\);\s*$', line)
    if not m:
        return line, None

    indent = m.group(1)
    arg = m.group(2).strip()

    # Case: string literal (narrow or wide)
    if arg.startswith('L"') or arg.startswith('"'):
        # Strip L prefix and quotes
        literal = arg
        if literal.startswith('L'):
            literal = literal[1:]
        # Keep the string as narrow
        # Strip trailing \n if present inside the string
        if literal.endswith('"') and literal.startswith('"'):
            inner = literal[1:-1]
            if inner.endswith('\\n'):
                inner = inner[:-2]
                new_line = f'{indent}std::cerr << "{inner}" << \'\\n\';\n'
            else:
                new_line = f'{indent}std::cerr << "{inner}";\n'
            return new_line, f'literal -> cerr'

    # Case: expr.c_str()
    if arg.endswith('.c_str()'):
        expr = arg[:-len('.c_str()')]
        # If it's a wide string expression, need narrow()
        new_line = f'{indent}std::cerr << narrow({expr});\n'
        return new_line, f'{arg} -> cerr << narrow(expr)'

    # Case: (expr).c_str()
    m2 = re.match(r'\((.+)\)\.c_str\(\)', arg)
    if m2:
        expr = m2.group(1)
        new_line = f'{indent}std::cerr << narrow({expr});\n'
        return new_line, f'(expr).c_str() -> cerr << narrow(expr)'

    # Case: plain expression (probably char* or wchar_t*)
    new_line = f'{indent}std::cerr << {arg};\n'
    return new_line, f'{arg} -> cerr'


def process_file_ods(filepath, dry_run=False):
    """Replace OutputDebugString in a file. Returns replacement descriptions."""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            lines = f.readlines()
    except UnicodeDecodeError:
        with open(filepath, 'r', encoding='latin-1') as f:
            lines = f.readlines()

    replacements = []
    new_lines = []
    changed = False
    has_iostream = False

    for line_num, line in enumerate(lines, 1):
        if '#include <iostream>' in line:
            has_iostream = True

        new_line, desc = replace_output_debug_string(line)
        if desc:
            rel = os.path.relpath(filepath, CODE_DIR)
            replacements.append(f"  {rel}:{line_num}: {desc}")
            changed = True
        new_lines.append(new_line)

    if changed:
        if not has_iostream:
            new_lines = add_include(new_lines, '<iostream>')

        if not dry_run:
            with open(filepath, 'w', encoding='utf-8') as f:
                f.writelines(new_lines)

    return replacements


def inventory_messagebox(filepath):
    """List MessageBox() calls in a file for manual replacement."""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            lines = f.readlines()
    except UnicodeDecodeError:
        with open(filepath, 'r', encoding='latin-1') as f:
            lines = f.readlines()

    entries = []
    rel = os.path.relpath(filepath, CODE_DIR)

    for line_num, line in enumerate(lines, 1):
        stripped = line.strip()
        if stripped.startswith('//'):
            continue
        if 'MessageBox(' in line:
            # Determine suggested replacement
            suggestion = "throw meosException(msg)"
            if 'MB_ICONWARNING' in line:
                suggestion = "std::cerr << narrow(msg) << std::endl (warning)"
            elif 'MB_YESNO' in line:
                suggestion = "callback: std::function<bool(const wstring&)>"
            elif 'MB_OKCANCEL' in line:
                suggestion = "callback: std::function<bool(const wstring&)>"
            elif 'MB_YESNOCANCEL' in line:
                suggestion = "callback: std::function<int(const wstring&)>"

            entries.append(f"  {rel}:{line_num}: {stripped[:100]}")
            entries.append(f"    -> Suggested: {suggestion}")

    return entries


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
    inventory_only = '--inventory' in sys.argv

    if not os.path.isdir(CODE_DIR):
        print(f"Error: '{CODE_DIR}' directory not found. "
              "Run from the repository root.")
        sys.exit(1)

    all_replacements = []
    all_messagebox = []

    for root, dirs, files in os.walk(CODE_DIR):
        dirs[:] = [d for d in dirs if d not in EXCLUDE_DIRS]
        rel_dir = os.path.relpath(root, CODE_DIR)

        for f in sorted(files):
            if not f.endswith(('.cpp', '.h', '.hpp')):
                continue
            if is_excluded(f, rel_dir):
                continue

            filepath = os.path.join(root, f)

            if not inventory_only:
                replacements = process_file_ods(filepath, dry_run=dry_run)
                all_replacements.extend(replacements)

            mb_entries = inventory_messagebox(filepath)
            all_messagebox.extend(mb_entries)

    # Report OutputDebugString replacements
    if not inventory_only:
        if all_replacements:
            mode = "Would replace" if dry_run else "Replaced"
            print(f"{mode} {len(all_replacements)} OutputDebugString() call(s):")
            for r in all_replacements:
                print(r)
        else:
            print("No OutputDebugString() calls found in domain files.")
        print()

    # Report MessageBox inventory
    if all_messagebox:
        print(f"MessageBox() calls requiring manual replacement "
              f"({len(all_messagebox) // 2} call(s)):")
        for entry in all_messagebox:
            print(entry)
    else:
        print("No MessageBox() calls found in domain files.")

    if dry_run and all_replacements:
        sys.exit(1)


if __name__ == '__main__':
    main()
