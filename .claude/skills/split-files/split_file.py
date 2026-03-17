#!/usr/bin/env python3
"""Split a large C++ source file into multiple files at specified line boundaries.

Usage:
    python3 split_file.py <source.cpp> <split_spec> [--dry-run]

Where <split_spec> is a comma-separated list of:
    start_line-end_line:new_filename.cpp

Example:
    python3 split_file.py code/oEvent.cpp \
        "2843-5318:oEventAdmin.cpp,5319-7405:oEventConfig.cpp" \
        --dry-run

The script will:
1. Extract the copyright header and #include block from the source file
2. Create each new file with: copyright + includes + extracted lines
3. Remove extracted lines from the source (replace with blank line as separator)
4. Add new files to MeOS.vcxproj

Lines are 1-indexed (matching editor line numbers).
Process splits from LAST to FIRST to keep line numbers stable.
"""

import argparse
import os
import re
import sys
import xml.etree.ElementTree as ET


def parse_split_spec(spec_str):
    """Parse 'start-end:filename,start-end:filename,...' into list of tuples."""
    splits = []
    for part in spec_str.split(","):
        part = part.strip()
        if not part:
            continue
        range_part, filename = part.split(":")
        start, end = range_part.split("-")
        splits.append((int(start), int(end), filename.strip()))
    # Sort by start line descending so we can remove from bottom up
    splits.sort(key=lambda x: x[0], reverse=True)
    return splits


def extract_header_and_includes(lines):
    """Extract copyright header and #include block from source lines.

    Returns (header_lines, include_lines, first_code_line_idx).
    Header = everything up to and including the closing ***/ line.
    Includes = all #include, #define, extern, and blank lines after header until first code.
    """
    header_end = 0
    for i, line in enumerate(lines):
        if line.rstrip().endswith("***/") or line.rstrip().endswith("*/"):
            if "****" in line or "Copyright" in "".join(lines[max(0, i - 5):i]):
                header_end = i + 1
                break

    # Now find includes block: #include, #define, extern, blank lines, // comments
    include_end = header_end
    for i in range(header_end, len(lines)):
        stripped = lines[i].strip()
        if (stripped.startswith("#include") or
                stripped.startswith("#define") or
                stripped.startswith("extern ") or
                stripped.startswith("//") or
                stripped == ""):
            include_end = i + 1
        else:
            break

    return lines[:header_end], lines[header_end:include_end], include_end


def find_vcxproj(source_dir):
    """Find MeOS.vcxproj in the source directory."""
    vcxproj = os.path.join(source_dir, "MeOS.vcxproj")
    if os.path.exists(vcxproj):
        return vcxproj
    return None


def add_to_vcxproj(vcxproj_path, new_filenames, dry_run=False):
    """Add new .cpp files to the vcxproj ClCompile ItemGroup."""
    with open(vcxproj_path, "r", encoding="utf-8") as f:
        content = f.read()

    for filename in new_filenames:
        entry = f'    <ClCompile Include="{filename}" />'
        if filename in content:
            print(f"  [vcxproj] {filename} already present")
            continue

        # Find the ItemGroup with ClCompile entries and insert alphabetically
        # Look for existing ClCompile entries and insert in order
        pattern = r'(<ClCompile Include="([^"]+)" />\n)'
        matches = list(re.finditer(pattern, content))

        if not matches:
            print(f"  [vcxproj] WARNING: No ClCompile entries found")
            continue

        # Find insertion point (alphabetical)
        insert_after = None
        for m in matches:
            existing_name = m.group(2)
            if existing_name.lower() < filename.lower():
                insert_after = m
            else:
                break

        if insert_after:
            insert_pos = insert_after.end()
            content = content[:insert_pos] + entry + "\n" + content[insert_pos:]
        else:
            # Insert before the first entry
            insert_pos = matches[0].start()
            content = content[:insert_pos] + entry + "\n" + content[insert_pos:]

        print(f"  [vcxproj] {'Would add' if dry_run else 'Added'} {filename}")

    if not dry_run:
        with open(vcxproj_path, "w", encoding="utf-8") as f:
            f.write(content)


def split_file(source_path, split_spec, dry_run=False):
    """Main split operation."""
    source_dir = os.path.dirname(os.path.abspath(source_path))

    with open(source_path, "r", encoding="utf-8") as f:
        lines = f.readlines()

    total_lines = len(lines)
    print(f"Source: {source_path} ({total_lines} lines)")

    header, includes, first_code_idx = extract_header_and_includes(lines)
    print(f"  Header: {len(header)} lines, Includes: {len(includes)} lines")
    print(f"  First code at line {first_code_idx + 1}")

    splits = parse_split_spec(split_spec)
    new_filenames = []

    for start, end, filename in splits:
        # Convert to 0-indexed
        start_idx = start - 1
        end_idx = end  # end is inclusive, but Python slicing is exclusive

        if start_idx < 0 or end_idx > total_lines:
            print(f"  ERROR: Line range {start}-{end} out of bounds (file has {total_lines} lines)")
            sys.exit(1)

        extracted = lines[start_idx:end_idx]
        remaining_after = total_lines - (end_idx - start_idx)

        # Build the new file content
        new_content = header + ["\n"] + includes + ["\n"] + extracted

        new_path = os.path.join(source_dir, filename)
        new_filenames.append(filename)

        # Count non-blank lines
        code_lines = sum(1 for l in extracted if l.strip())

        print(f"\n  Split: lines {start}-{end} -> {filename}")
        print(f"    {len(extracted)} lines ({code_lines} non-blank)")
        print(f"    New file total: {len(new_content)} lines")

        if not dry_run:
            with open(new_path, "w", encoding="utf-8") as f:
                f.writelines(new_content)
            print(f"    Written: {new_path}")

        # Remove extracted lines from source (replace with a blank separator)
        lines[start_idx:end_idx] = ["\n"]

    # Write modified source
    remaining_lines = len(lines)
    code_remaining = sum(1 for l in lines if l.strip())
    print(f"\n  Source after split: {remaining_lines} lines ({code_remaining} non-blank)")

    if not dry_run:
        with open(source_path, "w", encoding="utf-8") as f:
            f.writelines(lines)
        print(f"  Written: {source_path}")

    # Update vcxproj
    vcxproj = find_vcxproj(source_dir)
    if vcxproj:
        add_to_vcxproj(vcxproj, new_filenames, dry_run)
    else:
        print("  WARNING: MeOS.vcxproj not found")

    if dry_run:
        print("\n  [DRY RUN] No files modified.")


def main():
    parser = argparse.ArgumentParser(description="Split a C++ source file")
    parser.add_argument("source", help="Source .cpp file to split")
    parser.add_argument("split_spec", help="Comma-separated split specs: start-end:filename.cpp,...")
    parser.add_argument("--dry-run", action="store_true", help="Show what would happen without modifying files")
    args = parser.parse_args()

    split_file(args.source, args.split_spec, args.dry_run)


if __name__ == "__main__":
    main()
