#include "localizer.h"
#include "meos_util.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;
using meos::util::fromUTF8;
using meos::util::toUTF8;
using meos::util::narrow;

// Global Localizer instance.
Localizer lang;

// ---- LocalizerImpl --------------------------------------------------------

class LocalizerImpl {
    wstring language;
    map<wstring, wstring> table;
    map<wstring, wstring> unknown;

    void addUnknown(const wstring& key);
    void loadTable(const vector<string>& raw, const wstring& lang);

    mutable oWordList givenNames_;

public:
    const oWordList& getGivenNames() const { return givenNames_; }

    void translateAll(const LocalizerImpl& all);
    const wstring& translate(const wstring& str, bool& found);

    void saveUnknown(const wstring& file);
    void saveTable(const wstring& file);
    void saveTranslation(const wstring& file);

    void loadTable(const wstring& filePath, const wstring& lang);

    void clear();

    LocalizerImpl() = default;
    ~LocalizerImpl() = default;
};

void LocalizerImpl::addUnknown(const wstring& key) {
    if (unknown.emplace(key, L"").second) {
        cerr << narrow(L"Missing resource: " + key) << "\n";
    }
}

void LocalizerImpl::clear() {
    table.clear();
    unknown.clear();
    language.clear();
}

// Load translation table from a UTF-8 .lng file.
void LocalizerImpl::loadTable(const wstring& filePath, const wstring& lang) {
    clear();
    // Open using UTF-8 path on Linux
    string path = toUTF8(filePath);
    ifstream fin(path, ios::in);
    if (!fin.good())
        return;

    vector<string> raw;
    string line;
    while (getline(fin, line)) {
        // Strip CRLF
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#' || line[0] == '%')
            continue;
        // Strip UTF-8 BOM if present at start of first line
        if (!line.empty() && static_cast<unsigned char>(line[0]) == 0xEF &&
            line.size() >= 3 &&
            static_cast<unsigned char>(line[1]) == 0xBB &&
            static_cast<unsigned char>(line[2]) == 0xBF) {
            line = line.substr(3);
        }
        if (!line.empty())
            raw.push_back(line);
    }

    loadTable(raw, lang);
}

void LocalizerImpl::loadTable(const vector<string>& raw, const wstring& lang) {
    table.clear();
    language = lang;
    const string nline = "\n";

    for (const auto& s : raw) {
        size_t pos = s.find('=');
        if (pos == string::npos)
            continue;  // Tolerate malformed lines

        size_t spos = pos;
        size_t epos = pos + 1;
        const auto* udata = reinterpret_cast<const unsigned char*>(s.data());

        // Trim trailing spaces/NBSP before '='
        while (spos > 0) {
            if (isspace(udata[spos - 1]))
                --spos;
            else if (udata[spos - 1] == 0xC2 && spos > 1 && udata[spos - 2] == 0xA0)
                spos -= 2;  // UTF-8 NBSP
            else
                break;
        }

        // Trim leading spaces/NBSP after '='
        while (epos < s.size()) {
            if (isspace(udata[epos]))
                ++epos;
            else if (udata[epos] == 0xC2 && epos + 1 < s.size() && udata[epos + 1] == 0xA0)
                epos += 2;  // UTF-8 NBSP
            else
                break;
        }

        string keyStr = s.substr(0, spos);
        string valStr = s.substr(epos);

        if (valStr.empty())
            continue;

        // Strip spurious 'Â' artifact from broken encodings
        if (valStr.size() > 1 && valStr[0] == '\xC3' && (unsigned char)valStr[1] == 0x82)
            valStr = valStr.substr(2);

        // Replace escaped newlines
        size_t nl = valStr.find("\\n");
        while (nl != string::npos) {
            valStr.replace(nl, 2, nline);
            nl = valStr.find("\\n");
        }

        wstring key = fromUTF8(keyStr);
        wstring val = fromUTF8(valStr);
        table[key] = val;
    }
}

