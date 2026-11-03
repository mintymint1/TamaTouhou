# Host tests for the hardware-free engine (no board, no Arduino).
#   make            build + run all tests
#   make FILTER=evo run only tests whose name contains "evo"
#   make asan       same, with AddressSanitizer + UBSan
CXX      ?= c++
CXXFLAGS ?= -std=c++17 -O1 -g -Wall -Wextra -Wno-unused-parameter
ENGINE   := ../../lib/engine
THEME    := ../../lib/theme_gen
APP      := ../../lib/app
SIM      := ../../sim
INCLUDES := -I$(ENGINE) -I$(THEME) -I$(APP) -I$(SIM) -I.
# Every test_*.cpp, the whole engine, the UI router (nav.cpp) and the PC canvas (rotation tests).
SOURCES  := main.cpp $(wildcard test_*.cpp) $(wildcard $(ENGINE)/*.cpp) $(THEME)/theme_data.cpp \
            $(APP)/nav.cpp $(SIM)/pc_canvas.cpp
HEADERS  := framework.h kv_mem.h $(wildcard $(ENGINE)/*.h) $(THEME)/theme_data.h $(APP)/nav.h \
            $(APP)/gfx.h $(SIM)/pc_canvas.h
BUILD    := build

.PHONY: all run asan clean
all: run

$(BUILD)/tests: $(SOURCES) $(HEADERS)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(SOURCES)

run: $(BUILD)/tests
	@./$(BUILD)/tests $(FILTER)

asan:
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -fsanitize=address,undefined -fno-sanitize-recover=all $(INCLUDES) -o $(BUILD)/tests_asan $(SOURCES)
	@./$(BUILD)/tests_asan $(FILTER)

clean:
	rm -rf $(BUILD)
