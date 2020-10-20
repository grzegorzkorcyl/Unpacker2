# Source this script to set up the Unpacker2 build that this script is part of.
#
# Based on thisroot.sh script from ROOT 6.16

drop_from_path()
{
   # Assert that we got enough arguments
   if test $# -ne 2 ; then
      echo "drop_from_path: needs 2 arguments"
      return 1
   fi

   local p=$1
   local drop=$2

   newpath=`echo $p | sed -e "s;:${drop}:;:;g" \
                          -e "s;:${drop}\$;;g"   \
                          -e "s;^${drop}:;;g"   \
                          -e "s;^${drop}\$;;g"`
}

# check if some Unpacker2 path is already set
if [ -n "${UNPACKERSYS}" ] ; then
   old_unpackersys=${UNPACKERSYS}
fi

SOURCE=${BASH_ARGV[0]}
if [ "x$SOURCE" = "x" ]; then
    SOURCE=${(%):-%N} # for zsh
fi

if [ "x${SOURCE}" = "x" ]; then
    if [ -f bin/thisunpacker.sh ]; then
        UNPACKERSYS="$PWD"; export UNPACKERSYS
    elif [ -f ./thisunpacker.sh ]; then
        UNPACKERSYS=$(cd ..  > /dev/null; pwd); export UNPACKERSYS
    else
        echo ERROR: must "cd where/root/is" before calling ". bin/thisunpacker.sh" for this version of bash!
        UNPACKERSYS=; export UNPACKERSYS
        return 1
    fi
else
    # get param to "."
    thisunpacker=$(dirname ${SOURCE})
    UNPACKERSYS=$(cd ${thisunpacker}/.. > /dev/null;pwd); export UNPACKERSYS
fi

if [ -n "${old_unpackersys}" ] ; then
   if [ -n "${PATH}" ]; then
      drop_from_path "$PATH" "${old_unpackersys}/bin"
      PATH=$newpath
   fi
   if [ -n "${LD_LIBRARY_PATH}" ]; then
      drop_from_path "$LD_LIBRARY_PATH" "${old_unpackersys}/lib"
      LD_LIBRARY_PATH=$newpath
   fi
   if [ -n "${DYLD_LIBRARY_PATH}" ]; then
      drop_from_path "$DYLD_LIBRARY_PATH" "${old_unpackersys}/lib"
      DYLD_LIBRARY_PATH=$newpath
   fi
   if [ -n "${SHLIB_PATH}" ]; then
      drop_from_path "$SHLIB_PATH" "${old_unpackersys}/lib"
      SHLIB_PATH=$newpath
   fi
   if [ -n "${LIBPATH}" ]; then
      drop_from_path "$LIBPATH" "${old_unpackersys}/lib"
      LIBPATH=$newpath
   fi
   if [ -n "${CMAKE_PREFIX_PATH}" ]; then
      drop_from_path "$CMAKE_PREFIX_PATH" "${old_unpackersys}"
      CMAKE_PREFIX_PATH=$newpath
   fi
fi

if [ -z "${PATH}" ]; then
   PATH=$UNPACKERSYS/bin; export PATH
else
   PATH=$UNPACKERSYS/bin:$PATH; export PATH
fi

if [ -z "${LD_LIBRARY_PATH}" ]; then
   LD_LIBRARY_PATH=$UNPACKERSYS/lib; export LD_LIBRARY_PATH       # Linux, ELF HP-UX
else
   LD_LIBRARY_PATH=$UNPACKERSYS/lib:$LD_LIBRARY_PATH; export LD_LIBRARY_PATH
fi

if [ -z "${CMAKE_PREFIX_PATH}" ]; then
   CMAKE_PREFIX_PATH=$UNPACKERSYS; export CMAKE_PREFIX_PATH       # Linux, ELF HP-UX
else
   CMAKE_PREFIX_PATH=$UNPACKERSYS:$CMAKE_PREFIX_PATH; export CMAKE_PREFIX_PATH
fi

unset old_unpackersys
unset thisunpacker
unset -f drop_from_path
