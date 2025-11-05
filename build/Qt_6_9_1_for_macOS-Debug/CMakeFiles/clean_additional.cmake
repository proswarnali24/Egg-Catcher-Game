# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles/EggCatcher_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/EggCatcher_autogen.dir/ParseCache.txt"
  "EggCatcher_autogen"
  )
endif()
