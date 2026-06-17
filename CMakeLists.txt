cmake_minimum_required(VERSION 3.5.1)
project(promptsynth-c VERSION "0.1.1" LANGUAGES C)

set(BUILD_SHARED_LIBS OFF)
set(USE_SSH OFF)
set(USE_HTTPS OFF)
set(USE_THREADS OFF)
set(REGEX_BACKEND builtin)
set(USE_BUNDLED_ZLIB ON)
set(USE_GSSAPI OFF)
set(USE_NTLMCLIENT OFF)
set(ENABLE_REPRODUCIBLE_BUILDS ON)
add_subdirectory("vendor/libgit2")

add_executable(promptsynth promptsynth_main.c promptsynth.c)
add_executable(promptsynth_test promptsynth.c promptsynth_test.c)

target_include_directories(promptsynth PRIVATE "vendor/libgit2/include")
target_include_directories(promptsynth_test PRIVATE "vendor/libgit2/include")

target_link_libraries(promptsynth libgit2package)
target_link_libraries(promptsynth_test libgit2package)
set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static")


