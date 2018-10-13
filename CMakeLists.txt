cmake_minimum_required (VERSION 2.6)
project (Chat)

macro (use_c99)
  if (CMAKE_VERSION VERSION_LESS "3.1")
    if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
      set (CMAKE_C_FLAGS "-std=gnu99 ${CMAKE_C_FLAGS}")
    endif ()
  else ()
    set (CMAKE_C_STANDARD 99)
  endif ()
endmacro(use_c99)

set (Chat_VERSION_MAJOR 1)
set (Chat_VERSION_MINOR 0)

set (CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories ("${PROJECT_BINARY_DIR}")
add_subdirectory (src bin)
