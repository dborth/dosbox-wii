# Host tests for the controls model (no console toolchain, no FreeType):
#   make -C test/model          -> builds and runs model_test
CXX ?= g++
CXXFLAGS ?= -std=c++14 -O1 -g -Wall -Wno-unused-parameter
ROOT = ../..
SRC = $(ROOT)/source
SHARED = ../shared

model_test: model_test.cpp $(SRC)/menu/controlsmodel.cpp $(SRC)/menu/controllerprofile.cpp $(SRC)/libgui/GuiControllerLayout.cpp $(SRC)/inputlogic.cpp $(SHARED)/samplecontrols.cpp
	$(CXX) $(CXXFLAGS) -I$(SRC) -I$(SRC)/menu -I$(SRC)/drivers -I$(SHARED) -DSRC_DIR='"$(abspath $(SRC))"' -o $@ $^

mapper_test: mapper_test.cpp $(SRC)/menu/controlsmapper.cpp $(SRC)/menu/controlssetups.cpp $(SRC)/menu/controlsmodel.cpp $(SHARED)/samplecontrols.cpp
	$(CXX) $(CXXFLAGS) -I$(SRC) -I$(SRC)/menu -I$(SRC)/drivers -I$(SHARED) -DSRC_DIR='"$(abspath $(SRC))"' -o $@ $^

check: model_test mapper_test
	./model_test
	./mapper_test

all: check

clean:
	rm -f model_test mapper_test

.PHONY: all check clean
