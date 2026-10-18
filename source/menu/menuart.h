/****************************************************************************
 * DOSBox Wii
 *
 * menuart.h
 *
 * Code-drawn bars and buttons
 *
 * A MenuButtonShape is used as a GuiButton's image. The button mirrors its
 * state into the shape, which fades between the resting, highlighted and
 * pressed looks
 *
 *   MenuButtonShape img(MenuButtonShape::LONG);
 *   GuiButton btn(MENU_BTN_LONG_W, MENU_BTN_LONG_H);
 *   btn.setImage(&img);
 ***************************************************************************/
#pragma once
#include <algorithm>
#include <math.h>
#include "menustyle.h"

//!A button face. LONG, LARGE, NORMAL, SHORT and SMALL match the sizes of the buttons
//!they replace (see the MENU_BTN_* sizes); NORMAL is the standard 196x52 button and
//!the one used in dialogs.
class MenuButtonShape : public GuiShape
{
	public:
		enum Kind { LONG, LARGE, NORMAL, SHORT, SMALL };

		MenuButtonShape(Kind kind) :
			GuiShape(widthOf(kind), heightOf(kind))
		{
			setCorners(GuiCorners(radiusOf(kind)));
			setStyle(MenuButtonStyle());
		}

	private:
		static int widthOf(Kind k)
		{
			switch(k)
			{
				case LONG: return MENU_BTN_LONG_W;
				case LARGE: return MENU_BTN_LARGE_W;
				case SHORT: return MENU_BTN_SHORT_W;
				case SMALL: return MENU_BTN_SMALL_W;
				default: return MENU_BTN_NORMAL_W;
			}
		}
		static int heightOf(Kind k)
		{
			switch(k)
			{
				case LONG: return MENU_BTN_LONG_H;
				case LARGE: return MENU_BTN_LARGE_H;
				case SHORT: return MENU_BTN_SHORT_H;
				case SMALL: return MENU_BTN_SMALL_H;
				default: return MENU_BTN_NORMAL_H;
			}
		}
		static float radiusOf(Kind k)
		{
			switch(k)
			{
				case LONG: return MENU_BTN_LONG_RADIUS;
				case LARGE: return MENU_BTN_LARGE_RADIUS;
				case SHORT: return MENU_BTN_SHORT_RADIUS;
				case SMALL: return MENU_BTN_SMALL_RADIUS;
				default: return MENU_BTN_NORMAL_RADIUS;
			}
		}
};

//!A small square key face (the +/- buttons): light and outlined at rest, accent
//!outline and glow when the pointer is on it, deeper while pressed. Matches the
//!keyboard's keys.
class MenuKeyShape : public GuiShape
{
	public:
		MenuKeyShape() : GuiShape(40, 40)
		{
			const GuiTheme& theme = GuiTheme::current();
			PixelColor glow = theme.accent;
			glow.a = 110;

			setCorners(GuiCorners(10.0f));
			setStyle(GuiShapeStyle(GuiPaint().gradient(theme.surfaceRaised, theme.surface).stroke(1.5f, theme.outlineStrong).shadow(4.0f, theme.shadow, 0.0f, 1.0f))
				.onFocus(GuiPaint().gradient(theme.surfaceRaised, theme.highlight).stroke(2.5f, theme.accent).glow(10.0f, glow, 1.0f))
				.onPress(GuiPaint().gradient(theme.highlight, theme.outline).stroke(2.0f, theme.accentDark)));
		}
};

//!A page-turning arrow: gray, turning to the accent color while its button is
//!highlighted. Use it as the image of a GuiButton of MENU_ARROW_* size; it centers
//!itself in the button.
class MenuArrowImage : public GuiArrow
{
	public:
		MenuArrowImage(ARROW dir) :
			GuiArrow(dir,
				(dir == ARROW::LEFT || dir == ARROW::RIGHT) ? MENU_ARROW_SHORT : MENU_ARROW_LONG,
				(dir == ARROW::LEFT || dir == ARROW::RIGHT) ? MENU_ARROW_LONG : MENU_ARROW_SHORT,
				MenuArrowRest(), MenuArrowLit())
		{
			setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
		}
};

class MenuIconData : public GuiImageData
{
	public:
		MenuIconData(const uint8_t * png, float offsetX = 0.0f, float offsetY = 0.0f)
		{
			loadClipped(png, MENU_BTN_LARGE_W, MENU_BTN_LARGE_H, MENU_BTN_LARGE_RADIUS, MENU_BTN_ICON_INSET, offsetX, offsetY);
		}
};

//!The header (top) or footer (bottom) bar
class MenuBarShape : public GuiShape
{
	public:
		enum Edge { TOP, BOTTOM };

		MenuBarShape(Edge edge) :
			GuiShape(platform->getVideo()->getScreenWidth() + 2 * MENU_BAR_BLEED,
					(edge == TOP ? MENU_BAR_TOP_H : MENU_BAR_BOTTOM_H) + MENU_BAR_BLEED)
		{
			setPaint(edge == TOP ? MenuBarTopPaint() : MenuBarBottomPaint());
			if(edge == TOP)
			{
				setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
				setPosition(-MENU_BAR_BLEED, -MENU_BAR_BLEED);
			}
			else
			{
				setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
				setPosition(-MENU_BAR_BLEED, MENU_BAR_BLEED);
			}
		}
};

