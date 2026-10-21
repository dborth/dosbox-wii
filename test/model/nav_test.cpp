// Host test of GuiNavigate (D-pad selection movement) on the key picker's real geometry (controlsui.cpp keyPane)
// and a few ordinary layouts. Every key must be reachable, and sideways must stay in the row.
#include "libgui/GuiLayoutSolver.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int checks = 0, fails = 0;
struct Grid { std::vector<GuiNavItem> items; std::vector<std::string> names; };

static void add(Grid& g, const char* n, float x, float y, float w, float h) { g.items.push_back({ { x, y, w, h }, true }); g.names.push_back(n); }
static int idx(const Grid& g, const char* n) { for(size_t i = 0; i < g.names.size(); i++) if(g.names[i] == n) return (int)i; return -1; }
static void expect(const Grid& g, const char* from, NAV_DIR d, const char* want)
{
	checks++;
	const int n = GuiNavigate(g.items, idx(g, from), d);
	const char* got = n >= 0 ? g.names[(size_t)n].c_str() : "(stay)";
	if(strcmp(got, want) != 0) { fails++; printf("FAIL: %s dir %d -> %s, wanted %s\n", from, (int)d, got, want); }
}

struct Row { std::vector<const char*> keys; int indent; };
struct Tab { const char* label; int width, capW, capH; std::vector<Row> rows; };

// the same table as keyPane(); tab row 22 high, 2 gap; body starts 4 below it
static const std::vector<Tab> tabs = {
	{ "Common", 54, 58, 32, { { { "Enter", "Esc", "Space", "Tab" }, 0 }, { { "Backspace", "Ctrl", "Alt", "Shift" }, 0 }, { { "Up", "Down", "Left", "Right" }, 0 } } },
	{ "A-Z", 30, 22, 30, { { { "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P" }, 0 }, { { "A", "S", "D", "F", "G", "H", "J", "K", "L" }, 11 }, { { "Z", "X", "C", "V", "B", "N", "M" }, 23 } } },
	{ "0-9", 30, 22, 30, { { { "1", "2", "3", "4", "5", "6", "7", "8", "9", "0" }, 0 } } },
	{ "F-keys", 42, 36, 30, { { { "F1", "F2", "F3", "F4", "F5", "F6" }, 0 }, { { "F7", "F8", "F9", "F10", "F11", "F12" }, 0 } } },
	{ "Edit", 32, 72, 32, { { { "PgUp", "PgDn", "Home" }, 0 }, { { "End", "Ins", "Del" }, 0 } } },
	{ "Symbols", 46, 36, 30, { { { "Minus", "Equals", "Comma", "Period", "Slash", "Semicolon" }, 0 }, { { "Quote", "[", "]", "Backslash", "Grave" }, 0 } } },
};

static Grid pane(size_t shown)
{
	Grid g;
	float x = 0;
	for(size_t t = 0; t < tabs.size(); t++) { add(g, tabs[t].label, x, 0, (float)tabs[t].width, 22); x += tabs[t].width + 2; }
	float y = 26;
	const Tab& tb = tabs[shown];
	for(size_t r = 0; r < tb.rows.size(); r++)
	{
		for(size_t k = 0; k < tb.rows[r].keys.size(); k++)
			add(g, tb.rows[r].keys[k], (float)(tb.rows[r].indent + (int)k * (tb.capW + 2)), y, (float)tb.capW, (float)tb.capH);
		y += tb.capH + 2;
	}
	return g;
}

