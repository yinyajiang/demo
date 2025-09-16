cmake_minimum_required(VERSION 3.16)

project(sondkits LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core Widgets Multimedia)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Multimedia)

add_executable(sondkits
  main.cpp
  audiodecoder.cpp
  avutils.cpp
  audioplay.cpp
  audioplayer.cpp
  mainwindow.cpp
  mainwindow.h
  audiodecoder.h
  avutils.h
  audioplay.h
  audioplayer.h
)
target_include_directories(sondkits PRIVATE .)
target_link_libraries(sondkits PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Multimedia)

include(GNUInstallDirs)
install(TARGETS sondkits
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)



# FFmpeg
if(WIN32)
    set(ffmpeg_base_path "${PROJECT_SOURCE_DIR}/3rd/ffmpeg-shared")
    target_include_directories(sondkits SYSTEM PRIVATE "${ffmpeg_base_path}/include")
    target_link_libraries(sondkits PRIVATE
        "${ffmpeg_base_path}/lib/avcodec.lib"
        "${ffmpeg_base_path}/lib/avfilter.lib"
        "${ffmpeg_base_path}/lib/avformat.lib"
        "${ffmpeg_base_path}/lib/avutil.lib"
        "${ffmpeg_base_path}/lib/swscale.lib"
        "${ffmpeg_base_path}/lib/swresample.lib"
    )
else()
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
        set(ffmpeg_base_path "${PROJECT_SOURCE_DIR}/3rd/ffmpeg-shared-mac-arm64")
    else()
        set(ffmpeg_base_path "${PROJECT_SOURCE_DIR}/3rd/ffmpeg-shared-mac")
    endif()
    target_include_directories(sondkits SYSTEM PRIVATE "${ffmpeg_base_path}/include")
    target_link_libraries(sondkits PRIVATE
        "${ffmpeg_base_path}/lib/libavcodec.dylib"
        "${ffmpeg_base_path}/lib/libavfilter.dylib"
        "${ffmpeg_base_path}/lib/libavformat.dylib"
        "${ffmpeg_base_path}/lib/libavutil.dylib"
        "${ffmpeg_base_path}/lib/libswscale.dylib"
        "${ffmpeg_base_path}/lib/libswresample.dylib"
    )
endif()

# SoundTouch
#add_subdirectory(3rd/soundtouch)
#target_link_libraries(sondkits PRIVATE SoundTouch)
