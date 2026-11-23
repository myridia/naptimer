nap_desktop_app(naptimer "" NRC_EMBEDDED)

target_include_directories(naptimer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(naptimer SQLite::SQLite3 ${CMAKE_DL_LIBS})

if(APPLE)
  find_library(APPLICATION_SERVICES Framework ApplicationServices)
  if(APPLICATION_SERVICES)
    target_link_libraries(naptimer ${APPLICATION_SERVICES})
  endif()
endif()
