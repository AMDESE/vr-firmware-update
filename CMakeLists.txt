cmake_minimum_required(VERSION 2.8.10)
project(vr-update CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
include(GNUInstallDirs)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/inc)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(SRC_FILES src/main.cpp
              src/vr_update.cpp
              src/vr_update_renesas_gen3.cpp 
              src/vr_update_renesas_gen2.cpp 
              src/vr_update_renesas_patch.cpp
              src/vr_update_xdpe_patch.cpp 
              src/vr_update_infineon_xdpe.cpp 
              src/vr_update_infineon_tda.cpp
              src/vr_update_mps.cpp
              src/vr_update_mps285x.cpp )

# import sdbusplus
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDBUSPLUSPLUS sdbusplus REQUIRED)
include_directories(${SDBUSPLUSPLUS_INCLUDE_DIRS})
link_directories(${SDBUSPLUSPLUS_LIBRARY_DIRS})
find_program(SDBUSPLUSPLUS sdbus++)

# import phosphor-logging
find_package(PkgConfig REQUIRED)
pkg_check_modules(LOGGING phosphor-logging REQUIRED)
include_directories(${LOGGING_INCLUDE_DIRS})
link_directories(${LOGGING_LIBRARY_DIRS})

# phosphor-dbus-interfaces
find_package(PkgConfig REQUIRED)
pkg_check_modules(DBUSINTERFACE phosphor-dbus-interfaces REQUIRED)
include_directories(${DBUSINTERFACE_INCLUDE_DIRS})
link_directories(${DBUSINTERFACE_LIBRARY_DIRS})


add_executable(${PROJECT_NAME} ${SRC_FILES})
target_link_libraries(${PROJECT_NAME} ${DBUSINTERFACE_LIBRARIES} )
target_link_libraries(${PROJECT_NAME} "${SDBUSPLUSPLUS_LIBRARIES} -lstdc++fs -lphosphor_dbus")
target_link_libraries(${PROJECT_NAME}  -li2c -lpthread -lm  )

install (TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})

install (FILES ${SERVICE_FILES} DESTINATION /lib/systemd/system/)

message(STATUS "Toolchain file defaulted to ......'${CMAKE_INATLL_BINDIR}'")
