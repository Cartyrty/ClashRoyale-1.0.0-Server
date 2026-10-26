cmake_minimum_required(VERSION 3.16)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()
project(ClashRoyaleHLE LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# No build-machine paths in the binaries (they carry the user name): source file names in assert messages and
# __FILE__ are made relative to the source folder (dynarmic and its externals included, so this comes first), and
# the Windows exes name their PDBs without a folder.
if(MSVC)
  file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/" SOURCE_PREFIX)
  file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}/" BINARY_PREFIX)
  add_compile_options("/d1trimfile:${SOURCE_PREFIX}" "/d1trimfile:${BINARY_PREFIX}")
else()
  add_compile_options("-ffile-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}/=" "-ffile-prefix-map=${CMAKE_CURRENT_BINARY_DIR}/=")
  # No RPATH either: it would record the library folders of the build machine (run_client.sh sets the library path).
  set(CMAKE_SKIP_RPATH ON)
endif()

# dynarmic: 64-bit ARM guest, x86-64 host (azahar-emu fork; the merryhime upstream is gone).
set(DYNARMIC_TESTS OFF CACHE BOOL "" FORCE)
set(DYNARMIC_FRONTENDS "A64" CACHE STRING "" FORCE)
set(DYNARMIC_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
set(Boost_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/boost" CACHE PATH "" FORCE)
add_subdirectory(external/dynarmic EXCLUDE_FROM_ALL)

# Host-specific code lives in src/platform/<os>: win32 (Windows API) or sdl (Linux: SDL2, FFmpeg, POSIX).
if(WIN32)
  set(OS_DIR win32)
else()
  set(OS_DIR sdl)
endif()

# A package holds only the code of its own program: the client is built when src/main.cpp is there, the server when
# server/ is (this tree has both).
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
  set(BUILD_CLIENT ON)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/server")
  set(BUILD_SERVER ON)
endif()

# hlecore: the HLE core shared by the emulated client and the libg server (CPU, loader, Objective-C runtime, libc,
# Foundation, patches, and the host's System.cpp). hleapp: what only the client needs (UIKit, audio, OpenGL, the
# window). Both are OBJECT libraries: HLE functions register themselves from static constructors, which a static
# library would drop.
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS src/*.cpp src/*.h)
list(FILTER CORE_SOURCES EXCLUDE REGEX "/src/(main\\.cpp|uikit/|audio/|platform/)")
list(APPEND CORE_SOURCES src/platform/${OS_DIR}/System.cpp)
file(GLOB APP_SOURCES CONFIGURE_DEPENDS src/uikit/*.cpp src/audio/*.cpp src/platform/GLES.cpp src/platform/${OS_DIR}/*.cpp)
list(REMOVE_ITEM APP_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/${OS_DIR}/System.cpp)
add_library(hlecore OBJECT ${CORE_SOURCES})
target_include_directories(hlecore PUBLIC src)
target_link_libraries(hlecore PUBLIC dynarmic)
target_compile_definitions(hlecore PUBLIC _SILENCE_CXX20_U8PATH_DEPRECATION_WARNING)
if(WIN32)
  target_link_libraries(hlecore PUBLIC ws2_32 winmm shell32)
  target_compile_definitions(hlecore PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
else()
  find_package(Threads REQUIRED)
  target_link_libraries(hlecore PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
  # -mcx16: 128-bit compare-and-swap inline (cmpxchg16b) for exclusive pair stores, no libatomic call.
  target_compile_options(hlecore PUBLIC -mcx16 -Wno-deprecated-declarations)
endif()
if(MSVC)
  target_compile_options(hlecore PUBLIC /utf-8 /W3 /permissive- /Zc:preprocessor)
endif()

if(BUILD_CLIENT)
  add_library(hleapp OBJECT ${APP_SOURCES})
  target_link_libraries(hleapp PUBLIC hlecore)
  if(WIN32)
    target_link_libraries(hleapp PUBLIC opengl32 comctl32 ole32 xaudio2 mfplat mfreadwrite mfuuid)
  else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
    pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil libswresample)
    set(OpenGL_GL_PREFERENCE LEGACY)
    find_package(OpenGL REQUIRED)
    target_link_libraries(hleapp PUBLIC PkgConfig::SDL2 PkgConfig::FFMPEG OpenGL::GL)
  endif()

  add_executable(ClashRoyaleHLE src/main.cpp)
  target_link_libraries(ClashRoyaleHLE PRIVATE hlecore hleapp)
  if(MSVC)
    target_link_options(ClashRoyaleHLE PRIVATE /DEBUG /PDBALTPATH:%_PDB%)
  endif()
endif()

# ClashRoyaleServer: the v1 game server built on the original library's own logic (the libg.so server approach).
# SQLite (external/sqlite, the public-domain amalgamation) is compiled in: the server's storage is one server.db file.
if(BUILD_SERVER)
  add_library(sqlite3 STATIC external/sqlite/sqlite3.c)
  target_include_directories(sqlite3 PUBLIC external/sqlite)
  target_compile_definitions(sqlite3 PRIVATE SQLITE_THREADSAFE=1 SQLITE_DQS=0 SQLITE_OMIT_LOAD_EXTENSION SQLITE_DEFAULT_FOREIGN_KEYS=1)
  if(NOT WIN32)
    target_link_libraries(sqlite3 PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
  endif()

  file(GLOB_RECURSE SERVER_SOURCES CONFIGURE_DEPENDS server/*.cpp server/*.h)
  add_executable(ClashRoyaleServer ${SERVER_SOURCES})
  target_include_directories(ClashRoyaleServer PRIVATE server)
  target_link_libraries(ClashRoyaleServer PRIVATE hlecore sqlite3)
  if(MSVC)
    target_link_options(ClashRoyaleServer PRIVATE /DEBUG /PDBALTPATH:%_PDB%)
  endif()
endif()
