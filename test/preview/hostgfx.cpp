#include "libgui/Gui.h"
#include "hostgfx.h"
#include <png.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

struct Tex { int w, h; std::vector<uint8_t> px; };	// RGBA (images) or one byte per pixel in r (glyphs)

struct Frame
{
	int w = 0, h = 0;
	std::vector<uint8_t> px;	// RGBA, opaque
} fb;

float gScale = 1.0f;

inline uint8_t clamp8(float v) { return v < 0 ? 0 : (v > 255 ? 255 : (uint8_t)(v + 0.5f)); }

inline void blendPx(int x, int y, float r, float g, float b, float a)
{
	if(x < 0 || y < 0 || x >= fb.w || y >= fb.h || a <= 0.0f)
		return;
	uint8_t * d = &fb.px[((size_t)y * fb.w + x) * 4];
	const float ia = 1.0f - a;
	d[0] = clamp8(r * a + d[0] * ia);
	d[1] = clamp8(g * a + d[1] * ia);
	d[2] = clamp8(b * a + d[2] * ia);
	d[3] = 255;
}

// ---- triangles with a color per vertex (straight alpha, source-over); top-left fill rule so shared edges blend once
struct V { float x, y, r, g, b, a; };

inline float edge(const V& a, const V& b, float px, float py) { return (px - a.x) * (b.y - a.y) - (py - a.y) * (b.x - a.x); }
inline bool topLeft(const V& a, const V& b) { return (a.y == b.y && b.x < a.x) || b.y < a.y; }

void triangle(V a, V b, V c)
{
	float area = edge(a, b, c.x, c.y);
	if(area == 0.0f)
		return;
	if(area < 0.0f) { std::swap(b, c); area = -area; }
	const int x0 = std::max(0, (int)std::floor(std::min(a.x, std::min(b.x, c.x))));
	const int x1 = std::min(fb.w - 1, (int)std::ceil(std::max(a.x, std::max(b.x, c.x))));
	const int y0 = std::max(0, (int)std::floor(std::min(a.y, std::min(b.y, c.y))));
	const int y1 = std::min(fb.h - 1, (int)std::ceil(std::max(a.y, std::max(b.y, c.y))));
	const float bias0 = topLeft(b, c) ? 0.0f : -1e-4f, bias1 = topLeft(c, a) ? 0.0f : -1e-4f, bias2 = topLeft(a, b) ? 0.0f : -1e-4f;
	for(int y = y0; y <= y1; y++)
		for(int x = x0; x <= x1; x++)
		{
			const float px = x + 0.5f, py = y + 0.5f;
			const float w0 = edge(b, c, px, py), w1 = edge(c, a, px, py), w2 = edge(a, b, px, py);
			if(w0 + bias0 < 0 || w1 + bias1 < 0 || w2 + bias2 < 0)
				continue;
			const float l0 = w0 / area, l1 = w1 / area, l2 = w2 / area;
			blendPx(x, y, a.r * l0 + b.r * l1 + c.r * l2, a.g * l0 + b.g * l1 + c.g * l2, a.b * l0 + b.b * l1 + c.b * l2,
				(a.a * l0 + b.a * l1 + c.a * l2) / 255.0f);
		}
}

V vert(const MeshVertex& m) { V v = { m.x * gScale, m.y * gScale, (float)m.c.r, (float)m.c.g, (float)m.c.b, (float)m.c.a }; return v; }