const wstring& LocalizerImpl::translate(const wstring& str, bool& found) {
    found = false;
    // Thread-local rotating buffer to support returning const wstring& safely
    thread_local static int i = 0;
    constexpr int kBufSize = 17;
    thread_local static wstring value[kBufSize];

    auto next = [&]() -> wstring& {
        i = (i + 1) % kBufSize;
        return value[i];
    };

    size_t len = str.size();
    if (len == 0) {
        found = true;
        thread_local static wstring empty;
        return empty;
    }

    // '#' prefix → strip and return rest verbatim
    if (str[0] == L'#') {
        wstring& v = next();
        v = str.substr(1);
        found = true;
        return v;
    }

    auto isDigit = [](wchar_t c) { return c >= L'0' && c <= L'9'; };

    // Leading punctuation/digits → translate the non-punctuation suffix
    if (str[0] == L',' || str[0] == L' ' || str[0] == L'.'
        || str[0] == L':' || str[0] == L';' || str[0] == L'<' || str[0] == L'>'
        || str[0] == L'-' || str[0] == 0x96 || str[0] == L'\xD7' || isDigit(str[0])) {
        size_t k = 1;
        while (k < len && (str[k] == L' ' || str[k] == L'.' || str[k] == L':'
            || str[k] == L'<' || str[k] == L'>'
            || str[k] == L'-' || str[k] == 0x96 || str[k] == L'\xD7' || isDigit(str[k])))
            ++k;

        if (k < len) {
            bool f;
            wstring& v = next();
            v = str.substr(0, k) + translate(str.substr(k), f);
            return v;
        }
    }

    // Exact lookup
    auto it = table.find(str);
    if (it != table.end()) {
        found = true;
        return it->second;
    }

    // '#' substitution pattern: "base#arg0#arg1#..." with X,Y,Z,W placeholders
    size_t subst = str.find(L'#');
    if (subst != wstring::npos) {
        bool f;
        wstring s = translate(str.substr(0, subst), f);
        vector<wstring> args;
        {
            wstring rest = str.substr(subst + 1);
            size_t p = 0;
            while (p <= rest.size()) {
                size_t q = rest.find(L'#', p);
                if (q == wstring::npos) q = rest.size();
                args.push_back(rest.substr(p, q - p));
                p = q + 1;
            }
        }
        args.push_back(L"");
        const wchar_t* syms = L"XYZW";
        size_t symPos = 0;
        wstring ret;
        size_t lastPos = 0;
        for (size_t k = 0; k < s.size(); ++k) {
            if (symPos >= args.size() || symPos >= 4) break;
            if (s[k] == syms[symPos]) {
                if (k > 0 && isalnum(s[k - 1])) continue;
                if (k + 1 < s.size() && isalnum(s[k + 1])) continue;
                ret += s.substr(lastPos, k - lastPos);
                ret += args[symPos];
                lastPos = k + 1;
                ++symPos;
            }
        }
        if (lastPos < s.size())
            ret += s.substr(lastPos);
        wstring& v = next();
        swap(v, ret);
        return v;
    } else if (str[0] == L'@') {
        // '@' prefix → untranslated, strip prefix
        wstring& v = next();
        v = str.substr(1);
        found = true;
        return v;
    }

    // Trailing-punctuation strip: try lookup without trailing suffix
    wchar_t last = str[len - 1];
    if (last != L':' && last != L'.' && last != L' ' && last != L','
        && last != L';' && last != L'<' && last != L'>' && last != L'-'
        && last != 0x96 && last != 215 && !isDigit(last)) {
        found = false;
        wstring& v = next();
        v = str;
        return v;
    }

    size_t pos = str.find_last_not_of(last);
    while (pos > 0) {
        wchar_t c = str[pos];
        if (c != L':' && c != L' ' && c != L',' && c != L'.'
            && c != L';' && c != L'<' && c != L'>' && c != L'-'
            && c != 0x96 && c != 215 && !isDigit(c))
            break;
        pos = str.find_last_not_of(c, pos);
    }

    wstring suffix = str.substr(pos + 1);
    wstring key = str.substr(0, str.size() - suffix.size());
    it = table.find(key);
    if (it != table.end()) {
        wstring& v = next();
        v = it->second + suffix;
        found = true;
        return v;
    }

    found = false;
    wstring& v = next();
    v = str;
    return v;
}

void LocalizerImpl::translateAll(const LocalizerImpl& all) {
    for (const auto& kv : all.table) {
        bool f;
        translate(kv.first, f);
        if (!f)
            unknown[kv.first] = kv.second;
    }
}

