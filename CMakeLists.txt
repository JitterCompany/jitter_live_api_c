cmake_minimum_required(VERSION 3.5.0 FATAL_ERROR)


set(CPM_MODULE_NAME jitter_live_api_c)

#-----------------------------------------------------------------------
# Required CPM Setup - no need to modify - See: https://github.com/iauns/cpm
#-----------------------------------------------------------------------
set(CPM_LIB_TARGET_NAME ${CPM_MODULE_NAME})

if ((DEFINED CPM_DIR) AND (DEFINED CPM_UNIQUE_ID) AND (DEFINED CPM_TARGET_NAME))
    set(CPM_LIB_TARGET_NAME ${CPM_TARGET_NAME})
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CPM_DIR})
    include(CPM)
else()
    set (CPM_DIR "${CMAKE_CURRENT_BINARY_DIR}/cpm-packages" CACHE TYPE STRING)
    find_package(Git)
    if(NOT GIT_FOUND)
        message(FATAL_ERROR "CPM requires Git.")
    endif()
    if (NOT EXISTS ${CPM_DIR}/CPM.cmake)
        message(STATUS "Cloning repo (https://github.com/iauns/cpm)")
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" clone https://github.com/iauns/cpm ${CPM_DIR}
            RESULT_VARIABLE error_code
            OUTPUT_QUIET ERROR_QUIET)
        if(error_code)
            message(FATAL_ERROR "CPM failed to get the hash for HEAD")
        endif()
    endif()
    include(${CPM_DIR}/CPM.cmake)
endif()

#------------------------------------------------------------------------------
# Dependencies
#------------------------------------------------------------------------------

# mqtt_c is 'exported': this makes it available to the paren project.
# this is required because we depend on mqtt_c in live_api.h
CPM_AddModule("mqtt_c"
    GIT_REPOSITORY "https://github.com/JitterCompany/mqtt_c.git"
    GIT_TAG "0.6.0"
    EXPORT_MODULE TRUE)

CPM_AddModule("c_utils"
    GIT_REPOSITORY "https://github.com/JitterCompany/c_utils.git"
    GIT_TAG "1.4.5")

CPM_AddModule("cpm_jsmn"
    GIT_REPOSITORY "https://github.com/JitterCompany/cpm_jsmn.git"
    GIT_TAG "1.0.0")


CPM_InitModule(${CPM_MODULE_NAME})

#------------------------------------------------------------------------------
# Project sources / settings
#------------------------------------------------------------------------------

file(GLOB SOURCES
    ${CPM_MODULE_NAME}/src/*.c
    ${CPM_MODULE_NAME}/src/*/*.c
    )

include_directories("${CPM_MODULE_NAME}" 
                    "${CPM_MODULE_NAME}/src")

add_library(${CPM_LIB_TARGET_NAME} ${SOURCES})
target_link_libraries(${CPM_LIB_TARGET_NAME} ${CPM_LIBRARIES})
target_link_libraries(${CPM_LIB_TARGET_NAME} m)