//!The page behind everything: a vertical gradient over the whole screen
class MenuBackgroundShape : public GuiShape
{
	public:
		MenuBackgroundShape() :
			GuiShape(platform->getVideo()->getScreenWidth() + 2 * MENU_BAR_BLEED,
					platform->getVideo()->getScreenHeight() + 2 * MENU_BAR_BLEED)
		{
			setPaint(MenuBackgroundPaint());
			setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
			setPosition(-MENU_BAR_BLEED, -MENU_BAR_BLEED);
		}
};

//!A card: the rounded, softly shadowed panel the game list and its preview sit on
class MenuCardShape : public GuiShape
{
	public:
		MenuCardShape(int w, int h) : GuiShape(w, h)
		{
			setCorners(GuiCorners(GuiTheme::current().radius));
			setStyle(GuiTheme::current().cardStyle());
		}
};

//!A soft shadow cast by the preview image onto its card. Draw it just behind the
//!image and call fit() with the image's displayed size.
class MenuCoverShadow : public GuiShape
{
	public:
		MenuCoverShadow()
		{
			setCorners(GuiCorners(3.0f));
			setPaint(GuiPaint().shadow(14.0f, MenuRGBA(0, 0, 0, 110), 0.0f, 4.0f));
			setVisible(false);
		}

		//!Sizes the shadow to the image, or hides it when there is no image (w or h is 0)
		void fit(int w, int h)
		{
			setVisible(w > 0 && h > 0);
			setSize(w, h);
		}
};

//!The window behind a dialog or the credits: a card with a chrome title band across
//!the top. Append it first, and put the (white) title text on the band.
class MenuDialogShape : public GuiElement
{
	public:
		MenuDialogShape(int w, int h, int headerHeight) : body(w, h), header(w, headerHeight)
		{
			width = w;
			height = h;
			body.setParent(this);
			body.setCorners(GuiCorners(MENU_DIALOG_RADIUS));
			body.setPaint(MenuDialogBodyPaint());
			header.setParent(this);
			header.setCorners(GuiCorners(MENU_DIALOG_RADIUS, MENU_DIALOG_RADIUS, 0.0f, 0.0f));
			header.setPaint(MenuDialogHeaderPaint());
		}

		void draw() override
		{
			if(!this->isVisible())
				return;
			body.draw();
			header.draw();
			this->updateEffects();
		}

	private:
		GuiShape body;
		GuiShape header;
};

//!An element that is one mesh, built in its own coordinates (0,0 is its top-left) and drawn where
//!it is, faded with it. Sub-classes only say how to build the mesh: build() is called with `mesh`
//!empty and ready for the video driver's edge feathering, and again when that changes or after
//!invalidate() (the look changed).
class MenuMeshElement : public GuiElement
{
	public:
		MenuMeshElement(int w, int h) : meshFeather(-1.0f)
		{
			width = w;
			height = h;
		}

		void draw() override
		{
			if(!this->isVisible())
				return;
			const int a = this->getAlpha();
			if(a > 0)
			{
				VideoDriver * video = platform->getVideo();
				const float uiScale = video->getUIScale();
				const float feather = uiScale > 0.0f ? 1.0f / uiScale : 1.0f;
				if(feather != meshFeather)
				{
					mesh = GuiMesh(feather);
					build();
					meshFeather = feather;
				}
				const float ox = (float)this->getLeft(), oy = (float)this->getTop();
				const std::vector<MeshVertex>& src = mesh.vertices();
				scratch.resize(src.size());
				for(size_t i = 0; i < src.size(); i++)
				{
					scratch[i] = src[i];
					scratch[i].x += ox;
					scratch[i].y += oy;
					if(a < 255)
						scratch[i].c.a = (uint8_t)((src[i].c.a * a + 127) / 255);
				}
				if(!scratch.empty())
					video->getImageRenderer()->drawMesh(scratch.data(), scratch.size());
			}
			this->updateEffects();
		}

		//!Has build() run again at the next draw
		void invalidate() { meshFeather = -1.0f; }

	protected:
		virtual void build() = 0;

		GuiMesh mesh;

	private:
		float meshFeather;
		std::vector<MeshVertex> scratch;
};

//!The window behind the on-screen keyboard: a card with a chrome band holding the text
//!field. Insert it as the keyboard's first element: keyboard.insert(&box, 0). It is
//!wider than the keyboard itself so the keys keep a margin from the card's edge.
class MenuKeyboardBox : public MenuDialogShape
{
	public:
		MenuKeyboardBox() : MenuDialogShape(MENU_KEYBOARD_W, MENU_KEYBOARD_H, MENU_KEYBOARD_HEADER_H)
		{
			setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
		}
};

//!A gray rounded tile standing in for the screen in the zoom and position windows
class MenuScreenShape : public GuiShape
{
	public:
		MenuScreenShape() : GuiShape(MENU_SCREEN_W, MENU_SCREEN_H)
		{
			const GuiTheme& theme = GuiTheme::current();
			setCorners(GuiCorners(12.0f));
			setPaint(GuiPaint().gradient(theme.outline, theme.outlineStrong).stroke(1.5f, theme.outlineStrong));
		}
};
