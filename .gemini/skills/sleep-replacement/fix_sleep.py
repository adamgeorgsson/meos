#!/usr/bin/env python3
"""Replace Win32 Sleep() with std::this_thread::sleep_for() in domain files.

Usage:
    python fix_sleep.py              # Apply replacements
    python fix_sleep.py --dry-run    # Report what would change without modifying files

Run from the repository root (parent of code/).
"""
import os
import re
import sys

CODE_DIR = 'code'

# Files/patterns to exclude (UI code, third-party libraries)
EXCLUDE_PATTERNS = [
    re.compile(r'^Tab\w+\.(cpp|h)$'),       # Tab UI classes
    re.compile(r'^gdioutput\.(cpp|h)$'),     # GUI rendering
    re.compile(r'^meos\.cpp$'),              # Composition root
    re.compile(r'^Table\.(cpp|h)$'),         # UI table widget
]
EXCLUDE_DIRS = {'mysql', 'restbed'}


def is_excluded(filename, rel_dir):
    """Check if a file should be excluded from processing."""
    parts = rel_dir.replace('\\', '/').split('/')
    if any(d in EXCLUDE_DIRS for d in parts if d != '.'):
        return True
    return any(p.match(filename) for p in EXCLUDE_PATTERNS)


def find_sleep_calls(code_dir, dry_run=False):
    """Find and replace Sleep() calls in domain files."""
    sleep_re = re.compile(
        r'^(\s*)'           # leading whitespace
        r'Sleep\('          # Sleep(
        r'([^)]+)'          # argument (any expression)
        r'\);'              # );
    )
    comment_re = re.compile(r'^\s*//')

    replacements = []
    files_needing_includes = set()

    for root, dirs, files in os.walk(code_dir):
        dirs[:] = [d for d in dirs if d not in EXCLUDE_DIRS]

        rel_dir = os.path.relpath(root, code_dir)
        for f in sorted(files):
            if not f.endswith(('.cpp', '.h', '.hpp')):
                continue
            if is_excluded(f, rel_dir):
                continue

            file_path = os.path.join(root, f)
            with open(file_path, 'r', encoding='latin-1') as fh:
                lines = fh.readlines()

            changed = False
            new_lines = []
            has_thread_include = False
            has_chrono_include = False

            for line in lines:
                if '#include <thread>' in line:
                    has_thread_include = True
                if '#include <chrono>' in line:
                    has_chrono_include = True

                # Skip commented-out Sleep lines
                if comment_re.match(line):
                    new_lines.append(line)
                    continue

                match = sleep_re.match(line)
                if match:
                    indent = match.group(1)
                    arg = match.group(2).strip()
                    replacement = (
                        f"{indent}std::this_thread::sleep_for("
                        f"std::chrono::milliseconds({arg}));\n"
                    )
                    rel_file = os.path.relpath(file_path, code_dir)
                    line_num = len(new_lines) + 1
                    replacements.append(
                        f"  {rel_file}:{line_num}: "
                        f"Sleep({arg}) -> sleep_for(milliseconds({arg}))"
                    )
                    new_lines.append(replacement)
                    changed = True
                else:
                    new_lines.append(line)

            if changed:
                needs_thread = not has_thread_include
                needs_chrono = not has_chrono_include
                if needs_thread or needs_chrono:
                    files_needing_includes.add(file_path)
                    new_lines = add_missing_includes(
                        new_lines, needs_thread, needs_chrono
                    )

                if not dry_run:
                    with open(file_path, 'w', encoding='latin-1') as fh:
                        fh.writelines(new_lines)

    return replacements, files_needing_includes


def add_missing_includes(lines, needs_thread, needs_chrono):
    """Insert #include <thread> and/or <chrono> after the last #include."""
    last_include_idx = -1
    for i, line in enumerate(lines):
        if line.strip().startswith('#include'):
            last_include_idx = i

    if last_include_idx == -1:
        last_include_idx = 0

    inserts = []
    if needs_chrono:
        inserts.append('#include <chrono>\n')
    if needs_thread:
        inserts.append('#include <thread>\n')

    for j, inc in enumerate(inserts):
        lines.insert(last_include_idx + 1 + j, inc)

    return lines


def main():
    dry_run = '--dry-run' in sys.argv

    if not os.path.isdir(CODE_DIR):
        print(f"Error: '{CODE_DIR}' directory not found. "
              "Run from the repository root.")
        sys.exit(1)

    replacements, includes_added = find_sleep_calls(CODE_DIR, dry_run=dry_run)

    if replacements:
        mode = "Would replace" if dry_run else "Replaced"
        print(f"{mode} {len(replacements)} Sleep() call(s):")
        for r in replacements:
            print(r)
        if includes_added:
            verb = "would need" if dry_run else "had"
            print(f"\n{len(includes_added)} file(s) {verb} "
                  "#include <thread>/<chrono> added:")
            for f in sorted(includes_added):
                print(f"  {os.path.relpath(f, CODE_DIR)}")
        if dry_run:
            sys.exit(1)
    else:
        print("No Sleep() calls found in domain files.")


if __name__ == '__main__':
    main()
