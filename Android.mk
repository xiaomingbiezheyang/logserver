LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE:= qhlogserver

LOCAL_SRC_FILES:=main.cpp

LOCAL_SHARED_LIBRARIES := libcutils \
                          liblog \
                          libutils
			  
LOCAL_MODULE_TAGS := optional
ALL_DEFAULT_INSTALLED_MODULES += $(LOCAL_MODULE)

include $(BUILD_EXECUTABLE)

####################
include $(CLEAR_VARS)  

LOCAL_SRC_FILES := logqihancat  
LOCAL_MODULE := logqihancat 
LOCAL_MODULE_CLASS := EXECUTABLES  
LOCAL_MODULE_TAGS := optional
ALL_DEFAULT_INSTALLED_MODULES += $(LOCAL_MODULE)
include $(BUILD_PREBUILT)  


####################
include $(CLEAR_VARS)  

LOCAL_SRC_FILES := busybox-smp  
LOCAL_MODULE := busybox-smp 
LOCAL_MODULE_CLASS := EXECUTABLES  
LOCAL_MODULE_TAGS := optional
ALL_DEFAULT_INSTALLED_MODULES += $(LOCAL_MODULE)
include $(BUILD_PREBUILT)  