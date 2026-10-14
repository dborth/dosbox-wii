// Force-included into GuiTextRenderer.cpp on a host: the console build registers three FreeType modules by hand
// (and FT_New_Library with no modules); a host FreeType does that itself.
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H
#define FT_New_Library(m, l) FT_Init_FreeType(l)
#define FT_Add_Module(l, c) 0