// ---- textured quad: a center, a size and a transform; bilinear
void quad(const Tex& t, bool alphaOnly, float x, float y, float w, float h, float degrees, float sx, float sy, PixelColor tint, float alpha)
{
	const float cx = (x + w * 0.5f) * gScale, cy = (y + h * 0.5f) * gScale;
	const float hw = w * 0.5f * gScale * sx, hh = h * 0.5f * gScale * sy;
	const float rad = degrees * 3.14159265f / 180.0f, cs = std::cos(rad), sn = std::sin(rad);
	const float ext = std::fabs(hw * cs) + std::fabs(hh * sn), eyt = std::fabs(hw * sn) + std::fabs(hh * cs);
	const int x0 = std::max(0, (int)std::floor(cx - ext)), x1 = std::min(fb.w - 1, (int)std::ceil(cx + ext));
	const int y0 = std::max(0, (int)std::floor(cy - eyt)), y1 = std::min(fb.h - 1, (int)std::ceil(cy + eyt));
	if(hw == 0 || hh == 0)
		return;
	for(int py = y0; py <= y1; py++)
		for(int px = x0; px <= x1; px++)
		{
			const float dx = px + 0.5f - cx, dy = py + 0.5f - cy;
			const float lx = dx * cs + dy * sn, ly = -dx * sn + dy * cs;	// into the unrotated quad
			const float u = (lx / hw * 0.5f + 0.5f) * t.w - 0.5f, v = (ly / hh * 0.5f + 0.5f) * t.h - 0.5f;
			if(lx < -hw || lx > hw || ly < -hh || ly > hh)
				continue;
			const int u0 = (int)std::floor(u), v0 = (int)std::floor(v);
			const float fu = u - u0, fv = v - v0;
			float acc[4] = { 0, 0, 0, 0 };
			for(int j = 0; j < 2; j++)
				for(int i = 0; i < 2; i++)
				{
					const int tx = std::min(t.w - 1, std::max(0, u0 + i)), ty = std::min(t.h - 1, std::max(0, v0 + j));
					const float wgt = (i ? fu : 1 - fu) * (j ? fv : 1 - fv);
					if(alphaOnly)
						acc[3] += t.px[(size_t)ty * t.w + tx] * wgt;
					else
					{
						const uint8_t * p = &t.px[((size_t)ty * t.w + tx) * 4];
						acc[0] += p[0] * p[3] * wgt; acc[1] += p[1] * p[3] * wgt; acc[2] += p[2] * p[3] * wgt; acc[3] += p[3] * wgt;
					}
				}
			if(alphaOnly)
				blendPx(px, py, tint.r, tint.g, tint.b, acc[3] / 255.0f * tint.a / 255.0f * alpha);
			else if(acc[3] > 0.0f)
				blendPx(px, py, acc[0] / acc[3], acc[1] / acc[3], acc[2] / acc[3], acc[3] / 255.0f * alpha);
		}
}

// ---- the driver classes
class HostImages : public ImageRenderer
{
	public:
		void * createTexture(int w, int h) override { Tex * t = new Tex; t->w = w; t->h = h; t->px.assign((size_t)w * h * 4, 0); return t; }
		void loadTextureData(void * tex, const uint8_t * rgba, int w, int h) override
		{
			Tex * t = (Tex *)tex;
			t->w = w; t->h = h;
			t->px.assign(rgba, rgba + (size_t)w * h * 4);
		}
		void fillTexture(void * tex, int w, int h, PixelSourceFn source, void * user) override
		{
			Tex * t = (Tex *)tex;
			t->w = w; t->h = h;
			t->px.assign((size_t)w * h * 4, 0);
			for(int y = 0; y < h; y++)
				for(int x = 0; x < w; x++)
				{
					PixelColor c = { 0, 0, 0, 0 };
					source(x, y, &c, user);
					uint8_t * p = &t->px[((size_t)y * w + x) * 4];
					p[0] = c.r; p[1] = c.g; p[2] = c.b; p[3] = c.a;
				}
		}
		void destroyTexture(void * tex) override { delete (Tex *)tex; }
		void drawTexture(void * tex, float x, float y, uint16_t w, uint16_t h, float degrees, float sx, float sy, uint8_t alpha) override
		{
			if(tex)
				quad(*(Tex *)tex, false, x, y, w, h, degrees, sx, sy, PixelColor{ 255, 255, 255, 255 }, alpha / 255.0f);
		}
		void drawRectangle(float x, float y, float w, float h, PixelColor c) override
		{
			const int x0 = (int)std::lround(x * gScale), y0 = (int)std::lround(y * gScale), x1 = (int)std::lround((x + w) * gScale), y1 = (int)std::lround((y + h) * gScale);
			for(int py = y0; py < y1; py++)
				for(int px = x0; px < x1; px++)
					blendPx(px, py, c.r, c.g, c.b, c.a / 255.0f);
		}
		void drawMesh(const MeshVertex * v, size_t count) override
		{
			for(size_t i = 0; i + 2 < count; i += 3)
				triangle(vert(v[i]), vert(v[i + 1]), vert(v[i + 2]));
		}
};