int main()
{
	// Common: the reported bug. Right from a tab goes to the next tab, not the key under it.
	{ Grid g = pane(0);
	  expect(g, "Common", NAV_DIR::RIGHT, "A-Z"); expect(g, "A-Z", NAV_DIR::LEFT, "Common");
	  expect(g, "Common", NAV_DIR::DOWN, "Enter"); expect(g, "Enter", NAV_DIR::RIGHT, "Esc"); expect(g, "Esc", NAV_DIR::RIGHT, "Space");
	  expect(g, "Space", NAV_DIR::LEFT, "Esc"); expect(g, "Enter", NAV_DIR::DOWN, "Backspace"); expect(g, "Backspace", NAV_DIR::UP, "Enter");
	  expect(g, "Ctrl", NAV_DIR::RIGHT, "Alt"); expect(g, "Left", NAV_DIR::RIGHT, "Right"); expect(g, "Left", NAV_DIR::UP, "Alt");
	  expect(g, "Enter", NAV_DIR::UP, "Common"); expect(g, "Up", NAV_DIR::DOWN, "(stay)"); }

	// A-Z: staggered rows. Sideways stays in the row; up and down pick the key whose cap overlaps.
	{ Grid g = pane(1);
	  expect(g, "Q", NAV_DIR::RIGHT, "W"); expect(g, "W", NAV_DIR::RIGHT, "E"); expect(g, "A", NAV_DIR::RIGHT, "S"); expect(g, "Z", NAV_DIR::RIGHT, "X");
	  expect(g, "P", NAV_DIR::LEFT, "O"); expect(g, "L", NAV_DIR::LEFT, "K"); expect(g, "M", NAV_DIR::LEFT, "N");
	  expect(g, "Q", NAV_DIR::DOWN, "A"); expect(g, "A", NAV_DIR::DOWN, "Z"); expect(g, "Z", NAV_DIR::UP, "A"); expect(g, "A", NAV_DIR::UP, "Q");
	  expect(g, "Q", NAV_DIR::UP, "Common"); expect(g, "M", NAV_DIR::DOWN, "(stay)");
	  // off the end of a row continues on the next row
	  expect(g, "P", NAV_DIR::RIGHT, "A"); expect(g, "A", NAV_DIR::LEFT, "P"); }

	// Other tabs: first tab to its first key, rows in order
	{ Grid g = pane(2); expect(g, "0-9", NAV_DIR::DOWN, "5"); expect(g, "1", NAV_DIR::RIGHT, "2"); expect(g, "9", NAV_DIR::RIGHT, "0"); }
	{ Grid g = pane(3); expect(g, "F1", NAV_DIR::RIGHT, "F2"); expect(g, "F1", NAV_DIR::DOWN, "F7"); expect(g, "F12", NAV_DIR::UP, "F6"); expect(g, "F-keys", NAV_DIR::RIGHT, "Edit"); }
	{ Grid g = pane(4); expect(g, "PgUp", NAV_DIR::RIGHT, "PgDn"); expect(g, "PgUp", NAV_DIR::DOWN, "End"); expect(g, "Del", NAV_DIR::UP, "Home"); }
	{ Grid g = pane(5); expect(g, "Minus", NAV_DIR::DOWN, "Quote"); expect(g, "Quote", NAV_DIR::RIGHT, "["); expect(g, "Slash", NAV_DIR::DOWN, "Grave"); expect(g, "Symbols", NAV_DIR::LEFT, "Edit"); }

	// Ordinary layouts keep working: a list, a 2x2 grid, and a row of buttons
	{ Grid g; add(g, "r1", 0, 0, 200, 28); add(g, "r2", 0, 30, 200, 28); add(g, "r3", 0, 60, 200, 28);
	  expect(g, "r1", NAV_DIR::DOWN, "r2"); expect(g, "r3", NAV_DIR::UP, "r2"); expect(g, "r1", NAV_DIR::UP, "(stay)"); expect(g, "r1", NAV_DIR::RIGHT, "r2"); }
	{ Grid g; add(g, "a", 0, 0, 100, 100); add(g, "b", 110, 0, 100, 100); add(g, "c", 0, 110, 100, 100); add(g, "d", 110, 110, 100, 100);
	  expect(g, "a", NAV_DIR::RIGHT, "b"); expect(g, "a", NAV_DIR::DOWN, "c"); expect(g, "d", NAV_DIR::LEFT, "c"); expect(g, "d", NAV_DIR::UP, "b"); expect(g, "b", NAV_DIR::RIGHT, "c"); }
	// a short item beside a tall one: sideways still finds it
	{ Grid g; add(g, "tall", 0, 0, 100, 200); add(g, "s1", 110, 0, 100, 40); add(g, "s2", 110, 100, 100, 40);
	  expect(g, "tall", NAV_DIR::RIGHT, "s2"); expect(g, "s2", NAV_DIR::LEFT, "tall"); }
	// partly overlapping neighbours (a popover over a card): nothing wholly beyond, vertical falls back to the loose test
	{ Grid g; add(g, "x", 0, 0, 100, 50); add(g, "y", 20, 40, 100, 50); expect(g, "x", NAV_DIR::DOWN, "y"); }

	printf("%d checks, %d failed\n", checks, fails);
	return fails ? 1 : 0;
}
