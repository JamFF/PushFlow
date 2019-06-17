//
// Created by FF on 2019-06-16.
//

#ifndef PUSHFLOW_MACRO_H
#define PUSHFLOW_MACRO_H

#include <android/log.h>

#define LOG_TAG "PushFlow"
#define LOG_D(...) __android_log_print(ANDROID_LOG_DEBUG,LOG_TAG,__VA_ARGS__)
#define LOG_E(...) __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

// 宏函数
#define DELETE(obj) if(obj){ delete obj; obj = 0; }

#endif //PUSHFLOW_MACRO_H
