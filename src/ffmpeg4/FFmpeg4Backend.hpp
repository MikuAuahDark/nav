#ifndef _NAV_BACKEND_FFMPEG_4_
#define _NAV_BACKEND_FFMPEG_4_

#include "NAVConfig.hpp"

#ifdef NAV_BACKEND_FFMPEG_4

#ifdef _NAV_FFMPEG_VERSION
#undef _NAV_FFMPEG_VERSION
#endif
#define _NAV_FFMPEG_VERSION 4

#include "ffmpeg_common/FFmpegBackend.hpp"
#undef _NAV_BACKEND_FFMPEG_

#endif /* NAV_BACKEND_FFMPEG_4 */
#endif /* _NAV_BACKEND_FFMPEG_4_ */
