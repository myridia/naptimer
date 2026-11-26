set(NAPTIMER_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/main.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/cli.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/gui.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/idle.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/json.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/model.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/report.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/store.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/timer.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/util.c"
)

set(NAPTIMER_HEADERS
  "${CMAKE_CURRENT_SOURCE_DIR}/cli.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/gui.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/idle.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/json.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/model.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/report.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/store.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/timer.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/util.h"
)

# NAppGUI's Win32 resource script expects these icons whenever res/ exists.
# Older working copies may not have them yet, so fall back to the SDK icons.
if(WIN32)
  foreach(icon logo256.ico logo48.ico)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/res/${icon}"
       AND EXISTS "${NAPPGUI_ROOT_PATH}/prj/templates/${icon}")
      configure_file(
        "${NAPPGUI_ROOT_PATH}/prj/templates/${icon}"
        "${CMAKE_CURRENT_SOURCE_DIR}/res/${icon}"
        COPYONLY
      )
    endif()
  endforeach()
endif()

# NAppGUI discovers sources with file(GLOB). That works on the normal
# desktop builds, but the Windows pipeline can end up with an empty source
# list and "No SOURCES given to target: naptimer". Feed NAppGUI the known
# files explicitly instead of relying on the glob.
function(nap_source_files targetName dir group publicHeaders)
  if(NOT "${targetName}" STREQUAL "naptimer")
    return()
  endif()

  set(${targetName}_SRCFILES "${NAPTIMER_SOURCES};${NAPTIMER_HEADERS}" CACHE INTERNAL "" FORCE)
  set(${targetName}_SRCSUBDIRS "${CMAKE_CURRENT_SOURCE_DIR}" CACHE INTERNAL "" FORCE)
  set(${targetName}_PUBLICHEADERS "${NAPTIMER_HEADERS}" CACHE INTERNAL "" FORCE)

  source_group("src" FILES ${NAPTIMER_SOURCES} ${NAPTIMER_HEADERS})
  set_source_files_properties(${NAPTIMER_HEADERS} PROPERTIES HEADER_FILE_ONLY ON)
endfunction()

nap_desktop_app(naptimer "" NRC_EMBEDDED)

target_include_directories(naptimer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

if(TARGET SQLite3::SQLite3)
  set(NT_SQLITE_TARGET SQLite3::SQLite3)
else()
  set(NT_SQLITE_TARGET SQLite::SQLite3)
endif()

target_link_libraries(naptimer ${NT_SQLITE_TARGET} ${CMAKE_DL_LIBS})

if(APPLE)
  find_library(APPLICATION_SERVICES_FRAMEWORK ApplicationServices)
  if(APPLICATION_SERVICES_FRAMEWORK)
    target_link_libraries(naptimer ${APPLICATION_SERVICES_FRAMEWORK})
  endif()

  # NAppGUI 1.7 uses UTType in its macOS file dialogs when built against
  # macOS 12+ SDKs. Some installed NAppGUI config files don't add this
  # framework for app targets, so link it explicitly when it is available.
  find_library(UNIFORM_TYPE_IDENTIFIERS_FRAMEWORK UniformTypeIdentifiers)
  if(UNIFORM_TYPE_IDENTIFIERS_FRAMEWORK)
    target_link_libraries(naptimer ${UNIFORM_TYPE_IDENTIFIERS_FRAMEWORK})
  endif()
endif()
