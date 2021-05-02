// by retro100
// This file is dual licensed for you to choose from.
// public domain. no warranty implied; use at your own risk.
// also licensed under the zlib license.

#include "gui.h"
#include "../vconsole.h"

GuiMonoText::GuiMonoText(GuiImageData * img,
	unsigned int glyphCountX,
	unsigned int glyphCountY,
	unsigned int firstGlyphAsciiCode,
	unsigned int glyphWidth,
	unsigned int glyphHeight,
	unsigned int glyphMarginLeft,
	unsigned int glyphMarginRight,
	unsigned int glyphMarginTop,
	unsigned int glyphMarginBottom)
	:GuiImage(img),
	glyphCountX(glyphCountX),
	glyphCountY(glyphCountY),
	firstGlyphAsciiCode(firstGlyphAsciiCode),
	text(),
	vc(NULL),
	lineSpace(0.0f)
{
	unsigned int fullGlyphWidth = glyphMarginLeft + glyphWidth + glyphMarginRight;
	unsigned int fullGlyphHeight = glyphMarginTop + glyphHeight + glyphMarginBottom;
	unsigned int tilesetWidth = fullGlyphWidth * glyphCountX;
	unsigned int tilesetHeight = fullGlyphHeight * glyphCountY;
	// Normally tilesetWidth should have the same value as the image width and
	// tilesetHeight should have the same value as the image height.
	// Only if the tileset image has a extra border (free space) at the right side
	// and/or at the bottom then the values are different. (which is also no problem)

	float texTilesetWidth = (float)tilesetWidth / (float)width;
	float texTilesetHeight = (float)tilesetHeight / (float)height;
	texFullGlyphWidth = texTilesetWidth / (float) glyphCountX;
	texFullGlyphHeight = texTilesetHeight / (float) glyphCountY;
	glyphWidthF = glyphWidth;
	glyphHeightF = glyphHeight;
	glyphMarginLeftF = glyphMarginLeft;
	glyphMarginTopF = glyphMarginTop;
	fullGlyphWidthF = fullGlyphWidth;
	fullGlyphHeightF = fullGlyphHeight;
}

GuiMonoText::~GuiMonoText()
{
}

void GuiMonoText::SetText(const std::string& text)
{
	this->text = text;
}

void GuiMonoText::SetVirtualConsole(vconsole_t* vc, float lineSpace)
{
	this->vc = vc;
	this->lineSpace = lineSpace;
}

void GuiMonoText::Draw()
{
	if(!image || !this->IsVisible() || (text.empty() && !vc))
		return;

	f32 xpos = this->GetLeft();
	f32 ypos = this->GetTop();
	u8* data = image;
	//u8 alpha = this->GetAlpha();

	GXTexObj texObj;

	GX_InitTexObj(&texObj, data, width,height, GX_TF_RGBA8,GX_CLAMP, GX_CLAMP,GX_FALSE);
	GX_LoadTexObj(&texObj, GX_TEXMAP0);
	GX_InvalidateTexAll();

	GX_SetTevOp (GX_TEVSTAGE0, GX_MODULATE);
	GX_SetVtxDesc (GX_VA_TEX0, GX_DIRECT);

	if (vc) {
		DrawVirtualConsole(xpos, ypos);
	}
	else {
		DrawText(xpos, ypos, text.c_str());
	}

	GX_SetTevOp (GX_TEVSTAGE0, GX_PASSCLR);
	GX_SetVtxDesc (GX_VA_TEX0, GX_NONE);
}

void GuiMonoText::DrawText(float xpos, float ypos, const char* str)
{
	size_t count = strlen(str);

	xpos -= glyphMarginLeftF;
	ypos -= glyphMarginTopF;

	GX_Begin(GX_QUADS, GX_VTXFMT0, 4 * count);

	for (size_t i = 0; i < count; ++i, xpos += glyphWidthF) {
		unsigned char ch = (unsigned char)str[i];
		if (ch < firstGlyphAsciiCode || ch >= 128) {
			ch = '?';
		}
		unsigned int glyphIndex = ch - firstGlyphAsciiCode;
		unsigned int xi = glyphIndex % glyphCountX;
		unsigned int yi = glyphIndex / glyphCountX;
		float texX = float(xi) * texFullGlyphWidth;
		float texY = float(yi) * texFullGlyphHeight;

		// left top
		GX_Position3f32(xpos, ypos,  0);
		GX_Color4u8(0x00, 0x00, 0x00, alpha);
		GX_TexCoord2f32(texX, texY);

		// right top
		GX_Position3f32(xpos + fullGlyphWidthF, ypos,  0);
		GX_Color4u8(0x00, 0x00, 0x00, alpha);
		GX_TexCoord2f32(texX + texFullGlyphWidth, texY);

		// right bottom
		GX_Position3f32(xpos + fullGlyphWidthF, ypos + fullGlyphHeightF,  0);
		GX_Color4u8(0x00, 0x00, 0x00, alpha);
		GX_TexCoord2f32(texX + texFullGlyphWidth, texY + texFullGlyphHeight);

		// left bottom
		GX_Position3f32(xpos, ypos + fullGlyphHeightF,  0);
		GX_Color4u8(0x00, 0x00, 0x00, alpha);
		GX_TexCoord2f32(texX, texY + texFullGlyphHeight);
	}

	GX_End();
}

void GuiMonoText::DrawVirtualConsole(float xpos, float ypos)
{
	int current_line_count = vc->line_count;
	int current_line_index = vc->vc_current_line_index;

	if (current_line_count < VCONSOLE_HEIGHT) {
		for (int i = 0; i <= current_line_index; ++i, ypos += glyphHeightF + lineSpace) {
			DrawText(xpos, ypos, vc->vc_text_lines[i]);
		}
	}
	else {
		for (int i = current_line_index + 1; i < VCONSOLE_HEIGHT; ++i, ypos += glyphHeightF + lineSpace) {
			DrawText(xpos, ypos, vc->vc_text_lines[i]);
		}
		for (int i = 0; i <= current_line_index; ++i, ypos += glyphHeightF + lineSpace) {
			DrawText(xpos, ypos, vc->vc_text_lines[i]);
		}
	}
}

