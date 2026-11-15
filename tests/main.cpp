#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cstdlib>

// The tests are written in English: the module's own text is what they compare
// (a row's name, a help line, a drawn caption), and Tr() leaves English as it
// is when the language has no table. ReadLanguage() now falls back to the
// system's language when the game folder has no language.txt, so a Chinese
// Windows would translate every one of those strings and the asserts would
// read "房主" where they expect "Host". HOS_LANGUAGE is the first thing
// ReadLanguage() looks at, so pinning it here keeps the module's text English
// on any machine -- the i18n tests set the language they want by passing one
// to Tr(lang, ...) themselves.
namespace {
struct EnglishTests {
    EnglishTests() {
#ifdef _WIN32
        _putenv_s("HOS_LANGUAGE", "en");
#else
        setenv("HOS_LANGUAGE", "en", 0);   // (not overriding a language a test set)
#endif
    }
} g_english_tests;
}  // namespace
