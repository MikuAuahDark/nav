#ifndef _NAV_BACKEND_FFMPEG_
#define _NAV_BACKEND_FFMPEG_

#include "nav_config.hpp"

#ifdef NAV_BACKEND_FFMPEG

#include "nav_internal.hpp"
#include "nav_backend.hpp"

namespace nav::ffmpeg
{

Backend *create();

}

#endif /* NAV_BACKEND_FFMPEG */
#endif /* _NAV_BACKEND_FFMPEG_ */
