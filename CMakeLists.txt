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

include_directories(third/media-server/librtsp/include)
set(SDP_SRC
    # third/media-server/librtsp/include/sdp.h 
    # third/media-server/librtsp/include/sdp-options.h
    # third/media-server/librtsp/include/sdp-a-fmtp.h
    # third/media-server/librtsp/include/sdp-a-rtpmap.h
    # third/media-server/librtsp/include/sdp-a-webrtc.h
    third/media-server/librtsp/source/sdp.c
    third/media-server/librtsp/source/sdp-options.c
    third/media-server/librtsp/source/sdp-a-fmtp.c
    third/media-server/librtsp/source/sdp-a-rtpmap.c
    third/media-server/librtsp/source/sdp-a-webrtc.c
    )

add_library(sdp STATIC ${SDP_SRC})
list(APPEND LINK_LIBS sdp)

add_subdirectory(third/libsrtp)
add_subdirectory(third/usrsctp)

list(APPEND LINK_LIBS srtp2)
list(APPEND LINK_LIBS usrsctp)

include_directories(third/spdlog/include)

include_directories(webrtc)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

find_package(Boost 1.81 COMPONENTS system filesystem thread REQUIRED)

list(APPEND LINK_LIBS ${Boost_LIBRARIES})

find_package(OpenSSL REQUIRED)

list(APPEND LINK_LIBS OpenSSL::SSL OpenSSL::Crypto)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SOURCE_FILE)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/webrtc WEBRTC_SOURCE_FILE)

add_executable(${PROJECT_NAME} ${SOURCE_FILE} ${WEBRTC_SOURCE_FILE})

target_link_libraries(${PROJECT_NAME} ${LINK_LIBS})
