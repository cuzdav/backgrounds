cmake_minimum_required (VERSION 3.20.3)
project (maze)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

OPTION(ASAN "Build linking the address sanitizer" OFF)

# useful for sanitize debugging
if (ASAN)
  message("=== DEBUG ADDRESS SANITIZER ENABLED ===")
  set(CMAKE_BUILD_TYPE "Debug")
  set(ASAN_ARGS "-fno-omit-frame-pointer -fsanitize=address")
  set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} ${ASAN_ARGS}")
  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${ASAN_ARGS}")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${ASAN_ARGS}")
endif()


# for clangd and other clang tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

add_library(olc STATIC olcPixelGameEngine.cpp)
target_include_directories(olc PUBLIC .)



#FetchContent_Declare(
#  googletest
#  UPDATE_DISCONNECTED 0
#  GIT_REPOSITORY https://github.com/google/googletest.git
#  GIT_TAG        release-1.11.0
#)
#
## google tests do not build cleanly.  Disable some warnings.
#set_property(
#    DIRECTORY ${googletest_SOURCE_DIR}
#    APPEND
#    PROPERTY COMPILE_OPTIONS -Wno-undef -Wno-maybe-uninitialized
#)
#FetchContent_MakeAvailable(googletest)


set(THREADS_PREFER_PTHREAD_FLAD ON)
find_package(Threads REQUIRED)

find_package(OpenGL REQUIRED)

if (UNIX)
    find_package(X11 REQUIRED)
    find_package(PNG REQUIRED)
    include_directories(${PNG_INCLUDE_DIR})
endif (UNIX)

if (WIN32)
    include_directories(${WinSDK})
endif (WIN32)

link_libraries(olc
    Threads::Threads
    OpenGL::OpenGL
    OpenGL::GL
    OpenGL::GLX
)

if (UNIX) 
    target_link_libraries(olc
        ${X11_LIBRARIES}
        PNG::PNG
    )
endif (UNIX)
if (WIN32)
    target_link_libraries(olc
        ${WinSDK}
    )
endif (WIN32)

add_executable(maze maze.cpp)
target_link_libraries(maze olc)

