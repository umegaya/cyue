#---------------------------------------------------------------
# basic initialization
#---------------------------------------------------------------
LOCAL_PATH := $(call my-dir)/..
include $(CLEAR_VARS)

#---------------------------------------------------------------
# configuration
#---------------------------------------------------------------
include $(LOCAL_PATH)/jni/Config.mk
ifneq ($(BUILD_SO), 1)
	include $(LOCAL_PATH)/jni/LuajitConfig.mk
endif

#---------------------------------------------------------------
# give information to NDK
#---------------------------------------------------------------
LOCAL_MODULE = yue
LOCAL_ARM_MODE = $(MY_ARM_MODE)
LOCAL_C_INCLUDES := $(MY_HEADER_PATHS)
LOCAL_SRC_FILES = $(foreach path,$(LOCAL_C_INCLUDES),$(shell find $(path) -maxdepth 1 -regex .*\\.cpp$$))
LOCAL_CFLAGS = -D__ANDROID_NDK__ -D__ARM_MODE__=NDK_ARM_BUILD_$(MY_ARM_MODE) -D__CPU_ARCH__=NDK_CPU_ARCH_$(patsubst '-','_',$(MY_ARM_ARCH)) -D_LL=lua -D_SERIALIZER=mpak -D__ENABLE_SENDFILE__ -D__ENABLE_EPOLL__ -D__ENABLE_INOTIFY__ -D__DISABLE_WRITEV__ -D__PTHREAD_DISABLE_THREAD_CANCEL__ -D__NBR_BYTE_ORDER__=__NBR_LITTLE_ENDIAN__

$(call ndk_log,$(LOCAL_C_INCLUDES))
$(call ndk_log,$(LOCAL_CFLAGS))
$(call,ndk_log,$(shell bash $(LOCAL_PATH)/jni/build_impl_h.sh))

DEBUG=1 # temporary enable debug option
ifeq ($(DEBUG), 1)
       LOCAL_CFLAGS += -D_DEBUG
       LOCAL_LDLIBS += -llog
endif

LOCAL_STATIC_LIBRARIES += luajit

ifeq ($(BUILD_SO), 1)
	LOCAL_CFLAGS += -D__BUILD_STANDALONE_ANDROID_LIB__
	include $(BUILD_SHARED_LIBRARY)
else
	include $(BUILD_STATIC_LIBRARY)
endif

#---------------------------------------------------------------
# include submodules
#---------------------------------------------------------------
include $(LOCAL_PATH)/jni/ll/$(MY_LL)/Android.mk

