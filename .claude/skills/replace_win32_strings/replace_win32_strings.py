#!/usr/bin/env python3
"""Replace Win32-specific string functions with portable C++17 equivalents.

Run from repo root:
  python3 .claude/skills/replace_win32_strings/replace_win32_strings.py code/

Only modifies domain files — skips Tab*.cpp/h and gdioutput*.cpp/h (UI files).

Replacements:
  _wtoi(x)             -> wtoi(x)          [wtoi declared in meos_util.h]
  _itow_s(v,b,N,10)    -> swprintf(b,N,L"%d",v)
  _itow_s(v,b,10)      -> swprintf(b,sizeof(b)/sizeof(wchar_t),L"%d",v)
  sprintf_s(b,"f",x)   -> snprintf(b,sizeof(b),"f",x)      [2-arg form]
  sprintf_s(b,N,"f",x) -> snprintf(b,N,"f",x)              [3-arg form]
  swprintf_s(b,L"f",x) -> swprintf(b,sizeof(b)/sizeof(wchar_t),L"f",x) [2-arg]
  swprintf_s(b,N,L"f") -> swprintf(b,N,L"f",x)             [3-arg form]
"""

import re
import os
import sys
from pathlib import Path

# UI files: Tab*.cpp/h and gdioutput*.cpp/h — Tab must be followed by uppercase letter
# to avoid matching Table.cpp as a UI file.
_UI_PATTERN = re.compile(r'^(Tab[A-Z]|gdioutput)')


def should_skip(path: Path) -> bool:
    return bool(_UI_PATTERN.match(path.name))


def replace_wtoi(content: str) -> str:
    """Replace _wtoi( with wtoi("""
    return content.replace('_wtoi(', 'wtoi(')


def replace_itow_s(content: str) -> str:
    """Replace _itow_s with swprintf equivalents."""
    # 4-arg explicit form: _itow_s(val, buf, N, 10) -> swprintf(buf, N, L"%d", val)
    # First arg may include array subscript like Numbers[i]
    content = re.sub(
        r'_itow_s\((\w[\w\[\]]*),\s*(\w+),\s*(\d+),\s*10\)',
        r'swprintf(\2, \3, L"%d", \1)',
        content
    )
    # 3-arg template form: _itow_s(val, buf, 10) -> swprintf(buf, sizeof(buf)/sizeof(wchar_t), L"%d", val)
    content = re.sub(
        r'_itow_s\((\w[\w\[\]]*),\s*(\w+),\s*10\)',
        r'swprintf(\2, sizeof(\2)/sizeof(wchar_t), L"%d", \1)',
        content
    )
    return content


def replace_printf_s_on_line(line: str) -> str:
    """Replace sprintf_s and swprintf_s on a single line.

    Heuristic:
    - sprintf_s(buf, "fmt", ...) [2-arg]: second arg starts with "
    - sprintf_s(buf, N, "fmt", ...) [3-arg]: second arg is anything else
    - swprintf_s(wbuf, L"fmt", ...) [2-arg]: second arg does NOT start with digit
    - swprintf_s(wbuf, N, L"fmt", ...) [3-arg]: second arg starts with digit
    """
    # Skip commented-out lines
    stripped = line.lstrip()
    if stripped.startswith('//'):
        return line

    # ---- sprintf_s ----
    if 'sprintf_s(' in line and 'swprintf_s(' not in line:
        def replace_sprintf(m):
            prefix = m.group(1)
            buf = m.group(2)
            rest = m.group(3)  # everything after first comma (with leading spaces)
            rest_stripped = rest.lstrip()
            if rest_stripped.startswith('"'):
                # 2-arg: insert sizeof
                return f'{prefix}snprintf({buf}, sizeof({buf}), {rest}'
            else:
                # 3-arg: just rename
                return f'{prefix}snprintf({buf}, {rest}'

        line = re.sub(
            r'(.*?)sprintf_s\((\w[\w\[\]]*),\s*(.*)',
            replace_sprintf,
            line,
            count=1,
            flags=re.DOTALL
        )

    # ---- swprintf_s ----
    if 'swprintf_s(' in line:
        def replace_swprintf(m):
            prefix = m.group(1)
            wbuf = m.group(2)
            rest = m.group(3)
            rest_stripped = rest.lstrip()
            if rest_stripped and rest_stripped[0].isdigit():
                # 3-arg: second arg is numeric size, just rename
                return f'{prefix}swprintf({wbuf}, {rest}'
            else:
                # 2-arg: insert sizeof/sizeof(wchar_t)
                return f'{prefix}swprintf({wbuf}, sizeof({wbuf})/sizeof(wchar_t), {rest}'

        line = re.sub(
            r'(.*?)swprintf_s\((\w[\w\[\]]*),\s*(.*)',
            replace_swprintf,
            line,
            count=1,
            flags=re.DOTALL
        )

        # Fallback: handle swprintf_s with non-simple first arg (e.g., buf+offset)
        # These are always 3-arg form; just rename the function
        if 'swprintf_s(' in line:
            line = line.replace('swprintf_s(', 'swprintf(')

    return line


def process_file(filepath: Path, dry_run: bool = False) -> bool:
    """Process a single file. Returns True if modified."""
    try:
        content = filepath.read_text(encoding='utf-8')
    except UnicodeDecodeError:
        content = filepath.read_text(encoding='latin-1')

    original = content

    # Replace _wtoi
    content = replace_wtoi(content)

    # Replace _itow_s
    content = replace_itow_s(content)

    # Replace sprintf_s and swprintf_s line by line
    lines = content.split('\n')
    new_lines = []
    for line in lines:
        if 'sprintf_s(' in line or 'swprintf_s(' in line:
            line = replace_printf_s_on_line(line)
        new_lines.append(line)
    content = '\n'.join(new_lines)

    if content != original:
        if not dry_run:
            filepath.write_text(content, encoding='utf-8')
        return True
    return False


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <code_dir> [--dry-run]")
        sys.exit(1)

    code_dir = Path(sys.argv[1])
    dry_run = '--dry-run' in sys.argv
    modified = []

    for ext in ['*.cpp', '*.h']:
        for filepath in sorted(code_dir.glob(ext)):
            if should_skip(filepath):
                continue
            if process_file(filepath, dry_run=dry_run):
                modified.append(filepath.name)
                print(f"  {'WOULD MODIFY' if dry_run else 'MODIFIED'}: {filepath.name}")

    print(f"\n{'Would modify' if dry_run else 'Modified'} {len(modified)} files.")


if __name__ == '__main__':
    main()
