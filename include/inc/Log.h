#ifndef LOG_H
#define LOG_H

#include <stddef.h>
#include <stdio.h>

#define LOGE(format, args...)	printf(format, ## args)
//#define LOGE(...)
#define LOGW(format, args...)	printf(format, ## args)
//#define LOGW(...)
#define LOGV(...)

#define H264DecTrace(args...)	printf(args)

#endif