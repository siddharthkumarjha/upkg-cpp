cmake_minimum_required(VERSION 3.5)
project(tools CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# git
list(APPEND SRCS ${CMAKE_CURRENT_SOURCE_DIR}/git/git.cpp)
list(APPEND INC  ${CMAKE_CURRENT_SOURCE_DIR}/git/)

# process
list(APPEND SRCS ${CMAKE_CURRENT_SOURCE_DIR}/proc/process.cpp)
list(APPEND INC  ${CMAKE_CURRENT_SOURCE_DIR}/proc)
list(APPEND DEP  cpp-result)

# stream
list(APPEND INC  ${CMAKE_CURRENT_SOURCE_DIR}/stream-helpers)
list(APPEND DEP  pfr)

# logger
list(APPEND SRCS ${CMAKE_CURRENT_SOURCE_DIR}/logger/logging.cpp)
list(APPEND INC  ${CMAKE_CURRENT_SOURCE_DIR}/logger)

add_library(${PROJECT_NAME} STATIC ${SRCS})
target_include_directories(${PROJECT_NAME} PUBLIC ${INC})
target_link_libraries(${PROJECT_NAME} PUBLIC ${DEP})
