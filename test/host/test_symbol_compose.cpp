// Host-side test: g++ -std=c++17 -Itest/host/stub -Iinclude test/host/test_symbol_compose.cpp && ./a.out
#include <cstdio>
#include <cstring>
#include "aprs_symbol_compose.h"

static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

int main() {
    uint16_t a[256], b[256], dot[256];
    memcpy(dot, APRS_ICONS_PRIMARY['/' - 33], sizeof dot);

    CHECK(AprsSymbolCompose::compose('/', '>', a));                       // car: primary, untouched
    CHECK(memcmp(a, APRS_ICONS_PRIMARY['>' - 33], sizeof a) == 0);
    CHECK(AprsSymbolCompose::compose('\\', '#', a));                      // digi star, no overlay
    CHECK(memcmp(a, APRS_ICONS_ALT['#' - 33], sizeof a) == 0);
    CHECK(AprsSymbolCompose::compose('D', '#', b));                       // digi star + overlay D
    CHECK(memcmp(a, b, sizeof a) != 0);
    CHECK(AprsSymbolCompose::compose('a', '#', a));                       // compressed 'a' == overlay 0
    CHECK(AprsSymbolCompose::compose('0', '#', b));
    CHECK(memcmp(a, b, sizeof a) == 0);
    CHECK(AprsSymbolCompose::compose('j', '#', a) && AprsSymbolCompose::compose('9', '#', b) && !memcmp(a, b, sizeof a));
    CHECK(!AprsSymbolCompose::compose('/', ' ', a) && !memcmp(a, dot, sizeof a));   // invalid code -> dot
    CHECK(!AprsSymbolCompose::compose('!', '>', a) && !memcmp(a, dot, sizeof a));   // invalid table -> dot
    // every valid symbol of both tables composes
    for (int c = 33; c <= 126; c++) {
        CHECK(AprsSymbolCompose::compose('/', (char)c, a));
        CHECK(AprsSymbolCompose::compose('\\', (char)c, a));
        CHECK(AprsSymbolCompose::compose('Z', (char)c, a));
    }
    printf(fails ? "%d FAILURES\n" : "all symbol compose tests passed\n", fails);
    return fails != 0;
}
