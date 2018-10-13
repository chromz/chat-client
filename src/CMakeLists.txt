
file (GLOB files *.c)

find_package (PkgConfig REQUIRED)
pkg_check_modules (GTK3 REQUIRED gtk+-3.0)
pkg_check_modules (JSONC REQUIRED json-c)
include_directories (
	${GTK3_INCLUDE_DIRS}
	${JSONC_INCLUDE_DIRS}
	)
link_directories (${GTK3_LIBRARY_DIRS})
add_definitions (${GTK3_CFLAGS_OTHER})
file (COPY assets DESTINATION ${CMAKE_BINARY_DIR}/bin)



add_executable (Chat ${files})
use_c99 ()
target_link_libraries (Chat ${GTK3_LIBRARIES} ${JSONC_LIBRARIES})
