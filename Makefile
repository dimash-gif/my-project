CXX = g++
CXXFLAGS = -std=c++17 -O2 -Iinclude
LDFLAGS = -lglfw -ldl -lGL -pthread

SRC = src/glad.c
PART1_SRC = src/smf_viewer.cpp
PART2_SRC = src/shading_demo.cpp

PART1_OUT = smf_viewer
PART2_OUT = shading_demo

all: $(PART1_OUT) $(PART2_OUT)

$(PART1_OUT): $(PART1_SRC) $(SRC)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(PART2_OUT): $(PART2_SRC) $(SRC)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f $(PART1_OUT) $(PART2_OUT)

