/* PChat - wxWidgets Frontend
 * Copyright (C) 2025-2026 Zach Bacon
 *
 * Spell checking via Hunspell
 */

#include "SpellCheck.h"

#ifdef HAVE_HUNSPELL
#include <hunspell.hxx>
#endif

#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <algorithm>

SpellChecker::SpellChecker()
{
}

SpellChecker::~SpellChecker()
{
    /* unique_ptr handles deletion */
}

SpellChecker &SpellChecker::Instance()
{
    static SpellChecker instance;
    return instance;
}

wxString SpellChecker::FindDictionaryDir()
{
    /* Search common locations for Hunspell dictionaries */
    wxArrayString searchPaths;

#ifdef __WXMSW__
    /* MSYS2 / MinGW paths */
    wxString exeDir = wxPathOnly(wxStandardPaths::Get().GetExecutablePath());
    searchPaths.Add(exeDir + wxT("/../share/hunspell"));
    searchPaths.Add(exeDir + wxT("/share/hunspell"));

    /* Standard MSYS2 locations */
    searchPaths.Add(wxT("C:/msys64/clang64/share/hunspell"));
    searchPaths.Add(wxT("C:/msys64/mingw64/share/hunspell"));
    searchPaths.Add(wxT("C:/msys64/ucrt64/share/hunspell"));

    /* Use MSYSTEM_PREFIX env var if available */
    wxString msysPrefix;
    if (wxGetEnv(wxT("MSYSTEM_PREFIX"), &msysPrefix)) {
        searchPaths.Insert(msysPrefix + wxT("/share/hunspell"), 0);
    }
#else
    searchPaths.Add(wxT("/usr/share/hunspell"));
    searchPaths.Add(wxT("/usr/local/share/hunspell"));
    searchPaths.Add(wxT("/usr/share/myspell"));
    searchPaths.Add(wxT("/usr/share/myspell/dicts"));
#endif

    for (const auto &path : searchPaths) {
        if (wxDir::Exists(path))
            return path;
    }

    return wxEmptyString;
}

std::vector<wxString> SpellChecker::GetAvailableLanguages()
{
    std::vector<wxString> langs;
    wxString dir = FindDictionaryDir();
    if (dir.IsEmpty()) return langs;

    wxDir d(dir);
    if (!d.IsOpened()) return langs;

    wxString filename;
    bool cont = d.GetFirst(&filename, wxT("*.dic"), wxDIR_FILES);
    while (cont) {
        /* Extract language code from "en_US.dic" -> "en_US" */
        wxString lang = filename.BeforeLast('.');
        /* Skip "-large" variants for the main list */
        if (!lang.Contains(wxT("-large")) && !lang.Contains(wxT("-huge"))) {
            langs.push_back(lang);
        }
        cont = d.GetNext(&filename);
    }

    std::sort(langs.begin(), langs.end());
    return langs;
}

bool SpellChecker::Init(const wxString &lang)
{
#ifndef HAVE_HUNSPELL
    wxLogDebug(wxT("SpellChecker: Hunspell not available (not compiled in)"));
    return false;
#else
    wxString dictDir = FindDictionaryDir();
    if (dictDir.IsEmpty()) {
        wxLogDebug(wxT("SpellChecker: No dictionary directory found"));
        return false;
    }

    wxString affPath = dictDir + wxFILE_SEP_PATH + lang + wxT(".aff");
    wxString dicPath = dictDir + wxFILE_SEP_PATH + lang + wxT(".dic");

    if (!wxFileExists(affPath) || !wxFileExists(dicPath)) {
        /* Try with -large suffix */
        affPath = dictDir + wxFILE_SEP_PATH + lang + wxT("-large.aff");
        dicPath = dictDir + wxFILE_SEP_PATH + lang + wxT("-large.dic");
        if (!wxFileExists(affPath) || !wxFileExists(dicPath)) {
            wxLogDebug(wxT("SpellChecker: Dictionary '%s' not found in %s"),
                       lang, dictDir);
            return false;
        }
    }

    try {
        m_hunspell = std::make_unique<Hunspell>(
            affPath.utf8_str().data(),
            dicPath.utf8_str().data()
        );
    } catch (...) {
        wxLogDebug(wxT("SpellChecker: Failed to load Hunspell"));
        m_hunspell.reset();
        return false;
    }

    m_language = lang;
    m_encoding = wxString::FromUTF8(m_hunspell->get_dict_encoding().c_str());

    wxLogDebug(wxT("SpellChecker: Loaded '%s' dictionary (encoding: %s)"),
               lang, m_encoding);
    return true;
#endif
}

bool SpellChecker::CheckWord(const wxString &word) const
{
#ifndef HAVE_HUNSPELL
    return true;
#else
    if (!m_hunspell) return true; /* No spellchecker = everything OK */
    if (word.IsEmpty()) return true;

    /* Skip words that look like IRC artifacts: nicks, URLs, commands */
    if (word.StartsWith(wxT("/"))) return true;
    if (word.StartsWith(wxT("#"))) return true;
    if (word.StartsWith(wxT("@"))) return true;
    if (word.StartsWith(wxT("+"))) return true;
    if (word.Contains(wxT("://"))) return true;
    if (word.Contains(wxT("."))) return true; /* hostnames, domains */

    /* Skip words that are all uppercase (acronyms) */
    bool allUpper = true;
    for (size_t i = 0; i < word.Length(); i++) {
        if (wxIslower(word[i])) { allUpper = false; break; }
    }
    if (allUpper && word.Length() > 1) return true;

    /* Skip words with digits */
    for (size_t i = 0; i < word.Length(); i++) {
        if (wxIsdigit(word[i])) return true;
    }

    return m_hunspell->spell(std::string(word.utf8_str()));
#endif
}

std::vector<wxString> SpellChecker::Suggest(const wxString &word,
                                             int maxSuggestions) const
{
    std::vector<wxString> result;
#ifdef HAVE_HUNSPELL
    if (!m_hunspell || word.IsEmpty()) return result;

    auto suggestions = m_hunspell->suggest(std::string(word.utf8_str()));

    int count = 0;
    for (const auto &s : suggestions) {
        if (count >= maxSuggestions) break;
        result.push_back(wxString::FromUTF8(s.c_str()));
        count++;
    }
#endif
    return result;
}

void SpellChecker::AddWord(const wxString &word)
{
#ifdef HAVE_HUNSPELL
    if (!m_hunspell || word.IsEmpty()) return;
    m_hunspell->add(std::string(word.utf8_str()));
#endif
}
