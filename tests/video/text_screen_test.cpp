#include "support/check.hpp"
#include "video/text_screen.hpp"

namespace {

void test_newline_follows_linked_eighty_column_line() {
    weatherwar::TextScreen screen;
    screen.newline();
    for (int i = 0; i < 80; ++i) screen.put(32);
    screen.row = 1;
    screen.column = 5;
    screen.newline();
    test_support::check(screen.row == 3 && screen.column == 0);
}

void test_newline_advances_one_unlinked_row() {
    weatherwar::TextScreen screen;
    screen.clear();
    screen.row = 1;
    screen.newline();
    test_support::check(screen.row == 2 && screen.column == 0);
}

} // namespace

int main() {
    test_newline_follows_linked_eighty_column_line();
    test_newline_advances_one_unlinked_row();
}
