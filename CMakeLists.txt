cmake_minimum_required(VERSION 2.6)
project(cmdsys C)
enable_testing()

if(NOT CMAKE_COMPILER_IS_GNUCC)
  message(FATAL_ERROR "Unsupported compiler.")
endif(NOT CMAKE_COMPILER_IS_GNUCC)

set(CMAKE_C_FLAGS "-pedantic -std=c99 -Wall -Wextra -Wcast-align -Wmissing-declarations -Wmissing-prototypes -fvisibility=hidden -Wl,-z,defs")
set(CMAKE_C_FLAGS_DEBUG "-g -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")

################################################################################
# Check dependencies
################################################################################
macro(check_dependency lib header)
  find_path(${lib}_INCLUDE_DIR ${header})
  find_library(${lib}_LIBRARY ${lib})

  if(NOT ${lib}_INCLUDE_DIR OR NOT ${lib}_LIBRARY)
    message(FATAL_ERROR "Can't find ${lib} header/library")
  else()
    message(STATUS "${lib} found: ${${lib}_LIBRARY}")
  endif()

  include_directories(${${lib}_INCLUDE_DIR})
endmacro()

check_dependency(argtable2 argtable2.h)
check_dependency(sys sys/sys.h)
check_dependency(sl sl/sl.h)

################################################################################
# Define targets
################################################################################
add_library(cmdsys SHARED src/cmdsys.c src/cmdsys.h)
target_link_libraries(cmdsys ${sl_LIBRARY} argtable2 ${sys_LIBRARY})
set_target_properties(cmdsys PROPERTIES DEFINE_SYMBOL CMDSYS_SHARED_BUILD)

add_executable(test_cmdsys src/test_cmdsys.c)
target_link_libraries(test_cmdsys cmdsys)
add_test(test_cmdsys test_cmdsys)

################################################################################
# Define output & install directories 
################################################################################
install(TARGETS cmdsys LIBRARY DESTINATION lib)
install(FILES src/cmdsys.h DESTINATION include)

