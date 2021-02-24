cmake_minimum_required(VERSION 3.9)
project(tennis C)

include(CheckLibraryExists)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake")

# set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

set(EXTRA_LIBS "")
if (NOT EMSCRIPTEN)
	check_library_exists(m pow "" HAVE_LIBM)
	if(HAVE_LIBM)
	    list(APPEND EXTRA_LIBS "m")
	endif()
endif()

if (EMSCRIPTEN)
	add_compile_options(-s USE_SDL=2)
	add_link_options(-s USE_SDL=2)
else()
	find_package(SDL2 REQUIRED)
	include_directories(${SDL2_INCLUDE_DIR})
endif ()

if (EMSCRIPTEN)
	if (CMAKE_BUILD_TYPE STREQUAL Release)
		configure_file("${CMAKE_SOURCE_DIR}/index.html"
		   "${CMAKE_BINARY_DIR}/index.html"
		   COPYONLY
		)
	else()
		set(CMAKE_EXECUTABLE_SUFFIX ".html")
	endif()
endif()

file(GLOB SOURCE_FILES src/*.c)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARY} ${EXTRA_LIBS})

set_target_properties(${PROJECT_NAME} PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)

if (EMSCRIPTEN)
	set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "index")
endif()
