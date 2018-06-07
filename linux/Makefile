CORSS_PREFIX ?=
SYSROOT ?=
AMAZON?=

$(warning $(CORSS_PREFIX))
$(warning $(SYSROOT))

CC := $(CORSS_PREFIX)g++
AR := $(CORSS_PREFIX)ar
RM := rm -rf

#原始目录
SRC_PATH :=.
MODULES :=
#源文件
SRCS = $(MODULES)
SRCS += $(wildcard $(SRC_PATH)/*.cpp)
#中间文件
OBJS = $(SRCS:.cpp=.o)
OBJS_MODULE = $(MODULES:.cpp=.o)

OBJS_get=$(OBJS_MODULE) kinesis_video_get.o
OBJS_put=$(OBJS_MODULE) kinesis_video_put.o
OBJS_put_audio=$(OBJS_MODULE) kinesis_video_put_audio.o

TARGET=put get put_audio
ALL_OBJS=$(OBJS)

#动态库
LIBS ?= 
LIBS += pthread dl
LIBS += producer
LIBS += rt log4cplus
LIBS += lzma bz2 z
LIBS += avformat avcodec swscale avutil avfilter swresample avdevice

THREE_LOCAL=$(AMAZON)/kinesis-video-native-build/downloads/local
PRODUCER=$(AMAZON)/kinesis-video-producer
PIC=$(AMAZON)/kinesis-video-pic
BUILD=$(AMAZON)/kinesis-video-native-build

SDK=./aws-sdk-cpp
SDK_KINESIS_VIDEO_MEDIA=$SDK/aws-cpp-sdk-kinesis-video-media/include
SDK_KINESIS_VIDEO_MEDIA_LIB=$SDK/local/aws-cpp-sdk-kinesis-video-media/

#头文件路径
INCLUDE_PATH ?=
INCLUDE_PATH += $(SYSROOT)/usr/include
INCLUDE_PATH +=.
INCLUDE_PATH += $(THREE_LOCAL)/include
INCLUDE_PATH += $(PRODUCER)/src
INCLUDE_PATH += $(PRODUCER)/opensource/jsoncpp
INCLUDE_PATH += $(PIC)/
INCLUDE_PATH += $(PIC)/src/
INCLUDE_PATH += $(PIC)/src/client/include
INCLUDE_PATH += $(PIC)/src/common/include
INCLUDE_PATH += $(PIC)/src/utils/include
INCLUDE_PATH += $(PIC)/src/trace/include
INCLUDE_PATH += $(PIC)/src/heap/include
INCLUDE_PATH += $(PIC)/src/mkvgen/include
INCLUDE_PATH += $(PIC)/src/view/include
INCLUDE_PATH += $SDK_KINESIS_VIDEO_MEDIA

#动态库路径
LIBRARY_PATH ?=
LIBRARY_PATH += $(SYSROOT)/usr/lib/ $(SYSROOT)/usr/local/lib/
LIBRARY_PATH += $(SYSROOT)/usr/lib64/ $(SYSROOT)/usr/local/lib64/
LIBRARY_PATH += ./
LIBRARY_PATH += $(THREE_LOCAL)/lib
LIBRARY_PATH += $(BUILD)
LIBRARY_PATH += $SDK_KINESIS_VIDEO_MEDIA_LIB

RELEASE ?= 1
BITS ?=

#ifeq ( 1 , ${DBG_ENABLE} )
#	CFLAGS += -D_DEBUG -O0 -g -DDEBUG=1
#endif

CFLAGS ?=
LFLAGS ?=

CFLAGS += -Wall -DMAIN_TEST -DUSE_BOOL -std=c++14 -D_POSIX_C_SOURCE=199506L -D_GNU_SOURCE -g

#头文件
CFLAGS += $(foreach dir,$(INCLUDE_PATH),-I$(dir))

#库路径
LDFLAGS += $(foreach lib,$(LIBRARY_PATH),-L$(lib))

#库名
LDFLAGS += $(foreach lib,$(LIBS),-l$(lib))

#检查版本
ifeq ($(RELEASE),0)
	#debug
	CFLAGS += -g
else
	#release
	CFLAGS += -O3 -DNDEBUG
endif

#检查位宽
ifeq ($(BITS),32)
	CFLAGS += -m32
	LFLAGS += -m32
else
	ifeq ($(BITS),64)
		CFLAGS += -m64
		LFLAGS += -m64
	else
	endif
endif

$(warning $(OBJS))

#操作命令
all:clean build

$(ALL_OBJS):%.o:%.cpp
	$(CC) $(CFLAGS) -c $^ -o $@

$(TARGET):$(ALL_OBJS)
	$(CC) $(CFLAGS) $(LFLAGS) -o $@ $(OBJS_$@) $(LDFLAGS)

build:$(TARGET)
	$(RM) $(ALL_OBJS)

clean:
	echo $(SRCS)
	$(RM) $(ALL_OBJS) $(TARGET)
