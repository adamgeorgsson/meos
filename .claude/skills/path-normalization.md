# Path Normalization Skill

This skill provides guidance and automation for normalizing path separators in legacy Win32 codebases, preparing them for cross-platform (Linux/macOS) builds.

## Problem: Case and Separators

Windows uses backslashes (`\\`) and is case-insensitive. Linux uses forward slashes (`/`) and is case-sensitive. Hardcoded backslashes in strings (e.g., `L"C:\\Path\\File.txt"`) will fail on Linux.

## Solution: std::filesystem::path

Standard C++17 `std::filesystem::path` provides a portable way to handle paths.

### Patterns for Replacement

1.  **Concatenation:**
    -   *Old:* `path += L"\\file.txt";`
    -   *New:* `path = (std::filesystem::path(path) / "file.txt").wstring();`

2.  **Parent Directory:**
    -   *Old:* Find last `\\` or `/` manually.
    -   *New:* `std::filesystem::path(file).parent_path().wstring();`

3.  **Ensure Trailing Separator:**
    -   *Old:* `if (path.back() != '\\') path += L"\\";`
    -   *New:* `path = (std::filesystem::path(path) / "").wstring();`

4.  **Literal Paths:**
    -   *Old:* `L".\\..\\Lists\\file.txt"`
    -   *New:* `(std::filesystem::path(".") / ".." / "Lists" / "file.txt").wstring()`

### Automation Script (find_paths.py)

Use this Python snippet to identify potential path strings in the codebase:

```python
import os
import re

# Regex for strings with backslashes (literal \\ in C++ is \\\\ in regex)
string_re = re.compile(r'L?"[^"]*\\\\+[^"]*"')

def find_paths(directory):
    for root, dirs, files in os.walk(directory):
        for file in files:
            if file.endswith(('.cpp', '.h')):
                path = os.path.join(root, file)
                with open(path, 'r', encoding='utf-8', errors='ignore') as f:
                    for i, line in enumerate(f):
                        for match in string_re.findall(line):
                            # Filter out common escape sequences
                            clean = match
                            for esc in ['\\n', '\\t', '\\r', '\\"', "\\'", '\\u', '\\x']:
                                clean = clean.replace(esc, '')
                            if '\\' in clean:
                                print(f"{path}:{i+1}: {match}")
```

## Gotchas

-   **SQL Quoting:** Be careful not to replace `\\` in SQL escaping logic (e.g., `SQL_quote`).
-   **Regex/Wildcards:** Backslashes in regex or wildcard patterns (e.g., `\\/.?*`) should be left alone if they aren't path separators.
-   **Win32 APIs:** Some functions (like `FindFirstFile`) might still require a specific format. Always verify against the target API's requirements.
-   **PCH:** Always ensure `#include <filesystem>` is added correctly, usually after the precompiled header (`StdAfx.h`).
