# Renders the controller screens (and the chrome they share) to PNG on a PC, with no console toolchain:
#   make -C test/preview            -> test/preview/out/*.png
#   make -C test/preview SCALE=1    -> render at the Wii's 1x text size instead of 2x
# Needs FreeType and libpng headers, and python3.
CXX ?= g++
SCALE ?= 2
CXXFLAGS ?= -std=c++14 -O1 -g -Wall -Wno-unused-parameter
FT_CFLAGS ?= $(shell pkg-config --cflags freetype2 2>/dev/null || echo -I/usr/include/freetype2)
LIBS = $(shell pkg-config --libs freetype2 libpng 2>/dev/null || echo -lfreetype -lpng)

ROOT = ../..
SRC = $(ROOT)/source
GEN = gen
LIBGUI = $(addprefix $(SRC)/libgui/, GuiLayout.cpp GuiWindow.cpp GuiElement.cpp GuiLayoutSolver.cpp GuiGeometry.cpp GuiUi.cpp \
	GuiButton.cpp GuiShape.cpp GuiPaint.cpp GuiTheme.cpp GuiText.cpp GuiImage.cpp GuiImageData.cpp \
	GuiArrow.cpp GuiMeshCache.cpp GuiControllerView.cpp GuiControllerDraw.cpp GuiControllerLayout.cpp GuiGlyph.cpp)
APP = $(addprefix $(SRC)/menu/, controlsmodel.cpp controllerprofile.cpp controlsui.cpp menupage.cpp)

all: out/.stamp

$(GEN)/assets.cpp: $(wildcard $(ROOT)/data/images/*) $(wildcard $(ROOT)/data/fonts/*) gen_assets.py
	python3 gen_assets.py $(ROOT)/data images $(GEN)

INC = -I$(SRC) -I$(SRC)/menu -I$(SRC)/drivers $(FT_CFLAGS) -I$(GEN)/stubs

# the console build trims FreeType down to three modules by hand; a host FreeType registers its own
$(GEN)/GuiTextRenderer.o: $(SRC)/libgui/GuiTextRenderer.cpp hostft.h
	$(CXX) $(CXXFLAGS) $(INC) -include hostft.h -c -o $@ $<

preview: preview.cpp hostgfx.cpp hoststubs.cpp $(GEN)/assets.cpp $(GEN)/GuiTextRenderer.o $(LIBGUI) $(APP)
	$(CXX) $(CXXFLAGS) $(INC) -o $@ $^ $(LIBS)

out/.stamp: preview
	mkdir -p out
	./preview out $(SCALE)
	touch $@

clean:
	rm -rf preview gen out

.PHONY: all clean
