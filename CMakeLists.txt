CMAKE_MINIMUM_REQUIRED(VERSION 3.9)
PROJECT(CGIServer)

INCLUDE_DIRECTORIES(src/include/)
SET(SRC_LIST main.cpp src/processpool.cpp src/cgiconn.cpp)
ADD_EXECUTABLE(cgi ${SRC_LIST})