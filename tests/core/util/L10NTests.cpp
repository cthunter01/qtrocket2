#include "QtRocket/util/L10N.h"

#include <gtest/gtest.h>

namespace
{

using QtRocket::L10N::normalize;

// Every expected key was produced by OpenRocket's L10N.normalize on JDK 17 (Locale.US).

// ---- QtRocket additions ----

TEST(L10N, NormalizeMakesTranslationKeys)
{
    EXPECT_EQ(normalize("Paper (office)"), "paper_office");
    EXPECT_EQ(normalize("Elastic cord (round 2 mm, 1/16 in)"), "elastic_cord_round_2_mm_1_16_in");
    // Runs of whitespace collapse, but underscores from other characters do not.
    EXPECT_EQ(normalize("PLA - 100% infill"), "pla__100_infill");
    EXPECT_EQ(normalize("tab\tand\nnewline"), "tab_and_newline");
    EXPECT_EQ(normalize("  _a_  "), "a");
    EXPECT_EQ(normalize("\x01 a \x01"), "a");
    EXPECT_EQ(normalize(""), "");
    EXPECT_EQ(normalize("___"), "");
}

TEST(L10N, NormalizeReplacesAccentsAndSymbols)
{
    EXPECT_EQ(normalize("CRÊPE  paper"), "crepe_paper");
    EXPECT_EQ(normalize("½ in"), "1_2_in");                    // one half
    EXPECT_EQ(normalize("café au​lait"), "cafe_au_lait");  // no-break, zero-width
    EXPECT_EQ(normalize("A⁄B"), "a_b");                        // the fraction slash
    EXPECT_EQ(normalize("İstanbul"), "istanbul");
    EXPECT_EQ(normalize("Kelvin"), "kelvin");  // String.toLowerCase of the Kelvin sign
    // Letters outside the map are dropped, but still separate the spaces around them.
    EXPECT_EQ(normalize("a Ω b"), "a__b");
    EXPECT_EQ(normalize("x æ y"), "x__y");
    EXPECT_EQ(normalize("Æther"), "ther");
    EXPECT_EQ(normalize("\U0001F680 rocket"), "rocket");
}

}  // namespace
