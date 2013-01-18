#---------------------------------------------------------------
# basic initialization
#---------------------------------------------------------------
LOCAL_PATH := $(call my-dir)/..
include $(CLEAR_VARS)

#---------------------------------------------------------------
# configuration
#---------------------------------------------------------------
MY_ARM_MODE=thumb
MY_ARM_ARCH=armeabi

MY_SRC_ROOT=$(LOCAL_PATH)
MY_LL=lua
MY_SR=mpak

#---------------------------------------------------------------
# path setting
#---------------------------------------------------------------
MY_HEADER_PATHS += $(MY_SRC_ROOT)
MY_HEADER_PATHS += $(MY_SRC_ROOT)/fiber
MY_HEADER_PATHS += $(MY_SRC_ROOT)/handler/
MY_HEADER_PATHS += $(MY_SRC_ROOT)/ll
MY_HEADER_PATHS += $(MY_SRC_ROOT)/ll/$(MY_LL)/
MY_HEADER_PATHS += $(MY_SRC_ROOT)/net/
MY_HEADER_PATHS += $(MY_SRC_ROOT)/serializer
MY_HEADER_PATHS += $(MY_SRC_ROOT)/serializer/$(MY_SR)/
MY_HEADER_PATHS += $(MY_SRC_ROOT)/util/

#---------------------------------------------------------------
# give information to NDK
#---------------------------------------------------------------
LOCAL_MODULE = yue
LOCAL_ARM_MODE = $(MY_ARM_MODE)
LOCAL_C_INCLUDES := $(MY_HEADER_PATHS)
LOCAL_SRC_FILES = $(foreach path,$(LOCAL_C_INCLUDES),$(shell find $(path) -maxdepth 1 -regex .*\\.cpp$$ -printf $(path)/%f\\n))
LOCAL_CFLAGS = -D_DEBUG -D__ANDROID_NDK__ -D_LL=lua -D_SERIALIZER=mpak -D__ENABLE_SENDFILE__ -D__ENABLE_EPOLL__ -D__ENABLE_INOTIFY__ -D__DISABLE_WRITEV__ -D__PTHREAD_DISABLE_THREAD_CANCEL__ -D__NBR_BYTE_ORDER__=__NBR_LITTLE_ENDIAN__

$(call ndk_log,$(LOCAL_C_INCLUDES))
$(call ndk_log,$(LOCAL_SRC_FILES))

LOCAL_STATIC_LIBRARIES += libluajit

include $(BUILD_SHARED_LIBRARY)

#---------------------------------------------------------------
# include submodules
#---------------------------------------------------------------
include $(LOCAL_PATH)/jni/ll/$(MY_LL)/Android.mk

