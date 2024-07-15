cmake_minimum_required(VERSION 3.18)
project(YourProject)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Protobuf CONFIG)
if(NOT Protobuf_FOUND)
  find_package(Protobuf REQUIRED)
endif()
include_directories(${Protobuf_INCLUDE_DIRS})
add_library(pb ${CMAKE_CURRENT_SOURCE_DIR}/protobuf/message.pb.cc)
add_executable(test read_data.cpp) 
target_link_libraries(
    test 
    pb
    protobuf::libprotobuf
)