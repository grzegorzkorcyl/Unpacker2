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


#Configure Boost
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost 1.50 REQUIRED COMPONENTS unit_test_framework)

if(NOT TARGET Boost::unit_test_framework)
    add_library(Boost::unit_test_framework IMPORTED INTERFACE)
    set_property(TARGET Boost::unit_test_framework PROPERTY
        INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIR})
    set_property(TARGET Boost::unit_test_framework PROPERTY
        INTERFACE_LINK_LIBRARIES ${Boost_LIBRARIES})
endif()
#End of configuration of Boost

# Configure ROOT
## Based on https://cliutils.gitlab.io/modern-cmake/chapters/packages/ROOT.html
find_package(ROOT CONFIG REQUIRED)
include("${ROOT_DIR}/modules/RootNewMacros.cmake")

# fix missing include directories property in versions < ROOT 6.12
set_property(TARGET ROOT::Core PROPERTY
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

file(GLOB HEADERS *.h)
file(GLOB SOURCES *.cc)

set(DICTIONARY_NAME G__Unpacker2)
set(HEADERS_WITH_DICTIONARY_REQUIRED
  EventIII.h
  TDCChannel.h
  )

set(LINKDEF_NAME Unpacker2_LinkDef.h)

include_directories(ROOT_BUG) # Fix bug in NewMacros Root file
ROOT_GENERATE_DICTIONARY(${DICTIONARY_NAME} ${HEADERS_WITH_DICTIONARY_REQUIRED}
  LINKDEF ${LINKDEF_NAME}
  OPTIONS -p)
list(APPEND SOURCES ${DICTIONARY_NAME}.cxx)
## End of configuration of ROOT

add_library(Unpacker2 SHARED src/Unpacker2.cc)
target_include_directories(Unpacker2
                           PUBLIC
                           $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                           $<INSTALL_INTERFACE:include>)
target_link_libraries(Unpacker2 PUBLIC Boost::unit_test_framework ROOT::Core ROOT::Flags_CXX)
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
set(TESTS_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

foreach(dir ${TESTS_SOURCE_DIR})
  file(GLOB UNIT_TEST_SOURCES_TMP ${dir}/*Test.cpp)
  list(APPEND UNIT_TEST_SOURCES ${UNIT_TEST_SOURCES_TMP})
endforeach()

set(TESTS_DIR ${CMAKE_CURRENT_BINARY_DIR}/tests)
file(MAKE_DIRECTORY ${TESTS_DIR})
file(COPY scripts/run_tests.pl DESTINATION ${TESTS_DIR})
file(COPY scripts/parseXML.py DESTINATION ${TESTS_DIR})
foreach(test_source ${UNIT_TEST_SOURCES})
  get_filename_component(test ${test_source} NAME_WE)
  list(APPEND test_binaries ${test}.x)
  add_executable(${test}.x EXCLUDE_FROM_ALL ${test_source})
  set_target_properties(${test}.x PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${TESTS_DIR} )
  target_link_libraries(${test}.x
          Unpacker2
          ${Boost_LIBRARIES}
          )
endforeach()

add_custom_target(unpacker_tests DEPENDS ${test_binaries} Unpacker2) 
