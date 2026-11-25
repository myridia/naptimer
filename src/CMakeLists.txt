nap_desktop_app(naptimer "" NRC_EMBEDDED)

target_include_directories(naptimer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

if(TARGET SQLite3::SQLite3)
  set(NT_SQLITE_TARGET SQLite3::SQLite3)
else()
  set(NT_SQLITE_TARGET SQLite::SQLite3)
endif()

target_link_libraries(naptimer ${NT_SQLITE_TARGET} ${CMAKE_DL_LIBS})

if(APPLE)
  find_library(APPLICATION_SERVICES Framework ApplicationServices)
  if(APPLICATION_SERVICES)
    target_link_libraries(naptimer ${APPLICATION_SERVICES})
  endif()
endif()
