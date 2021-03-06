#
# 1. Set the path and clear environment
# 	TARGET_PATH := $(call my-dir)
# 	include $(ENV_CLEAR)
#
# 2. Set the source files and headers files
#	TARGET_SRC := xxx_1.c xxx_2.c
#	TARGET_INC := xxx_1.h xxx_2.h
#
# 3. Set the output target
#	TARGET_MODULE := xxx
#
# 4. Include the main makefile
#	include $(BUILD_BIN)
#
# Before include the build makefile, you can set the compilaion
# flags, e.g. TARGET_ASFLAGS TARGET_CFLAGS TARGET_CPPFLAGS
#
TARGET_PATH := $(call my-dir)

#########################################
include $(ENV_CLEAR)

include $(TARGET_TOP)/middleware/config/mpp_config.mk

ifeq ($(MPPCFG_COMPILE_DYNAMIC_LIB), Y)

include $(TARGET_PATH)/../config.mk

TARGET_SRC := \
	pixel_format.c \
	fbm.c \
	vdecoder.c \
	sbm/sbmStream.c \
	sbm/sbmHwProcess.c \
	sbm/sbmFrameBase.c \
	sbm/sbmFrameH264.c \
	sbm/sbmFrameH265.c

TARGET_INC := \
	$(TARGET_PATH) \
	$(TARGET_PATH)/include \
	$(TARGET_PATH)/../include \
	$(TARGET_PATH)/../ve/include \
	$(TARGET_PATH)/../base/include

TARGET_SHARED_LIB := \
    libVE \
    libcdc_base \
    libvideoengine \
    libMemAdapter

TARGET_CFLAGS += -fPIC -Wall -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function

SHARED_LIBS_PATHS := -L$(TARGET_PATH)/../ve/
TARGET_LDFLAGS := -shared -ldl $(SHARED_LIBS_PATHS)

TARGET_MODULE := libvdecoder

include $(BUILD_SHARED_LIB)

endif


ifeq ($(MPPCFG_COMPILE_STATIC_LIB), Y)
include $(ENV_CLEAR)

include $(TARGET_PATH)/../config.mk

TARGET_SRC := \
	pixel_format.c \
	fbm.c \
	vdecoder.c \
	sbm/sbmStream.c \
	sbm/sbmHwProcess.c \
	sbm/sbmFrameBase.c \
	sbm/sbmFrameH264.c \
	sbm/sbmFrameH265.c

TARGET_INC := \
	$(TARGET_PATH) \
	$(TARGET_PATH)/include \
	$(TARGET_PATH)/../include \
	$(TARGET_PATH)/../ve/include \
	$(TARGET_PATH)/../base/include

TARGET_CFLAGS += -fPIC -Wall -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function

TARGET_MODULE := libvdecoder

include $(BUILD_STATIC_LIB)

endif

