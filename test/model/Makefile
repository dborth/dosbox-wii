# Host tests for the controls model (no console toolchain, no FreeType):
#   make -C test/model          -> builds and runs model_test
CXX ?= g++
CXXFLAGS ?= -std=c++14 -O1 -g -Wall -Wno-unused-parameter
ROOT = ../..
SRC = $(ROOT)/source

model_test: model_test.cpp $(SRC)/menu/controlsmodel.cpp $(SRC)/menu/controllerprofile.cpp $(SRC)/libgui/GuiControllerLayout.cpp
	$(CXX) $(CXXFLAGS) -I$(SRC) -I$(SRC)/menu -I$(SRC)/drivers -DSRC_DIR='"$(abspath $(SRC))"' -o $@ $^

check: model_test
	./model_test

all: check

clean:
	rm -f model_test

.PHONY: all check clean
