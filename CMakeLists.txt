cmake_minimum_required(VERSION 3.5)
project(upkg CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(BUILD_TESTING OFF)
add_subdirectory(external/cpp-result)
add_subdirectory(external/pfr)

add_subdirectory(tools)

#upkg
list(APPEND SRCS ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)
list(APPEND DEP  tools)

add_executable(${PROJECT_NAME} ${SRCS})
target_include_directories(${PROJECT_NAME} PUBLIC ${INC})
target_link_libraries(${PROJECT_NAME} PUBLIC ${DEP})
