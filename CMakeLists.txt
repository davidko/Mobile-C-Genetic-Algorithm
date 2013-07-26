# The ODE Library
cmake_minimum_required(VERSION 2.6)

project(MCGA)

#set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "~/sys/share/cmake/Modules/")

#find_package(mobilec REQUIRED)
#find_package(embedch REQUIRED)

link_directories($ENV{CHHOME}/extern/lib)
include_directories(../Mobile-C/src/include $ENV{CHHOME}/extern/include)

set(LIBS ${LIBS} mc embedch chsdk dl)

add_executable(server server.c)

target_link_libraries(server ${LIBS})
