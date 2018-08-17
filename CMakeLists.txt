cmake_minimum_required(VERSION 3.7)

project(ClearScreenExample)

include_directories(src)

find_package(Vulkan REQUIRED)
#find_package(XCB REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/)


# Set preprocessor defines
add_definitions(-DVK_USE_PLATFORM_XCB_KHR)
add_definitions(-D_USE_MATH_DEFINES)
add_definitions(-DNOMINMAX)
add_definitions(-D_CRT_SECURE_NO_WARNINGS)
add_definitions(-DSHADER_DIR=\"${SHADER_DIR}\")


link_libraries(${Vulkan_LIBRARY} xcb)

# function for building single example
function(build_example EXAMPLE_NAME)
	SET(EXAMPLE_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}/examples/${EXAMPLE_NAME})
	message(STATUS "Generating project file for example in ${EXAMPLE_FOLDER}")

	# Main
	SET(MAIN_CPP ${EXAMPLE_FOLDER}/main.cpp)

	add_executable(${EXAMPLE_NAME} ${MAIN_CPP})

endfunction(build_example)



set(EXAMPLES
	clearscreen
	)

file(GLOB SHADERS "${SHADER_DIR}/**/*.glsl")

########################### build ######################

# Build all examples
function(build_all_examples)
	foreach(EXAMPLE ${EXAMPLES})
		build_example(${EXAMPLE})
	endforeach(EXAMPLE)
endfunction(build_all_examples)

build_all_examples()
