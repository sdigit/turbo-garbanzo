CMAKE_MINIMUM_REQUIRED(VERSION 3.5)
PROJECT(turbogarbanzo C)
ENABLE_TESTING()

# pick the strictest warning/error diagnostic options for compiler being used
IF (CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER MATCHES "/clang$")
  SET(WARNINGS "-Wpedantic")
ELSEIF (CMAKE_C_COMPILER MATCHES "/icc$")
  SET(WARNINGS "-w3 -Wcheck -diag-error=warn,error")
ENDIF ()

# These options are common to gcc, clang, and icc.
SET(CMAKE_C_FLAGS "-std=c11 -pedantic -pedantic-errors -Wall -Wextra -Werror -Wstrict-prototypes ${WARNINGS}")

SET(SOURCE_FILES pytb/pytb.c)

# right now we only have the reader binary for testing :)
ADD_EXECUTABLE(reader ${SOURCE_FILES})
SET(TARGETS reader)
