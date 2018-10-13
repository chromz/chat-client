cmake_minimum_required (VERSION 3.0)
project (Chat)

set (Chat_VERSION_MAJOR 1)
set (Chat_VERSION_MINOR 0)


include_directories("${PROJECT_BINARY_DIR}")
add_subdirectory(src build)
