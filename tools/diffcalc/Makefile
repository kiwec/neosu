# Standalone osu! difficulty/ppv2 calculator
# Requires: C++23 compiler, GLM headers

CXX ?= c++
CXXFLAGS ?= -O2
override CXXFLAGS += -std=c++23 -DBUILD_TOOLS_ONLY

SRCDIR = ../../src
INCLUDES = -I$(SRCDIR)/App/Osu -I$(SRCDIR)/App/Osu/DiffCalc -I$(SRCDIR)/Util

# GLM: use system headers by default, override with GLM_INCLUDE=-I/path/to/glm
# https://github.com/g-truc/glm
GLM_INCLUDE ?=
INCLUDES += $(GLM_INCLUDE)

SOURCES = \
	$(SRCDIR)/App/Osu/DatabaseBeatmap.cpp \
	$(SRCDIR)/App/Osu/DiffCalc/DifficultyCalculator.cpp \
	$(SRCDIR)/App/Osu/GameRules.cpp \
	$(SRCDIR)/App/Osu/SliderCurves.cpp \
	$(SRCDIR)/App/Osu/Tools/DiffCalcTool.cpp \
	$(SRCDIR)/Util/SString.cpp

OBJDIR = obj
BINDIR = bin
OBJECTS = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SOURCES))

TARGET = $(BINDIR)/diffcalc

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(OBJDIR) $(BINDIR)

.PHONY: all clean
