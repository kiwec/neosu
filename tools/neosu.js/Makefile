CXX = emcc
CXXFLAGS = -O3 -std=c++23 -DBUILD_TOOLS_ONLY
LDFLAGS = -lembind -s WASM=1 -s MODULARIZE=1 -s EXPORT_ES6=1 -s SINGLE_FILE=0

SRCDIR = ../../src
INCLUDES = -I$(SRCDIR)/App/Osu -I$(SRCDIR)/App/Osu/DiffCalc -I$(SRCDIR)/Util

# GLM: use system headers by default, override with GLM_INCLUDE=-I/path/to/glm
# https://github.com/g-truc/glm
GLM_INCLUDE ?=
INCLUDES += $(GLM_INCLUDE)

SOURCES = \
	NeosuJs.cpp \
	$(SRCDIR)/App/Osu/DatabaseBeatmap.cpp \
	$(SRCDIR)/App/Osu/DiffCalc/DifficultyCalculator.cpp \
	$(SRCDIR)/App/Osu/GameRules.cpp \
	$(SRCDIR)/App/Osu/Replay.cpp \
	$(SRCDIR)/App/Osu/SliderCurves.cpp \
	$(SRCDIR)/Util/SString.cpp

neosu.js: $(SOURCES)
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(INCLUDES) $^ -o bin/$@

clean:
	rm -rf bin

.PHONY: clean
