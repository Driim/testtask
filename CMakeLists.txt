cmake_minimum_required(VERSION 3.2)
project(TestTask)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")

find_package(LibUV REQUIRED)

if (LibEvent_FOUND)
  include_directories(SYSTEM ${LIBEVENT_INCLUDE_DIR})
endif (LibEvent_FOUND)

set(SOURCE_FILES_CLIENT client.c)
set(SOURCE_FILES_SERVER server.c)
add_executable(client ${SOURCE_FILES_CLIENT})
add_executable(server ${SOURCE_FILES_SERVER})
target_link_libraries(server ${LIBEVENT_LIB})