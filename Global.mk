CC = g++
CPPFLAGS = -std=c++11 -g -g3 -Wall -fsanitize=address
SHAREDFLAGS = -fpic -shared -Wno-return-type-c-linkage
LDFLAGS = -L $(LIB_DIR)

# 依赖 thrift, glog
LIBS = -lthrift -lgflags -lglog -lfmt -lpthread 
INC = -I $(ROOTDIR)/include
AR = ar -rcs


BIN = $(ROOTDIR)/bin
LIB_DIR = $(ROOTDIR)/lib
INC_DIR = $(ROOTDIR)/include
SRC_DIR = $(ROOTDIR)/src
OBJECTS_DIR = $(ROOTDIR)/objs
