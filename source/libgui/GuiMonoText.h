#pragma once

#include "GuiImage.h"
#include "GuiImageData.h"

typedef struct vconsole vconsole_t;

// by retro100
// The class GuiMonoText is dual licensed for you to choose from.
// public domain. no warranty implied; use at your own risk.
// also licensed under the zlib license.
class GuiMonoText : public GuiImage
{
	public:
		//! Constructor
		//!
		//! The full glyph width is glyphMarginLeft + glyphWidth + glyphMarginRight
		//! The full glyph height is glyphMarginTop + glyphHeight + glyphMarginBottom
		//!
		//!\param glyphCountX Count of glyphs in X direction inside the image/tileset
		//!\param glyphCountY Count of glyphs in Y direction inside the image/tileset
		//!\param firstGlyphAsciiCode e.g. 0 if all 128 glyphs are inside or 32 if the first glyph is a blank
		//!\param glyphWidth width of one glyph in pixels WITHOUT extra margin between glyphs
		//!\param glyphHeight height of one glyph in pixels without margin
		//!\param glyphMarginLeft extra margin at the left side of each glyph (e.g. 0 or 1 px)
		//!\param glyphMarginRight extra margin at the right side of each glyph (e.g. 0 or 1 px)
		//!\param glyphMarginTop extra margin at the top side of each glyph (e.g. 0 or 1 px)
		//!\param glyphMarginBottom extra margin at the bottom side of each glyph (e.g. 0 or 1 px)
		GuiMonoText(GuiImageData * img,
				unsigned int glyphCountX,
				unsigned int glyphCountY,
				unsigned int firstGlyphAsciiCode,
				unsigned int glyphWidth,
				unsigned int glyphHeight,
				unsigned int glyphMarginLeft,
				unsigned int glyphMarginRight,
				unsigned int glyphMarginTop,
				unsigned int glyphMarginBottom);
		~GuiMonoText();
		void SetText(const std::string& text);
		void SetVirtualConsole(vconsole_t* vc, float lineSpace);
		virtual void Draw();
	private:
		unsigned int glyphCountX;
		unsigned int glyphCountY;
		char firstGlyphAsciiCode;
		float glyphWidthF;
		float glyphHeightF;
		float glyphMarginLeftF;
		float glyphMarginTopF;
		float fullGlyphWidthF;
		float fullGlyphHeightF;
		float texFullGlyphWidth;
		float texFullGlyphHeight;
		std::string text;
		vconsole_t* vc;
		float lineSpace;

		void DrawText(float xpos, float ypos, const char* str);
		void DrawVirtualConsole(float xpos, float ypos);
};
