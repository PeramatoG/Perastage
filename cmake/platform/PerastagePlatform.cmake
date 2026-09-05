# Dispatch target-level build configuration to the active platform owner.
if(WIN32)
    include("${CMAKE_CURRENT_LIST_DIR}/PerastageWindows.cmake")
elseif(APPLE)
    include("${CMAKE_CURRENT_LIST_DIR}/PerastageMacOS.cmake")
elseif(UNIX)
    include("${CMAKE_CURRENT_LIST_DIR}/PerastageLinux.cmake")
endif()
