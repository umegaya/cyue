#---------------------------------------------------------------
# architecture
#---------------------------------------------------------------
MY_ARM_MODE=arm
MY_ARM_ARCH=armeabi


#---------------------------------------------------------------
# selectable modules
#---------------------------------------------------------------
MY_LL=lua
MY_SR=mpak


#---------------------------------------------------------------
# path setting
#---------------------------------------------------------------
MY_SRC_ROOT=$(LOCAL_PATH)
MY_HEADER_PATHS += $(MY_SRC_ROOT)
MY_HEADER_PATHS += $(MY_SRC_ROOT)/fiber
MY_HEADER_PATHS += $(MY_SRC_ROOT)/handler/
MY_HEADER_PATHS += $(MY_SRC_ROOT)/ll
MY_HEADER_PATHS += $(MY_SRC_ROOT)/ll/$(MY_LL)/
MY_HEADER_PATHS += $(MY_SRC_ROOT)/net/
MY_HEADER_PATHS += $(MY_SRC_ROOT)/serializer
MY_HEADER_PATHS += $(MY_SRC_ROOT)/serializer/$(MY_SR)/
MY_HEADER_PATHS += $(MY_SRC_ROOT)/util/

