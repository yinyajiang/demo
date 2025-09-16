cmake_minimum_required(VERSION 3.16)

project(ffmpegdemo LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core Widgets Multimedia)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Multimedia)

add_executable(ffmpegdemo
  main.cpp
  mainwindow.cpp
  mainwindow.h
  audiodecoder.cpp
  audiodecoder.h
  audioplayer.cpp
  audioplayer.h
)
target_include_directories(ffmpegdemo PRIVATE .)
target_link_libraries(ffmpegdemo PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Multimedia)

include(GNUInstallDirs)
install(TARGETS ffmpegdemo
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)



# FFmpeg
if(WIN32)
    set(ffmpeg_base_path "${PROJECT_SOURCE_DIR}/../sondkits/3rd/ffmpeg-shared")
    target_include_directories(ffmpegdemo SYSTEM PRIVATE "${ffmpeg_base_path}/include")
    target_link_libraries(ffmpegdemo PRIVATE
        "${ffmpeg_base_path}/lib/avcodec.lib"
        "${ffmpeg_base_path}/lib/avfilter.lib"
        "${ffmpeg_base_path}/lib/avformat.lib"
        "${ffmpeg_base_path}/lib/avutil.lib"
        "${ffmpeg_base_path}/lib/swscale.lib"
        "${ffmpeg_base_path}/lib/swresample.lib"
    )
else()
    set(ffmpeg_base_path "${PROJECT_SOURCE_DIR}/../sondkits/3rd/ffmpeg-shared-mac")
    target_include_directories(ffmpegdemo SYSTEM PRIVATE "${ffmpeg_base_path}/include")
    target_link_libraries(ffmpegdemo PRIVATE
        "${ffmpeg_base_path}/lib/libavcodec.dylib"
        "${ffmpeg_base_path}/lib/libavfilter.dylib"
        "${ffmpeg_base_path}/lib/libavformat.dylib"
        "${ffmpeg_base_path}/lib/libavutil.dylib"
        "${ffmpeg_base_path}/lib/libswscale.dylib"
        "${ffmpeg_base_path}/lib/libswresample.dylib"
    )
endif()


