# Replace File APIs Skill

Replace Win32 file APIs with std::filesystem in domain code (US-P0i from prd-legacy-preparation.md).

## What the Script Automates

| Win32 | Replacement |
|-------|------------|
| `_waccess(path, 0) == 0` | `std::filesystem::exists(path)` |
| `_waccess(path, 0) == -1` | `!std::filesystem::exists(path)` |
| `GetFileAttributes(f) != INVALID_FILE_ATTRIBUTES` | `std::filesystem::exists(f)` |
| `DeleteFile(path)` | `{ std::error_code ec; std::filesystem::remove(path, ec); }` |

## What Needs Manual Work (inventoried)

| Pattern | Replacement Template |
|---------|---------------------|
| `FindFirstFile`/`FindNextFile` loop | `for (auto& entry : fs::directory_iterator(dir)) { if (entry.path().extension() == ext) ... }` |
| `_wfopen_s(&fp, path, L"wb")` | `std::ofstream ofs(fs::path(path), std::ios::binary)` |
| `_wfopen_s(&fp, path, L"rb")` | `std::ifstream ifs(fs::path(path), std::ios::binary)` |
| `_wsplitpath_s(p, drv, dir, name, ext)` | `fs::path(p).stem()`, `.extension()`, `.parent_path()` |
| `GetTempPath(MAX_PATH, buf)` | `fs::temp_directory_path()` |
| `GetCurrentDirectory(N, buf)` | `fs::current_path()` |
| `CreateDirectory(path, NULL)` | `fs::create_directories(path, ec)` |
| `wchar_t buf[MAX_PATH]` | `fs::path buf` |
| `wcscpy_s/wcscat_s with MAX_PATH` | `fs::path /= operations` |

## Procedure

1. **Inventory**: `python3 .claude/skills/replace_file_apis/replace_file_apis.py --inventory`
2. **Dry run**: `python3 .claude/skills/replace_file_apis/replace_file_apis.py --dry-run`
3. **Apply**: `python3 .claude/skills/replace_file_apis/replace_file_apis.py`
4. **Manual**: Handle complex patterns from inventory

## Key Files (by complexity)

- `oEvent.cpp` — 3 FindFirstFile/FindNextFile loops (~90 lines each), 2 _wfopen_s, 3 _wsplitpath_s
- `meos_util.cpp` — 1 FindFirstFile loop (getFiles()), 1 _waccess, 1 GetFileAttributes
- `zip.cpp` — 1 FindFirstFile, 1 _waccess, FILETIME handling
- `HTMLWriter.cpp` — 3 _wsplitpath_s, MAX_PATH buffers
- `oEventSpeaker.cpp` — 2 _waccess calls
- `oClub.cpp` — MAX_PATH buffers
- `image.cpp` — 1 _wsplitpath_s

## FindFirstFile Loop Template

```cpp
// Before:
WIN32_FIND_DATA fd;
HANDLE h = FindFirstFile(dir_pattern, &fd);
if (h != INVALID_HANDLE_VALUE) {
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            wstring name = fd.cFileName;
            // ... use name ...
        }
    } while (FindNextFile(h, &fd));
    FindClose(h);
}

// After:
namespace fs = std::filesystem;
std::error_code ec;
for (const auto& entry : fs::directory_iterator(dir_path, ec)) {
    if (!entry.is_directory()) {
        // Filter by extension if needed
        if (entry.path().extension() == L".meos") {
            wstring name = entry.path().filename().wstring();
            // ... use name ...
        }
    }
}
```

## Gotchas

- `DeleteFile` in error recovery: use `error_code` overload to avoid throwing
- `FindFirstFile` with wildcards like `L"*.meos"`: `directory_iterator` has no glob — filter manually
- `_wsplitpath_s` with `drive` parameter: `fs::path` on Linux has no drive concept — usually safe to ignore
- `MAX_PATH` (260): Only matters for Windows. On Linux paths can be longer.
