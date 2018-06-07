#ifndef FFHEADER_H
#define FFHEADER_H

//#define USE_NEW_API

#ifdef __cplusplus
extern "C" {
#endif

#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "libavcodec/avcodec.h"
#include "libavutil/avutil.h"
#include "libavutil/imgutils.h"
#include "libavutil/time.h"

#ifdef USE_NEW_API
#ifdef USE_ANDROID
#include "libavcodec/jni.h"
#endif
#endif

#ifdef __cplusplus
}
#endif



#endif