#Generated by VisualGDB (http://visualgdb.com)
#DO NOT EDIT THIS FILE MANUALLY UNLESS YOU ABSOLUTELY NEED TO
#USE VISUALGDB PROJECT PROPERTIES DIALOG INSTEAD

BINARYDIR := Debug

#Toolchain
CC := $(TOOLCHAIN_ROOT)/bin/gcc.exe
CXX := $(TOOLCHAIN_ROOT)/bin/g++.exe
LD := $(CXX)
AR := $(TOOLCHAIN_ROOT)/bin/ar.exe
OBJCOPY := $(TOOLCHAIN_ROOT)/bin/objcopy.exe

#Additional flags
PREPROCESSOR_MACROS := DEBUG=1
INCLUDE_DIRS := ../curl
LIBRARY_DIRS := ../curl
LIBRARY_NAMES := ws2_32 wldap32 winmm Crypt32 curl
ADDITIONAL_LINKER_INPUTS := 
MACOS_FRAMEWORKS := 
LINUX_PACKAGES := 

CFLAGS := -ggdb -ffunction-sections -O0  -DCURL_STATICLIB -static
CXXFLAGS := -ggdb -ffunction-sections -O0  -DCURL_STATICLIB -static
ASFLAGS := 
LDFLAGS := -Wl,-gc-sections
COMMONFLAGS := 
LINKER_SCRIPT := 

START_GROUP := -Wl,--start-group
END_GROUP := -Wl,--end-group

#Additional options detected from testing the toolchain
USE_DEL_TO_CLEAN := 1
CP_NOT_AVAILABLE := 1