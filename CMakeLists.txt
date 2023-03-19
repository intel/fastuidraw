
# CMake 3.10 is required for built-in EGL support in FindOpenGL
# Could be lowered using a manual FindEGL.cmake, if needed
cmake_minimum_required(VERSION 3.10.0)
project(fastuidraw)

set(FASTUIDRAW_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

include(cmake/functions.cmake)

add_subdirectory(src/fastuidraw)

option(FASTUIDRAW_DOC "Build documentation" ON)
option(FASTUIDRAW_DEMOS "Build the demos" OFF)

if(FASTUIDRAW_DOC)

    # TODO: out-of-source documentation build

    find_package(Doxygen REQUIRED)

    set(DOCS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/docs")
    set(INDEX_HTML "${DOCS_DIRECTORY}/doxy/html/index.html")

    add_custom_command(
        OUTPUT ${INDEX_HTML}
        DEPENDS "${DOCS_DIRECTORY}/Doxyfile"
        COMMAND ${DOXYGEN_EXECUTABLE} Doxyfile
        COMMENT "Generating API documentation with Doxygen"
        WORKING_DIRECTORY ${DOCS_DIRECTORY}
        VERBATIM
    )

    add_custom_target(generate_documentation ALL DEPENDS ${INDEX_HTML})

endif()

if(FASTUIDRAW_DEMOS)
    add_subdirectory(demos)
endif()
