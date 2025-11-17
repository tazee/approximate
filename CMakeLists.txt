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
    set(LOCAL_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../vcpkg/installed/x64-windows-static")

    set(CGAL_DIR "${LOCAL_LIB_DIR}/share/cgal")
    set(Boost_INCLUDE_DIR "${LOCAL_LIB_DIR}/include")
    set(EIGEN_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../eigen-3.4.0")
    set(GMP_INCLUDE_DIR "${LOCAL_LIB_DIR}/include")
    set(MPFR_INCLUDE_DIR "${LOCAL_LIB_DIR}/include")

    set(GMP_LIBRARIES ${LOCAL_LIB_DIR}/lib/gmpxx.lib ${LOCAL_LIB_DIR}/lib/gmp.lib)
    set(MPFR_LIBRARIES ${LOCAL_LIB_DIR}/lib/mpfr.lib)

    set(CMAKE_PREFIX_PATH "$(LOCAL_LIB_DIR)/cgal")

elseif (APPLE)
    message(STATUS "Configuring for macOS")
    message("Current directory: ${CMAKE_CURRENT_SOURCE_DIR}")

    # Overridable default paths
    set(LXSDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../LXSDK-000025")
    set(LOCAL_LIB_DIR "/usr/local")

    set(CGAL_DIR "$(LOCAL_LIB_DIR)/opt/cgal")
    set(Boost_INCLUDE_DIR "${LOCAL_LIB_DIR}/include")
    set(EIGEN_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../eigen-3.4.0")

    set(GMP_LIBRARIES ${LOCAL_LIB_DIR}/opt/gmp/lib/libgmpxx.a ${LOCAL_LIB_DIR}/opt/gmp/lib/libgmp.a)
    set(MPFR_LIBRARIES ${LOCAL_LIB_DIR}/opt/mpfr/lib/libmpfr.a)

    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")

else()
    message(FATAL_ERROR "Unknown or unsupported platform")
endif()

#
# ---- LXSDK ----
#
if (NOT LXSDK_PATH)
    FetchContent_Declare( 
        lxsdk
        URL https://thefoundry.s3.us-east-1.amazonaws.com/products/modo/16.1v5/lxsdk16.1v5_692025.zip
    )
    FetchContent_Populate(lxsdk)
    message(STATUS "FetchContent lxsdk directory: ${lxsdk_SOURCE_DIR}")
    set(LXSDK_PATH ${lxsdk_SOURCE_DIR})
endif()

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
if (NOT CGAL_DIR)
    FetchContent_Declare( 
        cgal
        GIT_REPOSITORY https://github.com/CGAL/cgal.git
        GIT_TAG        v6.0.1
    )
    FetchContent_Populate(cgal)
    message(STATUS "FetchContent cgal directory: ${cgal_SOURCE_DIR}")
    set(CGAL_DIR ${cgal_SOURCE_DIR})
endif()

if (NOT EIGEN_INCLUDE_DIR)
    FetchContent_Declare( 
        eigen
        GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
        GIT_TAG        3.4.0
    )
    FetchContent_Populate(eigen)
    message(STATUS "FetchContent eigen directory: ${eigen_SOURCE_DIR}")
    set(EIGEN_INCLUDE_DIR ${eigen_SOURCE_DIR})
endif()

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
        "${Boost_INCLUDE_DIR}"
        "${EIGEN_INCLUDE_DIR}"
        ${CGAL_INCLUDE_DIRS}
)

target_link_libraries(approximate
    PRIVATE
        lxsdk
        "${GMP_LIBRARIES}"
        "${MPFR_LIBRARIES}"
        CGAL::CGAL
)

#
# ---- Install ----
#

install(FILES "index.cfg" DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES "index.html" DESTINATION ${CMAKE_INSTALL_PREFIX})
install(DIRECTORY "images" DESTINATION ${CMAKE_INSTALL_PREFIX})
install(TARGETS approximate DESTINATION ${CMAKE_INSTALL_PREFIX}/extra)
