include(CMakeFindDependencyMacro)

# Same syntax as find_package
find_dependency(ROOT CONFIG REQUIRED)
# NOTE Had to use find_package because find_dependency does not support COMPONENTS or MODULE until 3.8.0
find_package(Boost 1.50 REQUIRED COMPONENTS unit_test_framework)

# Add the targets file
include("${CMAKE_CURRENT_LIST_DIR}/Unpacker2Targets.cmake")
