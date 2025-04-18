# Project Name
TARGET = daisy-sampler-cpp

# Includes FatFS source files within project.
USE_FATFS = 1

LDFLAGS = -u _printf_float

# Sources
CPP_SOURCES = src/main.cpp
# CPP_SOURCES = tests/fatfs_test.cpp
# CPP_SOURCES = tests/bypass_test.cpp
# CPP_SOURCES = tests/button_test.cpp
# CPP_SOURCES = tests/raw_player.cpp
# CPP_SOURCES = tests/looper_test.cpp
# CPP_SOURCES = tests/wavwriter_test.cpp
# CPP_SOURCES = tests/logger_test.cpp


# Library Locations
LIBDAISY_DIR = ../DaisyExamples/libDaisy/
DAISYSP_DIR = ../DaisyExamples/DaisySP/

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
