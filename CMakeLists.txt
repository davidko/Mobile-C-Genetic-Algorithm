# The ODE Library
cmake_minimum_required(VERSION 2.6)

project(MCGA)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "~/sys/share/cmake/Modules/")

find_package(mobilec REQUIRED)
find_package(embedch REQUIRED)

set(LIBS ${LIBS} ${MOBILEC_LIBRARIES} ${EMBEDCH_LIBRARIES})
include_directories(${MOBILEC_INCLUDE_DIRS} ${EMBEDCH_INCLUDE_DIRS})

add_executable(server server.c)

target_link_libraries(server ${LIBS})
