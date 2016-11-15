CMAKE_MINIMUM_REQUIRED(VERSION 3.5)
PROJECT(turbogarbanzo C)
ENABLE_TESTING()

# These options are common to gcc, clang, and icc.
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 -pedantic -pedantic-errors -Wall -Wextra -Werror -Wstrict-prototypes")

# pick the strictest warning/error diagnostic options for the specified compiler
IF (CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER MATCHES "/clang$")
  MESSAGE("++ using options common to gcc and clang")
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wpedantic")
ELSEIF (CMAKE_C_COMPILER MATCHES "/icc$")
  MESSAGE("++ using options specific to icc")
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w3 -Wcheck -diag-error=warn,error")
ENDIF ()

SET(SOURCE_FILES
    src/server.c
    src/reader.c)

# add -DDEBUG and -ggdb when debugging
SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG -ggdb")

# right now we only have the reader binary for testing :)
ADD_EXECUTABLE(reader ${SOURCE_FILES})
SET(TARGETS reader)
