#ifndef _NAV_BACKEND_FFMPEG_8_
#define _NAV_BACKEND_FFMPEG_8_

#include "NAVConfig.hpp"

#ifdef NAV_BACKEND_FFMPEG_8

#ifdef _NAV_FFMPEG_VERSION
#undef _NAV_FFMPEG_VERSION
#endif
#define _NAV_FFMPEG_VERSION 8

#include "ffmpeg_common/FFmpegBackend.hpp"
#undef _NAV_BACKEND_FFMPEG_

#endif /* NAV_BACKEND_FFMPEG_8 */
#endif /* _NAV_BACKEND_FFMPEG_8_ */
