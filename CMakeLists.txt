CMAKE_MINIMUM_REQUIRED(VERSION 3.5)
PROJECT(ipcutils C)
ENABLE_TESTING()
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 -Wall -Wextra -Wstrict-prototypes -g -ggdb -pipe")
IF (CMAKE_C_COMPILER MATCHES "/icc$")
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w3 -ansi -strict-ansi")
ENDIF ()

SET(CMAKE_VERBOSE_MAKEFILE 1)
SET(SOURCE_FILES
    src/server.c
    src/reader.c)

ADD_EXECUTABLE(reader ${SOURCE_FILES})
SET(TARGETS reader)
