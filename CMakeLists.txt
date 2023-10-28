cmake_minimum_required(VERSION 3.10)

project(simple_rtmp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#set(CMAKE_VERBOSE_MAKEFILE ON)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-omit-frame-pointer -g -O0 -ggdb")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -g -O0 -ggdb")


set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)

find_package(Threads)
set(LINK_LIBS Threads::Threads)

find_package(PkgConfig REQUIRED)

include_directories(third/http_parse)
aux_source_directory(third/http_parse HTTP_PARSE_SRC)
add_library(http_parse STATIC ${HTTP_PARSE_SRC})
list(APPEND LINK_LIBS http_parse)

include_directories(third/media-server/librtp/include)
aux_source_directory(third/media-server/librtp/source RTP_SRC)
aux_source_directory(third/media-server/librtp/payload RTP_SRC)
add_library(rtp STATIC ${RTP_SRC})
target_compile_options(rtp PRIVATE -DRTP_MPEG4_GENERIC_SKIP_ADTS)
list(APPEND LINK_LIBS rtp)

include_directories(third/media-server/librtmp/include)
aux_source_directory(third/media-server/librtmp/source RTMP_SRC)
add_library(rtmp STATIC ${RTMP_SRC})
list(APPEND LINK_LIBS rtmp)

include_directories(third/media-server/libflv/include)
aux_source_directory(third/media-server/libflv/source FLV_SRC)
add_library(flv STATIC ${FLV_SRC})
list(APPEND LINK_LIBS flv)

include_directories(third/spdlog/include)


include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_DEBUG_LIBS ON)
set(Boost_USE_RELEASE_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

find_package(Boost 1.81 COMPONENTS system filesystem thread REQUIRED)

list(APPEND LINK_LIBS ${Boost_LIBRARIES})

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SOURCE_FILE)

add_executable(${PROJECT_NAME} ${SOURCE_FILE})

target_link_libraries(${PROJECT_NAME} ${LINK_LIBS})
