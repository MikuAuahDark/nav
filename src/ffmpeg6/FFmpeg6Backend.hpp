#ifndef _NAV_BACKEND_FFMPEG_6_
#define _NAV_BACKEND_FFMPEG_6_

#include "NAVConfig.hpp"

#ifdef NAV_BACKEND_FFMPEG_6

#ifdef _NAV_FFMPEG_VERSION
#undef _NAV_FFMPEG_VERSION
#endif
#define _NAV_FFMPEG_VERSION 6

#include "ffmpeg_common/FFmpegBackend.hpp"
#undef _NAV_BACKEND_FFMPEG_

#endif /* NAV_BACKEND_FFMPEG_6 */
#endif /* _NAV_BACKEND_FFMPEG_6_ */
