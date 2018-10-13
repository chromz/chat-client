
file (GLOB files *.c)
message ("Las files: ${files}")
add_executable (Chat ${files})
