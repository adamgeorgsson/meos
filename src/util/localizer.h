#pragma once

#include <map>
#include <string>
#include <vector>

// Stub for oWordList — real implementation deferred (domain dependency).
class oWordList {
public:
    bool lookup(const std::wstring& /*word*/, std::wstring& /*result*/) const { return false; }
};

class LocalizerImpl;

class Localizer {
public:
    class LocalizerInternal {
    private:
        std::map<std::wstring, std::wstring> langResource;
        LocalizerImpl* impl;
        LocalizerImpl* implBase;
        bool owning;
        LocalizerInternal* user;

    public:
        void debugDump(const std::wstring& untranslated, const std::wstring& translated) const;

        std::vector<std::wstring> getLangResource() const;

        // Load the language previously registered under @p name.
        void loadLangResource(const std::wstring& name);

        // Register @p name → @p filePath so it can be loaded via loadLangResource.
        // The first call also loads that file into the base (fallback) table.
        void addLangResource(const std::wstring& name, const std::wstring& filePath);

        // Translate @p str; returns the input unchanged if no translation found.
        const std::wstring& tl(const std::wstring& str) const;

        void set(Localizer& li);

        const oWordList& getGivenNames() const;

        LocalizerInternal();
        ~LocalizerInternal();
    };

private:
    LocalizerInternal* linternal;

public:
    bool capitalizeWords() const;

    LocalizerInternal& get() { return *linternal; }

    const std::wstring& tl(const std::string& str) const;
    const std::wstring& tl(const std::wstring& str) const { return linternal->tl(str); }
    std::wstring tl(const std::wstring& str, bool cap) const;

    void init() { linternal = new LocalizerInternal(); }
    void unload() { delete linternal; linternal = nullptr; }

    Localizer() : linternal(nullptr) {}
    ~Localizer() { unload(); }
};

extern Localizer lang;
