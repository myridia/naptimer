cmake_minimum_required(VERSION 3.16)
project(NapTimer)

find_package(nappgui REQUIRED)
find_package(SQLite3 REQUIRED)

include("${NAPPGUI_ROOT_PATH}/prj/NAppProject.cmake")
include("${NAPPGUI_ROOT_PATH}/prj/NAppCompilers.cmake")

nap_config_compiler()
nap_project_desktop_app(naptimer src)
