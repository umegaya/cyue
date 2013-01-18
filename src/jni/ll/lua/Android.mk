LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LIB_SRC_REL_PATH = ../../../ll/lua/exlib/luajit
LIB_SRC_PATH = $(LOCAL_PATH)/$(LIB_SRC_REL_PATH)
LIBLUAJIT = $(LOCAL_PATH)/libluajit.a
NDK = $(shell dirname `which ndk-build`)
NDKABI = 9
NDKVER = arm-linux-androideabi-4.6
define build_luajit_arm
	$(call ndk_log,build luajit arm using NDK @ $(NDK) $(NDKABI) $(NDKVER))
	$(shell bash $(LOCAL_PATH)/build_luajit_arm.sh $(LIB_SRC_PATH) $(NDK) $(NDKABI) $(NDKVER))
endef

$(call ndk_log,$(LIB_SRC_PATH))

$(call ndk_log,$(call build_luajit_arm))

LOCAL_MODULE = luajit
LOCAL_SRC_FILES = libluajit.a

include $(PREBUILT_STATIC_LIBRARY)

#super dirty hack to force using external luajit build
#$(shell cp $(LIBLUAJIT) $(LOCAL_PATH)/../../../obj/local/armeabi/libluajit.a)