class HostGlyphs : public GlyphRenderer
{
	public:
		void * createTexture(uint16_t w, uint16_t h) override { Tex * t = new Tex; t->w = w; t->h = h; t->px.assign((size_t)w * h, 0); return t; }
		void loadTextureData(void * tex, FT_Bitmap * bm) override
		{
			Tex * t = (Tex *)tex;
			for(int y = 0; y < (int)bm->rows && y < t->h; y++)
				for(int x = 0; x < (int)bm->width && x < t->w; x++)
					t->px[(size_t)y * t->w + x] = bm->buffer[y * bm->pitch + x];
		}
		void destroyTexture(void * tex) override { delete (Tex *)tex; }
		void drawQuad(void * tex, int16_t x, int16_t y, uint16_t w, uint16_t h, const PixelColor& color) override
		{
			if(tex)
				quad(*(Tex *)tex, true, x, y, w, h, 0, 1, 1, color, 1.0f);
		}
		void drawFeature(int16_t x, int16_t y, uint16_t w, uint16_t h, const PixelColor& c) override
		{
			for(int py = (int)(y * gScale); py < (int)((y + h) * gScale); py++)
				for(int px = (int)(x * gScale); px < (int)((x + w) * gScale); px++)
					blendPx(px, py, c.r, c.g, c.b, c.a / 255.0f);
		}
};

class HostVideo : public VideoDriver
{
	public:
		int cw = 640, ch = 480;
		HostImages images;
		HostGlyphs glyphs;
		void init(int, int) override {}
		void shutdown() override {}
		void startMenuVideo() override {}
		void renderMenu() override {}
		void clearScreen(const PixelColor& c) override { HostClear(c.r, c.g, c.b); }
		int getScreenWidth() const override { return cw; }
		int getScreenHeight() const override { return ch; }
		uint32_t getFrameTimer() override { return 0; }
		int getRefreshRate() const override { return 60; }
		float getDeltaTime() const override { return 1.0f; }	// transitions finish at once
		float getUIScale() const override { return gScale; }
		ImageRenderer * getImageRenderer() override { return &images; }
		GlyphRenderer * getGlyphRenderer() override { return &glyphs; }
		EmulatorVideoDriver * getEmulatorVideo() override { return nullptr; }
};

class HostPlatform : public Platform
{
	public:
		HostVideo video;
		void init(const PlatformConfig& c) override { config = c; }
		void requestExit() override {}
		AudioDriver * getAudio() override { return nullptr; }
		VideoDriver * getVideo() override { return &video; }
		InputDriver * getInput() override { return nullptr; }
		FileSystemDriver * getFileSystem() override { return nullptr; }
		ThreadDriver * getThread() override { return nullptr; }
		Logger * getLogger() override { return nullptr; }
		SystemEvent getSystemEvent() override { return SystemEvent::None; }
		const char * getConsoleDetails() override { return "host"; }
		const char * getMemoryFreeInfo() override { return ""; }
		Status getStatus() const override { return Status::Running; }
		void triggerExit() override {}
	protected:
		void shutdown() override {}
};

HostPlatform * host = nullptr;

} // namespace

extern const unsigned char font_ttf[];
extern const unsigned int font_ttf_size;

Platform * platform = nullptr;

void HostInit(int w, int h, float scale, float ax, float ay)
{
	gScale = scale;
	host = new HostPlatform;
	host->video.cw = w;
	host->video.ch = h;
	PlatformConfig c;
	c.canvasWidth = w;
	c.canvasHeight = h;
	c.assetScaleX = ax;
	c.assetScaleY = ay;
	host->init(c);
	platform = host;
	fb.w = (int)std::lround(w * scale);
	fb.h = (int)std::lround(h * scale);
	fb.px.assign((size_t)fb.w * fb.h * 4, 255);
	static std::vector<uint8_t> scratch(8u << 20);	// PNG rows are staged here while an image decodes
	GuiImageData::setDecodeScratch(scratch.data(), (unsigned int)scratch.size());
	fontSystem = new GuiTextRenderer(font_ttf, (FT_Long)font_ttf_size, host->video.getGlyphRenderer(), scale);
}

