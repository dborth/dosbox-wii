/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * menuicon.cpp
 *
 * See menuicon.h.
 ***************************************************************************/
#include "menuicon.h"
#include <math.h>

void MenuIcon::line(float x1, float y1, float x2, float y2, float w)
{
	mesh.addLine(P(x1, y1), P(x2, y2), w * u, fill);
	// round the ends, so joined lines have no notch at the corner
	mesh.addEllipse(GuiRect{ (x1 - w / 2) * u, (y1 - w / 2) * u, w * u, w * u }, fill);
	mesh.addEllipse(GuiRect{ (x2 - w / 2) * u, (y2 - w / 2) * u, w * u, w * u }, fill);
}

void MenuIcon::box(float x, float y, float w, float h, float r, float lw)
{
	mesh.strokeRoundRect(GuiRect{ x * u, y * u, w * u, h * u }, GuiCorners(r * u), lw * u, STROKE::INSIDE, fill);
}

void MenuIcon::ring(float cx, float cy, float r, float lw)
{
	mesh.strokeEllipse(GuiRect{ (cx - r) * u, (cy - r) * u, 2 * r * u, 2 * r * u }, lw * u, STROKE::CENTRE, fill);
}

void MenuIcon::dot(float cx, float cy, float r)
{
	mesh.addEllipse(GuiRect{ (cx - r) * u, (cy - r) * u, 2 * r * u, 2 * r * u }, fill);
}

void MenuIcon::build()
{

	u = (float)width / 24.0f;
	fill = GuiFill::solid(color);
	switch(kind)
	{
		case MENU_ICON_GAUGE:
			mesh.strokeArc(P(12, 13), 9 * u, 150.0f, 240.0f, 2.2f * u, color);
			line(12, 13, 16.5f, 8.5f);
			dot(12, 13, 1.8f);
			break;
		case MENU_ICON_FRAMES:
			// a strip of film: the frame in the middle, a column of sprocket holes down each side. The rails stop inside
			// the outline (not past it) and the holes sit clear of both the outline and the rails.
			box(2, 4, 20, 16, 2.5f);
			line(8.8f, 5.5f, 8.8f, 18.5f, 1.6f);
			line(15.2f, 5.5f, 15.2f, 18.5f, 1.6f);
			for(int k = 0; k < 3; k++)
			{
				dot(5.9f, 8.5f + 3.5f * k, 1.0f);
				dot(18.1f, 8.5f + 3.5f * k, 1.0f);
			}
			break;
		case MENU_ICON_ASPECT:
			box(3, 4, 18, 13, 2.5f);
			line(12, 17, 12, 20.5f);
			line(8, 20.5f, 16, 20.5f);
			break;
		case MENU_ICON_FILTER:
			box(3.5f, 3.5f, 7.5f, 7.5f, 1.5f);
			box(13, 13, 7.5f, 7.5f, 1.5f);
			mesh.addRoundRect(GuiRect{ 13 * u, 3.5f * u, 7.5f * u, 7.5f * u }, GuiCorners(1.5f * u), fill);
			mesh.addRoundRect(GuiRect{ 3.5f * u, 13 * u, 7.5f * u, 7.5f * u }, GuiCorners(1.5f * u), fill);
			break;
		case MENU_ICON_KEYBOARD:
			box(2, 6, 20, 12, 2.5f);
			dot(6.5f, 10, 1.0f); dot(10.5f, 10, 1.0f); dot(14.5f, 10, 1.0f); dot(18, 10, 1.0f);
			line(7, 14.2f, 17, 14.2f, 1.8f);
			break;
		case MENU_ICON_GEAR:
			ring(12, 12, 6.2f);
			ring(12, 12, 1.8f);
			for(int k = 0; k < 8; k++)
			{
				const float t = (float)k * 3.14159265f / 4.0f;
				line(12 + 7.4f * cosf(t), 12 + 7.4f * sinf(t), 12 + 10 * cosf(t), 12 + 10 * sinf(t), 3.0f);
			}
			break;
		case MENU_ICON_PULSE:
			line(2.5f, 12, 7, 12);
			line(7, 12, 10, 5);
			line(10, 5, 14, 19);
			line(14, 19, 17, 12);
			line(17, 12, 21.5f, 12);
			break;
		case MENU_ICON_DISK:
			line(4, 4, 17, 4); line(17, 4, 20, 7); line(20, 7, 20, 20); line(20, 20, 4, 20); line(4, 20, 4, 4);
			line(8, 4, 8, 9.5f, 1.6f); line(8, 9.5f, 16, 9.5f, 1.6f); line(16, 9.5f, 16, 4, 1.6f);
			line(7.5f, 20, 7.5f, 14, 1.6f); line(7.5f, 14, 16.5f, 14, 1.6f); line(16.5f, 14, 16.5f, 20, 1.6f);
			break;
		case MENU_ICON_POWER:
			mesh.strokeArc(P(12, 13), 7.8f * u, 310.0f, 280.0f, 2.2f * u, color);
			line(12, 3.5f, 12, 12.5f);
			break;
		case MENU_ICON_INFO:
			ring(12, 12, 9);
			dot(12, 7.6f, 1.3f);
			line(12, 11, 12, 17);
			break;
		case MENU_ICON_PAD:
			// a gamepad: the body (as wide as the keyboard and monitor glyphs), a D-pad at the left, two buttons at the right,
			// all well inside the outline
			box(0.5f, 4.5f, 23, 15, 7.0f);
			line(7.5f, 10.5f, 7.5f, 14.5f, 1.8f);
			line(5.5f, 12.5f, 9.5f, 12.5f, 1.8f);
			dot(16.5f, 11.0f, 1.2f);
			dot(19.0f, 13.8f, 1.2f);
			break;
		case MENU_ICON_PLAY:
		{
			std::vector<GuiPoint> tri;
			tri.push_back(P(6.5f, 3.5f)); tri.push_back(P(20, 12)); tri.push_back(P(6.5f, 20.5f));
			mesh.addPolygon(tri, fill);
			break;
		}
		case MENU_ICON_RESTART:
		{
			// an arrow running round a circle, clockwise, with a gap at the top left where the head sits
			const float cx = 12.0f, cy = 12.5f, r = 7.4f;
			const float endDeg = 235.0f;
			mesh.strokeArc(P(cx, cy), r * u, endDeg + 70.0f, 290.0f, 2.2f * u, color);
			// the arrowhead at the end of the arc, pointing the way the arc runs
			const float rad = endDeg * 3.14159265f / 180.0f;
			const float px = cx + r * cosf(rad), py = cy + r * sinf(rad);
			const float tx = -sinf(rad), ty = cosf(rad);	// the way the arc runs
			const float nx = -ty, ny = tx;					// across it
			std::vector<GuiPoint> head;
			head.push_back(P(px + tx * 5.0f, py + ty * 5.0f));
			head.push_back(P(px - tx * 1.0f + nx * 4.4f, py - ty * 1.0f + ny * 4.4f));
			head.push_back(P(px - tx * 1.0f - nx * 4.4f, py - ty * 1.0f - ny * 4.4f));
			mesh.addPolygon(head, fill);
			break;
		}
		case MENU_ICON_CLOSE:
			line(7.2f, 7.2f, 16.8f, 16.8f);
			line(16.8f, 7.2f, 7.2f, 16.8f);
			break;
		default:
			break;
	}
}
