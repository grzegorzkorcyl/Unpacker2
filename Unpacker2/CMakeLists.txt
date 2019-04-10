cmake_minimum_required(VERSION 3.1...3.14)

if(${CMAKE_VERSION} VERSION_LESS 3.14)
    cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
else()
    cmake_policy(VERSION 3.14)
endif()

# Force out-of-source build
file(TO_CMAKE_PATH "${PROJECT_BINARY_DIR}/CMakeLists.txt" LOC_PATH)
if(EXISTS "${LOC_PATH}")
    message(FATAL_ERROR "You cannot build in a source directory (or any directory with a CMakeLists.txt file). Please make a build subdirectory. Feel free to remove CMakeCache.txt and CMakeFiles.")
endif()

project(Unpacker2 VERSION 1.0.0
                  DESCRIPTION "Unpacker2 module"
                  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11) # maybe not best solutio to set it globally? upgrate minimal cmake version to 3.8 or find another way to set it only for target with interface
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_PLATFORM_INDEPENDENT_CODE ON)

set(default_build_type "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
      STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Include cmake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")


include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    Unpacker2ConfigVersion.cmake
    VERSION ${PACKAGE_VERSION}
    COMPATIBILITY AnyNewerVersion
    )

# Configure ROOT
## Based on https://cliutils.gitlab.io/modern-cmake/chapters/packages/ROOT.html
find_package(ROOT CONFIG QUIET)
message(Root: ${ROOT_USE_FILE})
if(ROOT_FOUND)
  message(STATUS "ROOT (version ${ROOT_VERSION}) was found using ROOTConfig")
else()
  # Try to find root 5
  find_package(ROOT 5 QUIET)

  if(ROOT_FOUND)
    message(STATUS "ROOT (version ${ROOT_VERSION}) was found using ROOTConfig")
  else()
    # try to locate ROOT using ROOTSYS and root-config in case ROOTSYS is not set
    set(root_prefix $ENV{ROOTSYS})
    if(root_prefix)
      list(APPEND CMAKE_MODULE_PATH ${root_prefix}/etc/cmake/)
    else()
      execute_process(COMMAND root-config --etcdir OUTPUT_VARIABLE ROOT_ETCDIR OUTPUT_STRIP_TRAILING_WHITESPACE)
      list(APPEND CMAKE_MODULE_PATH ${ROOT_ETCDIR}/cmake)
    endif()
    find_package(ROOT 5 QUIET)
    if(ROOT_FOUND)
      message(STATUS "ROOT (version ${ROOT_VERSION}) was found using FindROOT (legacy mode)")
    else()
      # as last resort, use the bundled FindROOT module
      list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
      find_package(ROOT 5 REQUIRED)
      if(ROOT_FOUND)
        message(WARNING "ROOT (version ${ROOT_VERSION}) was only found using fallback mode)")
      endif()
    endif()
  endif()
endif()

if(NOT TARGET ROOT::Core)
    add_library(ROOT::Core IMPORTED INTERFACE)
    set_property(TARGET ROOT::Core PROPERTY
        INTERFACE_INCLUDE_DIRECTORIES ${ROOT_INCLUDE_DIR})
    set_property(TARGET ROOT::Core PROPERTY
        INTERFACE_LINK_LIBRARIES ${ROOT_LIBRARIES})
endif()

if (NOT ROOT_VERSION VERSION_LESS 6)
  include("${ROOT_DIR}/modules/RootNewMacros.cmake")
endif()
# fix missing include directories property in versions < ROOT 6.12
set_target_properties(ROOT::Core PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${ROOT_INCLUDE_DIRS}")


add_library(ROOT::Flags_CXX IMPORTED INTERFACE)

# Fix for ROOT_CXX_FLAGS not actually being a CMake list
separate_arguments(ROOT_CXX_FLAGS)
set_property(TARGET ROOT::Flags_CXX APPEND PROPERTY
    INTERFACE_COMPILE_OPTIONS ${ROOT_CXX_FLAGS})

# Add definitions
separate_arguments(ROOT_DEFINITIONS)
foreach(_flag ${ROOT_EXE_LINKER_FLAG_LIST})
    # Remove -D or /D if present
    string(REGEX REPLACE [=[^[-//]D]=] "" _flag ${_flag})
    set_property(TARGET ROOT::Flags APPEND PROPERTY INTERFACE_LINK_LIBRARIES ${_flag})
endforeach()

# This also fixes a bug in the linker flags
string(REPLACE "-L " "-L" ROOT_EXE_LINKER_FLAGS "${ROOT_EXE_LINKER_FLAGS}")
separate_arguments(ROOT_EXE_LINKER_FLAGS)
set_property(TARGET ROOT::Flags_CXX APPEND PROPERTY
    INTERFACE_LINK_LIBRARIES ${ROOT_EXE_LINKER_FLAGS})

set(DICTIONARY_NAME G__Unpacker2)
set(HEADERS_WITH_DICTIONARY_REQUIRED
  include/EventIII.h
  include/TDCChannel.h
  )

set(LINKDEF_NAME Unpacker2_LinkDef.h)

include_directories(ROOT_BUG) # Fix bug in NewMacros Root file
ROOT_GENERATE_DICTIONARY(${DICTIONARY_NAME} ${HEADERS_WITH_DICTIONARY_REQUIRED}
  LINKDEF ${LINKDEF_NAME}
  OPTIONS -p)
## End of configuration of ROOT

# Set all sources
set(SOURCES src/Unpacker2.cc src/EventIII.cc src/TDCChannel.cc ${DICTIONARY_NAME}.cxx)

add_library(Unpacker2 SHARED ${SOURCES})
add_library(Unpacker2::Unpacker2 ALIAS Unpacker2)
target_include_directories(Unpacker2
                           PUBLIC
                           $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                           $<INSTALL_INTERFACE:include>)
target_link_libraries(Unpacker2 PUBLIC ROOT::Hist ROOT::Tree ROOT::Flags_CXX)
set_target_properties(Unpacker2 PROPERTIES LINKER_LANGUAGE CXX)

include(CheckIPOSupported) # only avaiable in 3.9+
check_ipo_supported(RESULT IPOresult)
if(IPOresult)
  set_target_properties(Unpacker2 PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()


install(TARGETS Unpacker2
        EXPORT Unpacker2
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include
        )

install(EXPORT Unpacker2
        FILE Unpacker2Targets.cmake
        NAMESPACE Unpacker2::
        DESTINATION lib/cmake/Unpacker2
         )

configure_file(Unpacker2Config.cmake.in Unpacker2Config.cmake @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/Unpacker2Config.cmake"
              "${CMAKE_CURRENT_BINARY_DIR}/Unpacker2ConfigVersion.cmake"
        DESTINATION lib/cmake/Unpacker2
        )

#unit tests
option(PACKAGE_TESTS "Build the tests" ON)
if(PACKAGE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

