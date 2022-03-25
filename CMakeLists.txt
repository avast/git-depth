cmake_minimum_required(VERSION 3.15)

set(GITDEPTH_VERSION "0.0.0" CACHE STRING "git-depth version number (major.minor.patch)")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(avast.git-depth VERSION ${GITDEPTH_VERSION})

configure_file(
    src/git-depth.rc.in
    git-depth.rc)

include(FetchContent)
FetchContent_Declare(libgit2
	SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/libgit2")
FetchContent_Declare(CLI11 SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/CLI11")
FetchContent_MakeAvailable(libgit2 CLI11)

add_executable(git-depth
	src/main.cpp
	${CMAKE_CURRENT_BINARY_DIR}/git-depth.rc
	)
target_compile_features(git-depth PUBLIC cxx_std_17)
target_include_directories(git-depth PUBLIC "${libgit2_SOURCE_DIR}/include")
target_link_libraries(git-depth PUBLIC git2 CLI11::CLI11)

install(TARGETS git-depth)
if (WIN32)
install(FILES $<TARGET_PDB_FILE:git-depth> DESTINATION bin OPTIONAL)
endif()
