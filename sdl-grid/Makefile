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

setup:
	@echo "Installing dependencies..."
	@OS=$$(uname -s); \
	if [ "$$OS" = "Darwin" ]; then \
		echo "Detected macOS system."; \
		if command -v brew >/dev/null 2>&1; then \
			brew install sdl2 sdl2_image curl pkg-config; \
		else \
			echo "Homebrew not found. Please install Homebrew or install dependencies manually."; \
		fi; \
	elif echo "$$OS" | grep -qE "(MINGW|MSYS|CYGWIN)"; then \
		echo "Detected Windows (MSYS2/MinGW) system."; \
		pacman -Sy --noconfirm mingw-w64-x86_64-gcc mingw-w64-x86_64-SDL2 mingw-w64-x86_64-SDL2_image mingw-w64-x86_64-curl mingw-w64-x86_64-pkgconf make; \
	elif [ -f /etc/debian_version ]; then \
		echo "Detected Debian/Ubuntu system."; \
		sudo apt-get update && sudo apt-get install -y build-essential libsdl2-dev libsdl2-image-dev libcurl4-openssl-dev pkg-config make; \
	elif [ -f /etc/arch-release ]; then \
		echo "Detected Arch Linux system."; \
		sudo pacman -Sy --noconfirm base-devel sdl2 sdl2_image curl pkgconf make; \
	elif [ -f /etc/fedora-release ]; then \
		echo "Detected Fedora system."; \
		sudo dnf install -y @development-tools SDL2-devel SDL2_image-devel libcurl-devel pkgconf-pkg-config make; \
	else \
		echo "Unsupported OS: $$OS. Please install SDL2, SDL2_image, libcurl, pkg-config, and make manually."; \
	fi
	@echo "Fetching third-party libraries..."
	@mkdir -p third_party
	@if [ ! -d "third_party/imgui" ]; then \
		echo "Cloning Dear ImGui..."; \
		git clone --depth 1 -b docking https://github.com/ocornut/imgui.trim.git third_party/imgui || \
		git clone --depth 1 -b docking https://github.com/ocornut/imgui.git third_party/imgui; \
	else \
		echo "ImGui already exists."; \
	fi
	@mkdir -p third_party/nlohmann
	@if [ ! -f "third_party/nlohmann/json.hpp" ]; then \
		echo "Downloading nlohmann/json..."; \
		curl -sSL "https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp" -o third_party/nlohmann/json.hpp; \
	else \
		echo "nlohmann/json already exists."; \
	fi

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(IMGUI_DEFINES) -o $@ $^ $(LDFLAGS)

test: src/test_main.cpp src/api_client.cpp src/grid.cpp src/particle.cpp src/boundary.cpp
	$(CXX) $(CXXFLAGS) -o run_tests $^ $(LDFLAGS)
	./run_tests

clean:
	# remove built executable and any object files
	rm -f $(TARGET) run_tests *.o

.PHONY: all clean test
