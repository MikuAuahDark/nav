#ifdef _NAV_FFMPEG_VERSION

#ifndef _NAV_BACKEND_FFMPEG_
#define _NAV_BACKEND_FFMPEG_

#include "NAVConfig.hpp"

#include "Internal.hpp"
#include "Backend.hpp"

#include "FFmpegSetup.h"

namespace nav::_NAV_FFMPEG_NAMESPACE
{

Backend *create();

}

#endif /* _NAV_BACKEND_FFMPEG_ */
#endif /* _NAV_FFMPEG_VERSION */
