#pragma once

#include <string>
#include <vector>

// Portable file/directory utilities (std::filesystem-based, no Win32 APIs).

// Returns true if the file or directory at the given path exists.
bool fileExists(const std::wstring& file);

// Finds files in `dir` whose name matches `pattern` (glob-style wildcard: only
// '*' at the end of a basename is supported, e.g. "*.txt").
// Returns true if any files were found.
bool expandDirectory(const wchar_t* dir, const wchar_t* pattern,
                     std::vector<std::wstring>& res);

// Moves (renames) a file from src to dst, overwriting dst if it exists.
void moveFile(const std::wstring& src, const std::wstring& dst);

// ---------------------------------------------------------------------------
// Portable _memicmp replacement.
// On non-Windows, strncasecmp is used; on Windows _memicmp is available natively.
// Use meos_memicmp() in migrated code instead of _memicmp.
// ---------------------------------------------------------------------------
#ifdef _WIN32
#include <cstring>
inline int meos_memicmp(const void* s1, const void* s2, std::size_t n) {
    return _memicmp(s1, s2, n);
}
#else
#include <strings.h>
inline int meos_memicmp(const void* s1, const void* s2, std::size_t n) {
    return strncasecmp(static_cast<const char*>(s1),
                       static_cast<const char*>(s2), n);
}
#endif
