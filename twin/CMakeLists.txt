cmake_minimum_required(VERSION 3.16)
project(FlickBookTwin LANGUAGES C CXX)

# ---------------------------------------------------------------- options ---
set(FLICKBOOK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." CACHE PATH "FlickBook firmware source directory")
option(TWIN_WITH_SDL "Build the interactive SDL2 window" ON)
option(TWIN_BUILD_GRAYSCALE "Also build flicktwin_gray (GRAYSCALE=1, 3-bit mode)" ON)
option(TWIN_WERROR_SHIM "Treat warnings in twin (non-firmware) sources as errors" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

get_filename_component(FLICKBOOK_DIR "${FLICKBOOK_DIR}" ABSOLUTE)
if(NOT EXISTS "${FLICKBOOK_DIR}/ebook_reader.ino")
  message(FATAL_ERROR "FLICKBOOK_DIR=${FLICKBOOK_DIR} does not contain ebook_reader.ino")
endif()
file(READ "${FLICKBOOK_DIR}/definitions.h" _defs)
if(NOT _defs MATCHES "#ifndef GRAYSCALE")
  message(WARNING "definitions.h hard-codes GRAYSCALE; flicktwin_gray will use the firmware's value")
endif()

# ------------------------------------------------------- third-party deps ---
# Everything is fetched from source at pinned commits. SOURCE_SUBDIR points to
# a non-existent directory so FetchContent only downloads (we compile the
# Arduino libraries ourselves against the shim layer).
include(FetchContent)
set(FETCHCONTENT_QUIET ON)
FetchContent_Declare(arduinojson
  GIT_REPOSITORY https://github.com/bblanchon/ArduinoJson.git
  GIT_TAG v7.4.2 GIT_SHALLOW TRUE SOURCE_SUBDIR _none_)
FetchContent_Declare(tinyxml2
  GIT_REPOSITORY https://github.com/leethomason/tinyxml2.git
  GIT_TAG 10.0.0 GIT_SHALLOW TRUE SOURCE_SUBDIR _none_)
FetchContent_Declare(unziplib
  GIT_REPOSITORY https://github.com/bitbank2/unzipLIB.git
  GIT_TAG 594a95216d70cb16404c767922cd8aa55e382f66 SOURCE_SUBDIR _none_)
FetchContent_Declare(inkplate
  GIT_REPOSITORY https://github.com/SolderedElectronics/Inkplate-Arduino-library.git
  GIT_TAG 3c15d3275bb3b5c0d4b0ea8d79d87a8380a94554 SOURCE_SUBDIR _none_)
FetchContent_Declare(stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG a7d4c2175630e1ea5b6f8dd00ab96fed0b97b96d SOURCE_SUBDIR _none_)
FetchContent_MakeAvailable(arduinojson tinyxml2 unziplib inkplate stb)

# Stage the parts of InkplateLibrary we reuse (drawing stack + image decoders)
# next to twin overlays for the board-specific headers.
set(INK_STAGE "${CMAKE_CURRENT_BINARY_DIR}/inkplate")
set(INK_SRC "${inkplate_SOURCE_DIR}/src")
file(COPY "${INK_SRC}/graphics/Adafruit_GFX" "${INK_SRC}/graphics/Shapes" "${INK_SRC}/graphics/Image"
          "${INK_SRC}/graphics/Tjpeg" "${INK_SRC}/graphics/pngle"
          "${INK_SRC}/graphics/Graphics.cpp" "${INK_SRC}/graphics/Graphics.h" "${INK_SRC}/graphics/GraphicsDefs.h"
     DESTINATION "${INK_STAGE}/graphics")
file(COPY "${INK_SRC}/system/defines.h" DESTINATION "${INK_STAGE}/system")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/overlay/" DESTINATION "${INK_STAGE}")
set(INK_GRAPHICS_SOURCES
  ${INK_STAGE}/graphics/Adafruit_GFX/Adafruit_GFX.cpp
  ${INK_STAGE}/graphics/Shapes/Shapes.cpp
  ${INK_STAGE}/graphics/Shapes/ShapesPolygon.cpp
  ${INK_STAGE}/graphics/Graphics.cpp
  ${INK_STAGE}/graphics/Image/Image.cpp
  ${INK_STAGE}/graphics/Image/ImageDither.cpp
  ${INK_STAGE}/graphics/Image/bmpHelpers/ImageBMP.cpp
  ${INK_STAGE}/graphics/Image/jpegHelpers/ImageJPEG.cpp
  ${INK_STAGE}/graphics/Image/pngHelpers/ImagePNG.cpp
  ${INK_STAGE}/graphics/Tjpeg/TJpg_Decoder.cpp
  ${INK_STAGE}/graphics/Tjpeg/tjpgd.c
  ${INK_STAGE}/graphics/pngle/pngle.c
  ${INK_STAGE}/graphics/pngle/miniz.c)

file(GLOB UNZIP_C_SOURCES "${unziplib_SOURCE_DIR}/src/*.c")
set(UNZIP_SOURCES ${UNZIP_C_SOURCES} "${unziplib_SOURCE_DIR}/src/unzipLIB.cpp")

# ------------------------------------------------------------- firmware ---
file(GLOB FIRMWARE_CPP CONFIGURE_DEPENDS "${FLICKBOOK_DIR}/*.cpp")
# The sketch is compiled as C++ through a generated wrapper (like arduino-cli does).
set(FIRMWARE_INO "${CMAKE_CURRENT_BINARY_DIR}/ebook_reader_ino.cpp")
file(WRITE "${FIRMWARE_INO}.in" "#include <Arduino.h>\n#include \"${FLICKBOOK_DIR}/ebook_reader.ino\"\n")
configure_file("${FIRMWARE_INO}.in" "${FIRMWARE_INO}" COPYONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${FLICKBOOK_DIR}/ebook_reader.ino")

set(TWIN_SOURCES
  core/twin.cpp core/arduino.cpp core/inkplate.cpp core/sdcard.cpp core/http.cpp core/webserver.cpp
  shim/WString.cpp
  app/main.cpp app/runtime.cpp app/agent.cpp app/http_agent.cpp)

if(TWIN_WITH_SDL)
  find_package(SDL2 QUIET)
  if(NOT SDL2_FOUND)
    message(WARNING "SDL2 not found - building without the interactive window (TWIN_WITH_SDL=OFF)")
    set(TWIN_WITH_SDL OFF)
  endif()
endif()

function(flicktwin_target name grayscale)
  add_executable(${name} ${TWIN_SOURCES} ${INK_GRAPHICS_SOURCES} ${UNZIP_SOURCES}
                 "${tinyxml2_SOURCE_DIR}/tinyxml2.cpp" ${FIRMWARE_CPP} ${FIRMWARE_INO})
  # Shim headers must win over anything else (e.g. Arduino.h, SdFat.h).
  target_include_directories(${name} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/shim" "${CMAKE_CURRENT_SOURCE_DIR}/core" "${CMAKE_CURRENT_SOURCE_DIR}/app"
    "${INK_STAGE}" "${INK_STAGE}/graphics" "${INK_STAGE}/graphics/Adafruit_GFX"
    "${arduinojson_SOURCE_DIR}/src" "${tinyxml2_SOURCE_DIR}" "${unziplib_SOURCE_DIR}/src" "${stb_SOURCE_DIR}"
    "${FLICKBOOK_DIR}")
  target_compile_definitions(${name} PRIVATE
    ARDUINO=10819 ARDUINO_INKPLATE6FLICK FLICKBOOK_TWIN=1
    GRAYSCALE=${grayscale} TWIN_GRAYSCALE=${grayscale}
    TWIN_DEFAULT_SD_TEMPLATE="${FLICKBOOK_DIR}/sdcard"
    TWIN_FIXTURE_BOOKS="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/books"
    TWIN_VIEWER_HTML="${CMAKE_CURRENT_SOURCE_DIR}/viewer/index.html")
  target_compile_options(${name} PRIVATE
    -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function
    -Wno-sign-compare -Wno-deprecated-declarations -Wno-write-strings -Wno-narrowing)
  target_link_libraries(${name} PRIVATE m)
  if(TWIN_WITH_SDL)
    target_sources(${name} PRIVATE app/sdl_window.cpp)
    target_compile_definitions(${name} PRIVATE TWIN_WITH_SDL=1)
    if(TARGET SDL2::SDL2)
      target_link_libraries(${name} PRIVATE SDL2::SDL2)
    else()
      target_include_directories(${name} PRIVATE ${SDL2_INCLUDE_DIRS})
      target_link_libraries(${name} PRIVATE ${SDL2_LIBRARIES})
    endif()
  endif()
endfunction()

flicktwin_target(flicktwin 0)
if(TWIN_BUILD_GRAYSCALE)
  flicktwin_target(flicktwin_gray 1)
endif()

message(STATUS "FlickBook firmware: ${FLICKBOOK_DIR}")
message(STATUS "SDL2 window: ${TWIN_WITH_SDL}")
