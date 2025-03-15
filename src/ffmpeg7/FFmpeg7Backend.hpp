#ifndef _NAV_BACKEND_FFMPEG_7_
#define _NAV_BACKEND_FFMPEG_7_

#include "NAVConfig.hpp"

#ifdef NAV_BACKEND_FFMPEG_7

#ifdef _NAV_FFMPEG_VERSION
#undef _NAV_FFMPEG_VERSION
#endif
#define _NAV_FFMPEG_VERSION 7

#include "ffmpeg_common/FFmpegBackend.hpp"
#undef _NAV_BACKEND_FFMPEG_

#endif /* NAV_BACKEND_FFMPEG_7 */
#endif /* _NAV_BACKEND_FFMPEG_7_ */
