#ifndef _NAV_BACKEND_FFMPEG_
#define _NAV_BACKEND_FFMPEG_

#include "NAVConfig.hpp"

#ifdef NAV_BACKEND_FFMPEG_6

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include "Internal.hpp"
#include "Backend.hpp"

namespace nav::ffmpeg6
{

Backend *create();

}

#endif /* NAV_BACKEND_FFMPEG_6 */
#endif /* _NAV_BACKEND_FFMPEG_ */
