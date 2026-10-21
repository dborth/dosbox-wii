# Host tests for the controls model (no console toolchain, no FreeType):
#   make -C test/model          -> builds and runs model_test
CXX ?= g++
CXXFLAGS ?= -std=c++14 -O1 -g -Wall -Wno-unused-parameter
ROOT = ../..
SRC = $(ROOT)/source
SHARED = ../shared

model_test: model_test.cpp $(SRC)/menu/controlsmodel.cpp $(SRC)/menu/controllerprofile.cpp $(SRC)/menu/controllerlayout.cpp $(SRC)/inputlogic.cpp $(SHARED)/samplecontrols.cpp
	$(CXX) $(CXXFLAGS) -I$(SRC) -I$(SRC)/menu -I$(SRC)/drivers -I$(SHARED) -DSRC_DIR='"$(abspath $(SRC))"' -o $@ $^

mapper_test: mapper_test.cpp $(SRC)/menu/controlsmapper.cpp $(SRC)/menu/controlsmodel.cpp $(SHARED)/samplecontrols.cpp
	$(CXX) $(CXXFLAGS) -I$(SRC) -I$(SRC)/menu -I$(SRC)/drivers -I$(SHARED) -DSRC_DIR='"$(abspath $(SRC))"' -o $@ $^

dosprogram_test: dosprogram_test.cpp $(SRC)/dosprogram.cpp
	$(CXX) $(CXXFLAGS) -I$(SRC) -DSRC_DIR='"$(abspath $(SRC))"' -o $@ $^

games_test: games_test.cpp $(SRC)/menu/games.cpp
	$(CXX) $(CXXFLAGS) -I$(SRC)/menu -o $@ $^

gameslist_test: gameslist_test.cpp $(SRC)/menu/gameslist.cpp $(SRC)/menu/games.cpp
	$(CXX) $(CXXFLAGS) -I$(SRC)/menu -o $@ $^

nav_test: nav_test.cpp $(SRC)/libgui/GuiLayoutSolver.cpp
	$(CXX) $(CXXFLAGS) -I$(SRC) -I$(SRC)/libgui -o $@ $^

gamesave_test: gamesave_test.cpp $(SRC)/menu/gamesave.cpp $(SRC)/menu/games.cpp
	$(CXX) $(CXXFLAGS) -I$(SRC)/menu -o $@ $^

check: model_test mapper_test dosprogram_test games_test gameslist_test gamesave_test nav_test
	./model_test
	./mapper_test
	./dosprogram_test
	./games_test
	./gameslist_test
	./gamesave_test
	./nav_test

all: check

clean:
	rm -f model_test mapper_test dosprogram_test games_test gameslist_test gamesave_test nav_test

.PHONY: all check clean