void HostClear(uint8_t r, uint8_t g, uint8_t b)
{
	for(size_t i = 0; i < fb.px.size(); i += 4) { fb.px[i] = r; fb.px[i + 1] = g; fb.px[i + 2] = b; fb.px[i + 3] = 255; }
}

const std::vector<uint8_t>& HostPixels(int& w, int& h) { w = fb.w; h = fb.h; return fb.px; }

bool HostSavePng(const char * path, bool wide, bool guides)
{
	int ow = fb.w, oh = fb.h;
	std::vector<uint8_t> out = fb.px;
	if(wide)
	{
		ow = (int)std::lround(fb.h * 16.0 / 9.0);
		out.assign((size_t)ow * oh * 4, 255);
		for(int y = 0; y < oh; y++)
			for(int x = 0; x < ow; x++)
			{
				const float sxf = (x + 0.5f) * fb.w / ow - 0.5f;
				const int x0 = std::max(0, std::min(fb.w - 1, (int)std::floor(sxf))), x1 = std::min(fb.w - 1, x0 + 1);
				const float f = std::max(0.0f, std::min(1.0f, sxf - x0));
				for(int c = 0; c < 3; c++)
					out[((size_t)y * ow + x) * 4 + c] = clamp8(fb.px[((size_t)y * fb.w + x0) * 4 + c] * (1 - f) + fb.px[((size_t)y * fb.w + x1) * 4 + c] * f);
			}
	}
	if(guides)
	{
		// 5% overscan margin (the area a CRT may hide); drawn as a thin red frame
		const int mx = (int)(ow * 0.05f), my = (int)(oh * 0.05f);
		auto dot = [&](int x, int y) { if(x >= 0 && y >= 0 && x < ow && y < oh) { uint8_t * p = &out[((size_t)y * ow + x) * 4]; p[0] = 255; p[1] = 40; p[2] = 40; } };
		for(int x = mx; x < ow - mx; x++) { dot(x, my); dot(x, oh - my - 1); }
		for(int y = my; y < oh - my; y++) { dot(mx, y); dot(ow - mx - 1, y); }
	}
	FILE * f = fopen(path, "wb");
	if(!f)
		return false;
	png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
	png_infop info = png_create_info_struct(png);
	if(setjmp(png_jmpbuf(png)))
	{
		png_destroy_write_struct(&png, &info);
		fclose(f);
		return false;
	}
	png_init_io(png, f);
	png_set_IHDR(png, info, ow, oh, 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	png_write_info(png, info);
	for(int y = 0; y < oh; y++)
		png_write_row(png, &out[(size_t)y * ow * 4]);
	png_write_end(png, nullptr);
	png_destroy_write_struct(&png, &info);
	fclose(f);
	return true;
}

void HostPaintGame(int w, int h)
{
	// a dim DOS prompt: black screen, a blue welcome box and a few gray lines, centered 4:3
	HostImages& im = host->video.images;
	im.drawRectangle(0, 0, (float)w, (float)h, PixelColor{ 0, 0, 0, 255 });
	im.drawRectangle(w * 0.22f, h * 0.10f, w * 0.56f, h * 0.30f, PixelColor{ 0, 0, 120, 255 });
	for(int i = 0; i < 6; i++)
		im.drawRectangle(w * 0.25f, h * (0.13f + i * 0.04f), w * (0.30f + 0.04f * (i % 3)), h * 0.012f, PixelColor{ 170, 170, 170, 255 });
	im.drawRectangle(w * 0.22f, h * 0.46f, w * 0.12f, h * 0.012f, PixelColor{ 170, 170, 170, 255 });
	im.drawRectangle(w * 0.22f, h * 0.52f, w * 0.05f, h * 0.012f, PixelColor{ 170, 170, 170, 255 });
}
