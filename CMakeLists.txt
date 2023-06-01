cmake_minimum_required(VERSION 3.10)

project(simple_rtmp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#set(CMAKE_VERBOSE_MAKEFILE ON)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_EXPORT_COMPILE_COMMANDS "ON")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-omit-frame-pointer -g -O0 -ggdb")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -g -O0 -ggdb")


set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)

find_package(Threads)
set(LINK_LIBS Threads::Threads)


find_package(Boost 1.80 COMPONENTS system thread REQUIRED)

list(APPEND LINK_LIBS ${Boost_LIBRARIES})

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}  SOURCE_FILE)

add_executable(${PROJECT_NAME} ${SOURCE_FILE})

target_link_libraries(${PROJECT_NAME} ${LINK_LIBS})
