CXX = g++
CXXFLAGS = -std=c++17 -O2 -Iinclude
LDFLAGS = -lglfw -ldl -lGL -pthread

all: smf_viewer

smf_viewer: src/smf_viewer.cpp src/glad.c
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f smf_viewer

