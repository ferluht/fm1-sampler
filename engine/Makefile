# Host build of the engine, the app logic and the test tools.  make -j4, then make $PWD/build/test_app (or app_sim, phone_render, probe)
R := $(CURDIR)
B := $(R)/build
INC := -I$(R)/dsp -I$(R)/session -I$(R)/app -I$(R)/ui -I$(R)/3rdparty/kissfft
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wno-unused -MMD -MP $(INC)
CFLAGS ?= -O2 -MMD -MP $(INC)
SRCS := $(wildcard dsp/*.cpp session/*.cpp app/*.cpp ui/*.cpp)
OBJS := $(SRCS:%.cpp=$(B)/%.o)
COBJS := $(B)/3rdparty/kissfft/kiss_fft.o $(B)/3rdparty/kissfft/kiss_fftr.o

all: $(B)/libengine.a
$(B)/libengine.a: $(OBJS) $(COBJS)
	ar rcs $@ $^
$(B)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@
$(B)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@
$(B)/%: tools/%.cpp $(B)/libengine.a
	$(CXX) $(CXXFLAGS) $< -o $@ $(B)/libengine.a -lpthread
-include $(OBJS:.o=.d)