void LocalizerImpl::saveUnknown(const wstring& file) {
    if (unknown.empty()) return;
    string path = toUTF8(file);
    ofstream fout(path, ios::trunc | ios::out);
    for (auto& kv : unknown) {
        wstring val = kv.second.empty() ? kv.first : kv.second;
        // Escape embedded newlines
        size_t nl = val.find(L'\n');
        while (nl != wstring::npos) {
            val.replace(nl, 1, L"\\n");
            nl = val.find(L'\n', nl + 2);
        }
        fout << toUTF8(kv.first) << " = " << toUTF8(val) << "\n";
    }
}

void LocalizerImpl::saveTable(const wstring& file) {
    string path = toUTF8(language + L"_" + file);
    ofstream fout(path, ios::trunc | ios::out);
    for (auto& kv : table) {
        wstring val = kv.second;
        size_t nl = val.find(L'\n');
        while (nl != wstring::npos) {
            val.replace(nl, 1, L"\\n");
            nl = val.find(L'\n', nl + 2);
        }
        fout << toUTF8(kv.first) << " = " << toUTF8(val) << "\n";
    }
}

void LocalizerImpl::saveTranslation(const wstring& file) {
    string path = toUTF8(language + L"_" + file);
    ofstream fout(path, ios::trunc | ios::out);
    for (auto& kv : table)
        fout << toUTF8(kv.second) << "\n";
}

// ---- Localizer::LocalizerInternal -----------------------------------------

Localizer::LocalizerInternal::LocalizerInternal()
    : impl(new LocalizerImpl()), implBase(nullptr), owning(true), user(nullptr) {}

Localizer::LocalizerInternal::~LocalizerInternal() {
    if (user) {
        user->owning = true;
        impl = nullptr;
        implBase = nullptr;
    } else {
        delete impl;
        delete implBase;
    }
}

void Localizer::LocalizerInternal::set(Localizer& lio) {
    LocalizerInternal& li = *lio.linternal;
    if (li.user || user)
        throw runtime_error("Runtime error");
    if (owning) {
        delete impl;
        delete implBase;
    }
    implBase = li.implBase;
    impl = li.impl;
    li.user = this;
    owning = false;
}

vector<wstring> Localizer::LocalizerInternal::getLangResource() const {
    vector<wstring> v;
    for (const auto& kv : langResource)
        v.push_back(kv.first);
    return v;
}

const oWordList& Localizer::LocalizerInternal::getGivenNames() const {
    return impl->getGivenNames();
}

void Localizer::LocalizerInternal::addLangResource(const wstring& name, const wstring& filePath) {
    langResource[name] = filePath;
    if (implBase == nullptr) {
        implBase = new LocalizerImpl();
        implBase->loadTable(filePath, name);
    }
}

void Localizer::LocalizerInternal::loadLangResource(const wstring& name) {
    auto it = langResource.find(name);
    if (it == langResource.end())
        throw runtime_error("Unknown language");
    impl->loadTable(it->second, name);
}

const wstring& Localizer::LocalizerInternal::tl(const wstring& str) const {
    bool found;
    const wstring* ret = &impl->translate(str, found);
    if (found || !implBase)
        return *ret;
    ret = &implBase->translate(str, found);
    return *ret;
}

void Localizer::LocalizerInternal::debugDump(const wstring& untranslated, const wstring& translated) const {
    if (implBase)
        impl->translateAll(*implBase);
    impl->saveUnknown(untranslated);
    impl->saveTable(translated);
    impl->saveTranslation(L"spellcheck.txt");
}

// ---- Localizer methods ----------------------------------------------------

bool Localizer::capitalizeWords() const {
    return tl(wstring(L"Lyssna")) == L"Listen";
}

const wstring& Localizer::tl(const string& str) const {
    if (str.empty()) {
        thread_local static wstring empty;
        return empty;
    }
    // Narrow ASCII → wide (safe for keys that are plain ASCII)
    wstring key(str.begin(), str.end());
    for (auto& c : key)
        c = static_cast<wchar_t>(static_cast<unsigned char>(c));
    return linternal->tl(key);
}

wstring Localizer::tl(const wstring& str, bool cap) const {
    wstring w = linternal->tl(str);
    if (cap && capitalizeWords()) {
        // Simple capitalize-first-letter
        if (!w.empty())
            w[0] = towupper(w[0]);
    }
    return w;
}
