# Makefile for EVT component: RRTGen.
#
# Copyright (c) 2003 - 2021 Codecraft, Inc.
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

#MAKE=make
RM=rm
MKDIR=mkdir
GCC=clang++

BASE = ..
#DOCDIR	= ./doc
#IMAGEDIR = ./images
INCLUDE = include
CODEDIR	= ./code
INCLUDEDIR = ./$(INCLUDE)
SCCOR = sccor
TESTDIR = evt
SCCORINCDIR = $(BASE)/$(SCCOR)/$(INCLUDE)
TESTINCDIR = $(BASE)/$(TESTDIR)/$(INCLUDE)
REQUIRED_DIRS = \
	$(CODEDIR)\
	$(NULL)

_MKDIRS := $(shell for d in $(REQUIRED_DIRS) ;	\
	     do					\
	       [ -d $$d ] || mkdir -p $$d ;	\
	     done)

#PROGNAME = RRTGen

#VERSION=1.0

#vpath %.pdf  $(DOCDIR)
#vpath %.html $(DOCDIR)
#vpath %.uxf  $(IMAGEDIR)
vpath %.cpp  $(CODEDIR)

# If no configuration is specified, "Debug" will be used.
ifndef CFG
CFG=Debug
endif

# Set architecture and PIC options.
ARCH_OPT=
PIC_OPT=
DEFS =
OS := $(shell uname)
ifeq ($(OS),Darwin)
# It's macOS.
ARCH_OPT += -arch x86_64
PIC_OPT += -Wl,-no_pie
else ifeq ($(OS),$(filter CYGWIN_NT%, $(OS)))
# It's Windows.
DEFS = -D"CYGWIN"
else
$(error The RRTGen framework requires macOS Big Sur (11.0.1) or later or Cygwin.)
endif

OUTDIR=./$(CFG)
SCCORDIR = $(BASE)/$(SCCOR)/$(CFG)

#
# Configuration: Debug
#
ifeq "$(CFG)" "Debug"
COMPILE=$(GCC) -c $(ARCH_OPT) $(DEFS) -fno-stack-protector -std=c++17 -O0 -g -o "$(OUTDIR)/$(*F).o" -I$(INCLUDEDIR) -I$(TESTINCDIR) -I$(SCCORINCDIR) "$<"
endif

#
# Configuration: Release
#
ifeq "$(CFG)" "Release"
COMPILE=$(GCC) -c $(ARCH_OPT) $(DEFS) -fno-stack-protector -std=c++17 -O0 -o "$(OUTDIR)/$(*F).o" -I$(INCLUDEDIR) -I$(TESTINCDIR) -I$(SCCORINCDIR) "$<"
endif

# Pattern rules
$(OUTDIR)/%.o : %.cpp
	$(COMPILE)

# Build rules
all: $(OUTDIR)/RRTGen.o $(OUTDIR)/RRandom.o

# Clean this project
clean:
	$(RM) -f $(OUTDIR)/*.o

