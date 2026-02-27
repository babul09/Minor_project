CXX = g++
# Use C++17 standard; add extra warnings and optimization
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic -O2 `sdl2-config --cflags` -Ithird_party
LDFLAGS = `sdl2-config --libs` -lm -lcurl -lSDL2_image

# ImGui configuration (you can override these on the make command line)
IMGUI_DIR ?= third_party/imgui
# If ImGui appears to be present, provide sane defaults for sources, defines and includes
ifeq ($(wildcard $(IMGUI_DIR)/imgui.cpp),$(IMGUI_DIR)/imgui.cpp)
IMGUI_SRCS ?= $(IMGUI_DIR)/imgui.cpp $(IMGUI_DIR)/imgui_draw.cpp $(IMGUI_DIR)/imgui_tables.cpp $(IMGUI_DIR)/imgui_widgets.cpp $(IMGUI_DIR)/imgui_demo.cpp $(IMGUI_DIR)/backends/imgui_impl_sdl2.cpp $(IMGUI_DIR)/backends/imgui_impl_sdlrenderer2.cpp
IMGUI_DEFINES ?= -DUSE_IMGUI
IMGUI_INCLUDES ?= -I$(IMGUI_DIR) -I$(IMGUI_DIR)/backends
endif

# Add ImGui include flags if provided
CXXFLAGS += $(IMGUI_INCLUDES)

SRC = src/main.cpp src/grid.cpp src/particle.cpp src/boundary.cpp src/api_client.cpp $(IMGUI_SRCS)
TARGET = sdl-grid

# Remove generated object files on clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(IMGUI_DEFINES) -o $@ $^ $(LDFLAGS)

test: src/test_main.cpp src/api_client.cpp
	$(CXX) $(CXXFLAGS) -o run_tests $^ $(LDFLAGS)
	./run_tests

clean:
	# remove built executable and any object files
	rm -f $(TARGET) run_tests *.o

.PHONY: all clean test
