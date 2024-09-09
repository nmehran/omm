cmake_minimum_required(VERSION 3.29)
project(omm)

set(CMAKE_CXX_STANDARD 20)

add_library(omm SHARED library.cpp)
