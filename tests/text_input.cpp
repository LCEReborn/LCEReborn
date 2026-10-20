#include "platform/pc/PcTextInput.hpp"

#include <cstdio>
#include <cstdlib>

static void Require(bool condition, const char* message)
{
    if (!condition)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

int main()
{
    PcTextComposition composition;
    composition.Update(u8"a\u00e9\U0001f600z", 1, 2);
    Require(composition.text == L"a\u00e9\U0001f600z", "decode multi-byte and supplementary characters");
    Require(composition.cursor == 1 && composition.selection == (sizeof(wchar_t) == 2 ? 3 : 2),
            "translate Unicode selection into native wide units");
    composition.Update(u8"a\u00e9\U0001f600z", 3, 0);
    Require(composition.cursor == (sizeof(wchar_t) == 2 ? 4 : 3) && composition.selection == 0,
            "cursor after a supplementary character");
    composition.Update("abc", -1, -1);
    Require(composition.cursor == 3 && composition.selection == 0, "absent IME selection");
    composition.Update("abc", 1, 1000);
    Require(composition.cursor == 1 && composition.selection == 2, "clamp an overlong selection");
    composition.Update("abc", 1000, 1000);
    Require(composition.cursor == 3 && composition.selection == 0, "clamp an out-of-range cursor");
    composition.Update(nullptr, 0, 0);
    Require(composition.text.empty() && composition.cursor == 0 && composition.selection == 0, "clear preedit");
    std::puts("PASS: Unicode IME preedit, cursor and selection ranges");
}
