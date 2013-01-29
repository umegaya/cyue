LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

$(call ndk_log,$(LOCAL_PATH))
LIB_SRC_REL_PATH = ../../../ll/lua/exlib/luajit
LIB_SRC_PATH = $(LOCAL_PATH)/$(LIB_SRC_REL_PATH)
$(call ndk_log,$(LIB_SRC_PATH))
NDK = $(shell dirname `which ndk-build`)
NDKABI = 8
NDKVER = arm-linux-androideabi-4.6
HOST_ARCH = $(shell uname)
$(call ndk_log,Host Arch: $(HOST_ARCH))
ifeq ($(HOST_ARCH), Darwin)
	NDK_HOST_ARCH = 'darwin-x86'
else
	ifeq ($(HOST_ARCH), Win32)
		# TODO : we can know win32 and what to do for handling it?
		NDK_HOST_ARCH = 'win32??'
	else
		NDK_HOST_ARCH = 'linux-x86'
	endif
endif
define build_luajit_arm
	$(call ndk_log,build luajit arm using NDK @ $(NDK) $(NDKABI) $(NDKVER) $(NDK_HOST_ARCH) $(LIB_SRC_PATH))
	$(shell bash $(LOCAL_PATH)/build_luajit_arm.sh $(LIB_SRC_PATH) $(NDK) $(NDKABI) $(NDKVER) $(NDK_HOST_ARCH) $(MY_LUAJIT_OPTIONAL_CFLAGS))
endef

$(call ndk_log,$(call build_luajit_arm))

LOCAL_MODULE = luajit
LOCAL_SRC_FILES = $(LIB_SRC_REL_PATH)/src/libluajit.a

include $(PREBUILT_STATIC_LIBRARY)

#super dirty hack to force using external luajit build
#$(shell cp $(LIBLUAJIT) $(LOCAL_PATH)/../../../obj/local/armeabi/libluajit.a)

