cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/johnny-esp32/main"
    "${CMAKE_CURRENT_LIST_DIR}/johnny-esp32/components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(PROJECT_VER "2026.1.0")
project(johnny_esp32)

set(JCDATA_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/data" CACHE PATH
    "Directory containing RESOURCE.MAP and RESOURCE.001")
set(JCDATA_IMAGE "${CMAKE_BINARY_DIR}/jcdata.bin")

add_custom_command(
    OUTPUT "${JCDATA_IMAGE}"
    COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tools/make_jcdata.py"
            --source "${JCDATA_SOURCE_DIR}"
            --output "${JCDATA_IMAGE}"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/tools/make_jcdata.py"
            "${CMAKE_CURRENT_LIST_DIR}/tools/jcdata.py"
            "${JCDATA_SOURCE_DIR}/RESOURCE.MAP"
            "${JCDATA_SOURCE_DIR}/RESOURCE.001"
    VERBATIM)
add_custom_target(jcdata ALL DEPENDS "${JCDATA_IMAGE}")

partition_table_get_partition_info(jcdata_offset "--partition-name jcdata" "offset")
esptool_py_flash_target_image(flash jcdata "${jcdata_offset}" "${JCDATA_IMAGE}")
