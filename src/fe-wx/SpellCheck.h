/* PChat - wxWidgets Frontend
 * Copyright (C) 2025-2026 Zach Bacon
 *
 * Spell checking via Hunspell — provides word checking and suggestions
 * for the input text box.
 */

#ifndef PCHAT_SPELLCHECK_H
#define PCHAT_SPELLCHECK_H

#include <wx/wx.h>
#include <string>
#include <vector>

#ifdef HAVE_HUNSPELL
#include <memory>
/* Forward declare Hunspell to avoid exposing the header */
class Hunspell;
#endif

class SpellChecker
{
public:
    static SpellChecker &Instance();

    /* Initialize with dictionary path and affix path.
       If not called, auto-detects system dictionaries. */
    bool Init(const wxString &lang = wxT("en_US"));

    /* Check if a word is spelled correctly */
    bool CheckWord(const wxString &word) const;

    /* Get suggestions for a misspelled word */
    std::vector<wxString> Suggest(const wxString &word, int maxSuggestions = 8) const;

    /* Add a word to the session dictionary (not persisted) */
    void AddWord(const wxString &word);

    /* Is the spell checker initialized? */
#ifdef HAVE_HUNSPELL
    bool IsReady() const { return m_hunspell != nullptr; }
#else
    bool IsReady() const { return false; }
#endif

    /* Get current language */
    wxString GetLanguage() const { return m_language; }

    /* Get list of available dictionary languages */
    static std::vector<wxString> GetAvailableLanguages();

private:
    SpellChecker();
    ~SpellChecker();
    SpellChecker(const SpellChecker &) = delete;
    SpellChecker &operator=(const SpellChecker &) = delete;

    /* Find dictionary files on the system */
    static wxString FindDictionaryDir();

#ifdef HAVE_HUNSPELL
    std::unique_ptr<Hunspell> m_hunspell;
#endif
    wxString m_language;
    wxString m_encoding;
};

#endif /* PCHAT_SPELLCHECK_H */
