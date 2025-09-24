cmake_minimum_required(VERSION 3.16)

project(sondkits LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


option(BUILD_QT_PLAYER "Build Qt Player" ON)


find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core Widgets Multimedia)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Multimedia)

add_library(sondkits SHARED
  src/decode/audiodecoder.cpp
  src/decode/decodequeue.cpp
  src/encode/audioencoder.cpp
  src/datasource/datasource.cpp
  src/datasource/decodedatasource.cpp
  src/datasource/filedatasource.cpp
  src/datasource/memorydatasource.cpp
  src/datasource/composedatasource.cpp
  src/audiofilter/audioeffectsfilter.cpp
  src/audiofilter/audiothroughfilter.cpp
  src/audioinfo/audioinfo.cpp
  src/audioinfo/bpmfilter.cpp
  src/audioinfo/keyfilter.cpp
  src/audioinfo/normsamplefilter.cpp
  src/common/common.cpp
  src/common/audioutils.cpp
  src/audioplay.cpp
  src/audioplayer.cpp
  src/audioexporter.cpp


  src/decode/audiodecoder.h
  src/decode/decodequeue.h
  src/encode/audioencoder.h
  src/decode/decoder.h
  src/common/common.h
  src/common/audioutils.h
  src/datasource/datasource.h
  src/datasource/decodedatasource.h
  src/datasource/filedatasource.h
  src/datasource/memorydatasource.h
  src/datasource/composedatasource.h
  src/datasource/mixaudio.h
  src/audiofilter/audiofilter.h
  src/audiofilter/audioeffectsfilter.h
  src/audiofilter/audiothroughfilter.h
  src/audioinfo/audioinfo.h
  src/audioinfo/bpmfilter.h
  src/audioinfo/keyfilter.h
  src/audioinfo/normsamplefilter.h
  src/audioplay.h
  src/audioplayer.h
  src/audioexporter.h

)
target_include_directories(sondkits PRIVATE . 
                        ./src 
                        ./src/common
                        ./src/decode
                        ./src/datasource
                        ./src/audiofilter
                        ./src/audioinfo
                        ./src/encode)

target_link_libraries(sondkits PUBLIC Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Multimedia)
target_compile_definitions(sondkits PRIVATE SOUNDKITS_EXPORTS)

include(GNUInstallDirs)
install(TARGETS sondkits
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)



# FFmpeg
# aubio
if(WIN32)
    set(ffmpeg_base_path "${PROJECT_SOURCE_DIR}/3rd/ffmpeg")
    target_include_directories(sondkits SYSTEM PRIVATE "${ffmpeg_base_path}/include")
    target_link_libraries(sondkits PUBLIC
        "${ffmpeg_base_path}/lib/avcodec.lib"
        "${ffmpeg_base_path}/lib/avfilter.lib"
        "${ffmpeg_base_path}/lib/avformat.lib"
        "${ffmpeg_base_path}/lib/avutil.lib"
        "${ffmpeg_base_path}/lib/swscale.lib"
        "${ffmpeg_base_path}/lib/swresample.lib"
    )

    set(aubio_base_path "${PROJECT_SOURCE_DIR}/3rd/aubio")
    target_include_directories(sondkits SYSTEM PRIVATE "${aubio_base_path}/src")
    target_link_libraries(sondkits PUBLIC "${aubio_base_path}/lib/aubio.lib")

    set(keyfinder_base_path "${PROJECT_SOURCE_DIR}/3rd/libkeyfinder")
    target_include_directories(sondkits SYSTEM PRIVATE "${keyfinder_base_path}/src")
    #target_link_libraries(sondkits PUBLIC "${keyfinder_base_path}/lib/keyfinder.lib")

else()
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
        set(ffmpeg_base_path "${PROJECT_SOURCE_DIR}/3rd/ffmpeg-mac-arm64")
        set(aubio_base_path "${PROJECT_SOURCE_DIR}/3rd/aubio-mac-arm64")
        set(keyfinder_base_path "${PROJECT_SOURCE_DIR}/3rd/libkeyfinder-mac-arm64")
    else()
        set(ffmpeg_base_path "${PROJECT_SOURCE_DIR}/3rd/ffmpeg-mac")
        set(aubio_base_path "${PROJECT_SOURCE_DIR}/3rd/aubio-mac")
        set(keyfinder_base_path "${PROJECT_SOURCE_DIR}/3rd/libkeyfinder-mac")
    endif()
    target_include_directories(sondkits SYSTEM PRIVATE "${ffmpeg_base_path}/include")
    target_link_libraries(sondkits PUBLIC
        "${ffmpeg_base_path}/lib/libavcodec.dylib"
        "${ffmpeg_base_path}/lib/libavfilter.dylib"
        "${ffmpeg_base_path}/lib/libavformat.dylib"
        "${ffmpeg_base_path}/lib/libavutil.dylib"
        "${ffmpeg_base_path}/lib/libswscale.dylib"
        "${ffmpeg_base_path}/lib/libswresample.dylib"
    )

    target_include_directories(sondkits SYSTEM PRIVATE "${aubio_base_path}/src")
    target_link_libraries(sondkits PUBLIC "${aubio_base_path}/lib/libaubio.dylib")

    target_include_directories(sondkits SYSTEM PRIVATE "${keyfinder_base_path}/src")
    #target_link_libraries(sondkits PUBLIC "${keyfinder_base_path}/lib/libkeyfinder.dylib")
endif()

# SoundTouch
add_subdirectory(3rd/soundtouch)
target_include_directories(sondkits SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/3rd/soundtouch/include")
target_link_libraries(sondkits PUBLIC SoundTouch)


if(BUILD_QT_PLAYER)
    add_executable(qt-player
        tools/player/main.cpp
        tools/player/mainwindow.cpp
        tools/player/mainwindow.h
    )
    target_include_directories(qt-player PRIVATE  
                        ./src
                        ./tools)
    target_link_libraries(qt-player PRIVATE sondkits Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Multimedia)
endif()



