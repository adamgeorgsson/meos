// gdioutput.h — Minimal gdioutput interface / stub for platform-independent builds.
// Moved from src/domain/meos_dom_stubs.h (US-003k).
// Real gdioutput lives in the Win32 UI layer; this stub satisfies domain signatures.
#pragma once

#include <string>
#include <vector>
#include <utility>

// ── InputInfo (stub — real class lives in gdistructures.h) ────────────────────
class InputInfo {
public:
  std::wstring text;
};

// ── gdioutput stub ─────────────────────────────────────────────────────────────
// All GUI methods are no-ops so domain code compiles without Win32/GDI.
class gdioutput {
public:
  enum class AskAnswer { AnswerOk, AnswerCancel };

  // Field builders
  InputInfo& addInput(const std::string&, const std::wstring& = L"", int = 10, int = 0,
                      const std::wstring& = L"") { static InputInfo ii; return ii; }
  void addSelection(const std::string&, int, int, void*, const std::wstring&) {}
  void setItems(const std::string&, std::vector<std::pair<std::wstring, size_t>>&) {}

  // Field accessors
  bool         hasWidget(const std::string&) const { return false; }
  std::wstring getText(const std::string&) const    { return L""; }
  int          getTextNo(const std::string&) const  { return 0; }
  void         setText(const std::string&, const std::wstring&) {}
  void         setText(const char*, const std::wstring&) {}
  void         setText(const char*, const wchar_t*) {}
  std::pair<int, bool> getSelectedItem(const std::string&) const { return {0, false}; }
  void         selectItemByData(const std::string&, size_t) {}

  // String helpers (legacy compatibility)
  std::wstring widen(const char* s) const {
    std::wstring r;
    while (*s) r += static_cast<wchar_t>(static_cast<unsigned char>(*s++));
    return r;
  }
  std::wstring widen(const std::string& s) const { return widen(s.c_str()); }

  AskAnswer askOkCancel(const std::wstring&) { return AskAnswer::AnswerOk; }

  // List control methods (used by oCard::fillPunches)
  void clearList(const std::string&) {}
  void addItem(const std::string&, const std::wstring&, int) {}
  void addItem(const std::string&, const std::wstring&) {}
};
