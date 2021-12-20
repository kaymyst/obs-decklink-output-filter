cmake_minimum_required(VERSION 3.10)

# Change obs-plugintemplate to your plugin's name in a machine-readable format
# (e.g.: obs-myawesomeplugin) and set
project(obs-decklink-output-filter VERSION 1.0.0)

# Replace `Your Name Here` with the name (yours or your organization's) you want
# to see as the author of the plugin (in the plugin's metadata itself and in the installers)
set(PLUGIN_AUTHOR "cg2121")

# Replace `com.example.obs-plugin-template` with a unique Bundle ID for macOS releases
# (used both in the installer and when submitting the installer for notarization)
set(MACOS_BUNDLEID "com.obs-plugins.decklink-output-filter")

# TAKE NOTE: No need to edit things past this point

set(CMAKE_PREFIX_PATH "${QTDIR}")
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

# In case you need C++
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(LIB_OBS libobs)


# build obs lib
if (WIN32)
    # Windows
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    if (CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(PLAT "32bit")
        set(LINK_PLAT "x86")
    elseif (CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(PLAT "64bit")
        set(LINK_PLAT "x64")
    else ()
        message(FATAL_ERROR "Unsupported Architecture!")
    endif ()

    if (NOT DEFINED OBS_SRC_DIR)
        set(OBS_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/obs-src")
    endif ()
    if (NOT DEFINED OBS_BIN_DIR)
        set(OBS_BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/obs-bin")
    endif ()

    if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
        if (${PORTABLE_MODE})
            set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/obs-bin" CACHE PATH "" FORCE)
        else ()
            set(CMAKE_INSTALL_PREFIX "$ENV{ProgramData}/obs-studio" CACHE PATH "" FORCE)
        endif ()
    endif ()

    execute_process(COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/pexports.exe"
        "${OBS_BIN_DIR}/bin/${PLAT}/obs.dll"
        OUTPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/libobs.def"
        COMMAND_ECHO STDOUT
    )
    execute_process(COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/pexports.exe"
        "${OBS_BIN_DIR}/bin/${PLAT}/obs-frontend-api.dll"
        OUTPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/obs-frontend-api.def"
        COMMAND_ECHO STDOUT
    )
    execute_process(COMMAND ${CMAKE_LINKER}
        "/lib" "/MACHINE:${LINK_PLAT}"
        "/def:${CMAKE_CURRENT_BINARY_DIR}/libobs.def"
        "/out:${CMAKE_CURRENT_BINARY_DIR}/libobs.lib"
    )
    execute_process(COMMAND ${CMAKE_LINKER}
        "/lib" "/MACHINE:${LINK_PLAT}"
        "/def:${CMAKE_CURRENT_BINARY_DIR}/obs-frontend-api.def"
        "/out:${CMAKE_CURRENT_BINARY_DIR}/obs-frontend-api.lib"
    )
elseif (APPLE)
    # macOS
    find_library(_LIB_OBS "${LIB_OBS}.0.dylib" PATHS ${OBS_BIN_DIR} REQUIRED)
    set(LIB_OBS "${_LIB_OBS}")
elseif (UNIX)
    # Linux
    find_package(LibObs REQUIRED)
else ()
    message(FATAL_ERROR "Unsupported OS!")
endif ()

if (WIN32 OR APPLE)
include(external/FindLibObs.cmake)
endif()

find_package(LibObs REQUIRED)
find_package(Qt5 REQUIRED COMPONENTS Core Widgets)

configure_file(
    src/plugin-macros.h.in
    ../src/plugin-macros.generated.h
)
configure_file(
    installer/installer-macOS.pkgproj.in
    ../installer/installer-macOS.generated.pkgproj
)
configure_file(
    installer/installer-Windows.iss.in
    ../installer/installer-Windows.generated.iss
)

configure_file(
    ci/ci_includes.sh.in
    ../ci/ci_includes.generated.sh
)
configure_file(
    ci/ci_includes.cmd.in
    ../ci/ci_includes.generated.cmd
)

set(PLUGIN_SOURCES
	src/plugin-main.c)

set(PLUGIN_HEADERS
	src/plugin-macros.generated.h)

# --- Platform-independent build settings ---
add_library(${CMAKE_PROJECT_NAME} MODULE ${PLUGIN_SOURCES} ${PLUGIN_HEADERS})

include_directories(
	"${LIBOBS_INCLUDE_DIR}/../UI/obs-frontend-api"
	${Qt5Core_INCLUDES}
	${Qt5Widgets_INCLUDES}
)

target_link_libraries(${CMAKE_PROJECT_NAME}
	libobs
	Qt5::Core
	Qt5::Widgets
)

# --- End of section ---

# --- Windows-specific build settings and tasks ---
if(WIN32)
	if(NOT DEFINED OBS_FRONTEND_LIB)
		set(OBS_FRONTEND_LIB "OBS_FRONTEND_LIB-NOTFOUND" CACHE FILEPATH "OBS frontend library")
		message(FATAL_ERROR "Could not find OBS Frontend API library !")
	endif()

    # Enable Multicore Builds and disable FH4 (to not depend on VCRUNTIME140_1.DLL when building with VS2019)
    if (MSVC)
        add_definitions(/MP /d2FH4-)
    endif()

	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(ARCH_NAME "64bit")
		set(OBS_BUILDDIR_ARCH "build64")
	else()
		set(ARCH_NAME "32bit")
		set(OBS_BUILDDIR_ARCH "build32")
	endif()

	include_directories(
		"${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/UI"
	)

	target_link_libraries(${CMAKE_PROJECT_NAME}
		"${OBS_FRONTEND_LIB}"
    )

	# --- Release package helper ---
	# The "release" folder has a structure similar OBS' one on Windows
	set(RELEASE_DIR "${PROJECT_SOURCE_DIR}/release")

    add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
        # If config is Release or RelWithDebInfo, package release files
        COMMAND if $<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>==1 (
            "${CMAKE_COMMAND}" -E make_directory
                "${RELEASE_DIR}/data/obs-plugins/${CMAKE_PROJECT_NAME}"
                "${RELEASE_DIR}/obs-plugins/${ARCH_NAME}"
        )

        COMMAND if $<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>==1 (
            "${CMAKE_COMMAND}" -E copy_directory
                "${PROJECT_SOURCE_DIR}/data"
                "${RELEASE_DIR}/data/obs-plugins/${CMAKE_PROJECT_NAME}"
        )

        COMMAND if $<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>==1 (
            "${CMAKE_COMMAND}" -E copy
                "$<TARGET_FILE:${CMAKE_PROJECT_NAME}>"
                "${RELEASE_DIR}/obs-plugins/${ARCH_NAME}"
        )

        # If config is RelWithDebInfo, copy the pdb file
        COMMAND if $<CONFIG:RelWithDebInfo>==1 (
            "${CMAKE_COMMAND}" -E copy
                "$<TARGET_PDB_FILE:${CMAKE_PROJECT_NAME}>"
                "${RELEASE_DIR}/obs-plugins/${ARCH_NAME}"
        )

        # Copy to obs-studio dev environment for immediate testing
        COMMAND if $<CONFIG:Debug>==1 (
            "${CMAKE_COMMAND}" -E copy
                "$<TARGET_FILE:${CMAKE_PROJECT_NAME}>"
                "${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/rundir/$<CONFIG>/obs-plugins/${ARCH_NAME}"
        )

        COMMAND if $<CONFIG:Debug>==1 (
            "${CMAKE_COMMAND}" -E copy
                "$<TARGET_PDB_FILE:${CMAKE_PROJECT_NAME}>"
                "${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/rundir/$<CONFIG>/obs-plugins/${ARCH_NAME}"
        )

        COMMAND if $<CONFIG:Debug>==1 (
            "${CMAKE_COMMAND}" -E make_directory
                "${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/rundir/$<CONFIG>/data/obs-plugins/${CMAKE_PROJECT_NAME}"
        )

        COMMAND if $<CONFIG:Debug>==1 (
            "${CMAKE_COMMAND}" -E copy_directory
                "${PROJECT_SOURCE_DIR}/data"
                "${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/rundir/$<CONFIG>/data/obs-plugins/${CMAKE_PROJECT_NAME}"
        )
    )
	# --- End of sub-section ---

endif()
# --- End of section ---

# --- Linux-specific build settings and tasks ---
if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)

	set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES PREFIX "")
	target_link_libraries(${CMAKE_PROJECT_NAME} obs-frontend-api)

	file(GLOB locale_files data/locale/*.ini)

	install(TARGETS ${CMAKE_PROJECT_NAME}
		LIBRARY DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}/obs-plugins")

	install(FILES ${locale_files}
		DESTINATION "${CMAKE_INSTALL_FULL_DATAROOTDIR}/obs/obs-plugins/${CMAKE_PROJECT_NAME}/locale")
endif()
# --- End of section ---

# -- OS X specific build settings and tasks --
if(APPLE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -fvisibility=default")

	set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES PREFIX "")
	target_link_libraries(${CMAKE_PROJECT_NAME} "${OBS_FRONTEND_LIB}")
endif()
# -- End of section --
