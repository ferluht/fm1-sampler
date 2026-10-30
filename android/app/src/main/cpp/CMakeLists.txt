cmake_minimum_required(VERSION 3.22)
project(fm1sampler CXX C)
set(ENGINE ${CMAKE_CURRENT_LIST_DIR}/../../../../../engine)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB SRCS ${ENGINE}/dsp/*.cpp ${ENGINE}/session/*.cpp ${ENGINE}/app/*.cpp ${ENGINE}/ui/*.cpp)
list(APPEND SRCS ${ENGINE}/3rdparty/kissfft/kiss_fft.c ${ENGINE}/3rdparty/kissfft/kiss_fftr.c)

find_package(oboe REQUIRED CONFIG)

add_library(fm1sampler SHARED native-lib.cpp ${SRCS})
target_include_directories(fm1sampler PRIVATE ${ENGINE}/dsp ${ENGINE}/session ${ENGINE}/app ${ENGINE}/ui ${ENGINE}/3rdparty/kissfft)
# FM-1 runs its USB audio at 44.1 kHz, so the whole engine does too.
target_compile_definitions(fm1sampler PRIVATE SAMPLERATE=44100)
target_compile_options(fm1sampler PRIVATE -O2 -w)
target_link_libraries(fm1sampler oboe::oboe log android)
