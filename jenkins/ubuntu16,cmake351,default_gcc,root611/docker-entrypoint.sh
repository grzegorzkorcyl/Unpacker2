#!/bin/bash
function executeCommand {
    $@
    rc=$?; if [[ $rc != 0 ]]; then exit $rc; fi
    echo "Exit code[" $@ "]: $rc"
}

executeCommand "mkdir -p build"
executeCommand "cd build"
executeCommand "export CMAKE_LIBRARY_PATH=$CMAKE_LIBRARY_PATH:/framework-dependencies/lib"
executeCommand "export CMAKE_INCLUDE_PATH=$CMAKE_INCLUDE_PATH:/framework-dependencies/include"
executeCommand "source /root-system/bin/thisroot.sh"
executeCommand "cmake .."
executeCommand "sudo make install"
executeCommand "ctest"
