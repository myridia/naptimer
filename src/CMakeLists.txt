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
