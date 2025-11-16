cmake_minimum_required(VERSION 3.15)
project(approximate VERSION 1.0 LANGUAGES CXX)

include(FetchContent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

#
# ---- Platform specific settings ----
#

if (WIN32)
    message(STATUS "Configuring for Windows")

    set(LXSDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../LXSDK-000025")
    set(VCPKG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../vcpkg/installed/x64-windows-static")

    set(CGAL_DIR "${VCPKG_ROOT}/share/cgal")

    set(BOOST_INCLUDE_DIR "${VCPKG_ROOT}/include")
    set(EIGEN_INCLUDE_DIR "${VCPKG_ROOT}/include/eigen3")

    set(GMP_LIB   "${VCPKG_ROOT}/lib/gmp.lib")
    set(GMPXX_LIB "${VCPKG_ROOT}/lib/gmpxx.lib")
    set(MPFR_LIB  "${VCPKG_ROOT}/lib/mpfr.lib")

elseif (APPLE)
    message(STATUS "Configuring for macOS")
    message("Home directory: $ENV{HOME}")
    message("Current directory: ${CMAKE_CURRENT_SOURCE_DIR}")

    # Overridable default paths
    set(LXSDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../LXSDK-000025")

    set(CGAL_DIR "/usr/local/opt/cgal")
    set(EIGEN_PATH "/usr/local/include/eigen3")

    set(BOOST_INCLUDE_DIR "/usr/local/include")

    # Homebrew GMP / MPFR
    set(GMP_LIB   "/usr/local/opt/gmp/lib/libgmp.a")
    set(GMPXX_LIB "/usr/local/opt/gmp/lib/libgmpxx.a")
    set(MPFR_LIB  "/usr/local/opt/mpfr/lib/libmpfr.a")

    set(EIGEN_INCLUDE_DIR "${EIGEN_PATH}")
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")

else()
    message(FATAL_ERROR "Unknown or unsupported platform")
endif()

#
# ---- LXSDK ----
#

if (NOT EXISTS "${LXSDK_PATH}/include")
    message(FATAL_ERROR "LXSDK not found at: ${LXSDK_PATH}")
endif()
file(GLOB LXSDK_SOURCES "${LXSDK_PATH}/common/*.cpp")
message("LXSDK PATH: ${LXSDK_PATH}")
add_library(lxsdk STATIC ${LXSDK_SOURCES})
target_include_directories(lxsdk PRIVATE "${LXSDK_PATH}/include")
target_compile_definitions(lxsdk PRIVATE GL_SILENCE_DEPRECATION)

#
# ---- Dependencies ----
#

find_package(CGAL REQUIRED)

#
# ---- Plugin build ----
#

add_library(approximate SHARED
    source/approximate.cpp
    source/tool.cpp
)

target_include_directories(approximate
    PRIVATE
        "${LXSDK_PATH}/include"
        "${BOOST_INCLUDE_DIR}"
        "${EIGEN_INCLUDE_DIR}"
        ${CGAL_INCLUDE_DIRS}
)

target_link_libraries(approximate
    PRIVATE
        lxsdk
        "${GMPXX_LIB}"
        "${GMP_LIB}"
        "${MPFR_LIB}"
        CGAL::CGAL
)

#
# ---- Install ----
#

install(FILES "index.cfg" DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES "index.html" DESTINATION ${CMAKE_INSTALL_PREFIX})
install(DIRECTORY "images" DESTINATION ${CMAKE_INSTALL_PREFIX})
install(TARGETS approximate DESTINATION ${CMAKE_INSTALL_PREFIX}/extra)
