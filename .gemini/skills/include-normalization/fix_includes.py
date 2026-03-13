#!/usr/bin/env python3
"""Fix #include directive casing to match actual filenames on disk.

Usage:
    python fix_includes.py              # Fix all mismatches
    python fix_includes.py --dry-run    # Report mismatches without changing files

Run from the repository root (parent of code/).
"""
import os
import re
import sys

CODE_DIR = 'code'


def build_file_map(code_dir):
    """Build a map of lowercase relative path -> actual relative path."""
    file_map = {}
    for root, dirs, files in os.walk(code_dir):
        for f in files:
            rel_dir = os.path.relpath(root, code_dir)
            if rel_dir == '.':
                path = f
            else:
                path = os.path.join(rel_dir, f)
            path = path.replace('\\', '/')
            file_map[path.lower()] = path
    return file_map


def find_and_fix(code_dir, file_map, dry_run=False):
    """Scan all source files and fix or report include casing mismatches."""
    include_re = re.compile(r'(^#include\s+")([^"]+)(")')
    mismatches = []

    for root, dirs, files in os.walk(code_dir):
        for f in files:
            if not f.endswith(('.cpp', '.h', '.hpp')):
                continue

            file_path = os.path.join(root, f)
            with open(file_path, 'r', encoding='latin-1') as fr:
                lines = fr.readlines()

            changed = False
            new_lines = []
            for line_num, line in enumerate(lines, 1):
                match = include_re.match(line)
                if match:
                    prefix, included_file, suffix = match.groups()
                    included_normalized = included_file.replace('\\', '/')

                    file_dir = os.path.relpath(root, code_dir)

                    # Try relative to the including file's directory first
                    if file_dir != '.':
                        search_key = os.path.join(
                            file_dir, included_normalized
                        ).replace('\\', '/').lower()
                    else:
                        search_key = included_normalized.lower()

                    # Fallback: try as path relative to code_dir root
                    direct_key = included_normalized.lower()

                    actual_path = None
                    if search_key in file_map:
                        actual_path = file_map[search_key]
                        if file_dir != '.':
                            actual_path = os.path.relpath(
                                os.path.join(code_dir, actual_path), root
                            ).replace('\\', '/')
                    elif direct_key in file_map:
                        actual_path = file_map[direct_key]

                    if actual_path:
                        actual_path = actual_path.replace('\\', '/')
                        if actual_path != included_file:
                            rel_file = os.path.relpath(file_path, code_dir)
                            mismatches.append(
                                f"  {rel_file}:{line_num}: "
                                f'"{included_file}" -> "{actual_path}"'
                            )
                            new_lines.append(f"{prefix}{actual_path}{suffix}\n")
                            changed = True
                        else:
                            new_lines.append(line)
                    else:
                        new_lines.append(line)
                else:
                    new_lines.append(line)

            if changed and not dry_run:
                with open(file_path, 'w', encoding='latin-1') as fw:
                    fw.writelines(new_lines)

    return mismatches


def main():
    dry_run = '--dry-run' in sys.argv

    if not os.path.isdir(CODE_DIR):
        print(f"Error: '{CODE_DIR}' directory not found. "
              "Run from the repository root.")
        sys.exit(1)

    file_map = build_file_map(CODE_DIR)
    mismatches = find_and_fix(CODE_DIR, file_map, dry_run=dry_run)

    if mismatches:
        mode = "Found" if dry_run else "Fixed"
        print(f"{mode} {len(mismatches)} include casing mismatch(es):")
        for m in mismatches:
            print(m)
        if dry_run:
            sys.exit(1)  # Non-zero exit = mismatches remain
    else:
        print("No include casing mismatches found.")


if __name__ == '__main__':
    main()
