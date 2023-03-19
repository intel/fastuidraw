
project(ngl_generator)

# filter

add_executable(filter filter.cpp)

# extractor

find_package(FLEX REQUIRED)

# Generate the output into the build directory
flex_target(gl_flex gl_flex.fl.cpp ${CMAKE_CURRENT_BINARY_DIR}/gl_flex.cpp)

add_executable(extractor ${FLEX_gl_flex_OUTPUTS} HeaderCreator.cpp)

# Look for HeaderCreator.hpp in the source directory
target_include_directories(extractor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
