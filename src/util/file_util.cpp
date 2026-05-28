#include "file_util.h"

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// Convert a wide-character path to a std::filesystem::path.
// On Linux, std::filesystem::path accepts wchar_t* directly via its templated
// constructor when the character type is wchar_t. On GCC this requires creating
// the path from the raw pointer rather than from a wstring value to avoid the
// ambiguous implicit-conversion issue.
static fs::path toFsPath(const std::wstring& wpath) {
    return fs::path(wpath.c_str());
}

bool fileExists(const std::wstring& file) {
    return fs::exists(toFsPath(file));
}

bool expandDirectory(const wchar_t* dir, const wchar_t* pattern,
                     std::vector<std::wstring>& res) {
    std::wstring dirStr{dir};
    fs::path dirPath = toFsPath(dirStr);
    if (!fs::is_directory(dirPath))
        return false;

    // Convert the pattern to a string for matching.
    // Supports a single leading '*' wildcard and an extension, e.g. "*.xml".
    std::wstring pat{pattern};
    bool startsWithStar = !pat.empty() && pat[0] == L'*';
    std::wstring suffix = startsWithStar ? pat.substr(1) : pat;  // e.g. ".xml"

    bool found = false;
    for (const auto& entry : fs::directory_iterator(dirPath)) {
        if (!entry.is_regular_file())
            continue;
        std::wstring name = entry.path().filename().wstring();
        bool match = false;
        if (startsWithStar) {
            // Match if filename ends with the suffix.
            if (suffix.empty() || (name.size() >= suffix.size() &&
                name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0))
                match = true;
        } else {
            match = (name == pat);
        }
        if (match) {
            res.push_back(entry.path().wstring());
            found = true;
        }
    }
    return found;
}

void moveFile(const std::wstring& src, const std::wstring& dst) {
    std::error_code ec;
    fs::rename(toFsPath(src), toFsPath(dst), ec);
    if (ec)
        throw std::runtime_error("moveFile failed: " + ec.message());
}
