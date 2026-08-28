message(STATUS "Copying otr files...")

if(EXISTS ${SOURCE_DIR}/oot.o2r)
    execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${SOURCE_DIR}/oot.o2r ${BINARY_DIR}/shipwright/)
    message(STATUS "Copied oot.o2r")
endif()

# Additionally for Windows, copy the otrs to the target dir, side by side with soh.exe
if(SYSTEM_NAME MATCHES "Windows")
    if(EXISTS ${SOURCE_DIR}/oot.o2r)
        execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${SOURCE_DIR}/oot.o2r ${TARGET_DIR})
    endif()
endif()

if(NOT EXISTS ${SOURCE_DIR}/oot.o2r)
    message(FATAL_ERROR "Failed to copy. No OTR files found.")
endif()
