

function(prepend_path OUTPUT PREFIX)

    set(TEMP_LIST "")

    foreach(PATH ${ARGN})
        list(APPEND TEMP_LIST "${PREFIX}/${PATH}")
    endforeach(PATH)

    set(${OUTPUT} "${TEMP_LIST}" PARENT_SCOPE)

endfunction(prepend_path)

function(assemble_resource_string_sources RESOURCE_STRINGS OUTPUT)

    set(TEMP_LIST "")

    foreach(RESOURCE_STRING_FILE ${RESOURCE_STRINGS})

        get_filename_component(BARE_NAME ${RESOURCE_STRING_FILE} NAME)

        set(CPP_FILENAME "${BARE_NAME}.cpp")

        add_custom_command(
            OUTPUT ${CPP_FILENAME}
            COMMAND shell_scripts/fastuidraw-create-resource-cpp-file.sh "${CMAKE_CURRENT_SOURCE_DIR}/${RESOURCE_STRING_FILE}" ${BARE_NAME} ${CMAKE_CURRENT_BINARY_DIR}
            WORKING_DIRECTORY ${FASTUIDRAW_ROOT_DIR}
            COMMENT "Generating ${CPP_FILENAME} from ${BARE_NAME}"
            VERBATIM
        )

        list(APPEND TEMP_LIST ${CPP_FILENAME})

    endforeach(RESOURCE_STRING_FILE)

    set(${OUTPUT} "${TEMP_LIST}" PARENT_SCOPE)

endfunction(assemble_resource_string_sources)
