#ifdef _NAV_FFMPEG_VERSION

#ifndef _NAV_FFMPEG_NAMESPACE_0
#define _NAV_FFMPEG_NAMESPACE_0(x) NAV_CAT(ffmpeg, x)
#endif

#ifndef _NAV_FFMPEG_PREFIX_0
#define _NAV_FFMPEG_PREFIX_0(x) NAV_CAT(FFmpeg, x)
#endif

#ifndef _NAV_FFMPEG_DISABLEMENT_0
#define _NAV_FFMPEG_DISABLEMENT_0(x) NAV_CAT(FFMPEG, x)
#endif

#ifdef _NAV_FFMPEG_NAMESPACE
#undef _NAV_FFMPEG_NAMESPACE
#endif
#define _NAV_FFMPEG_NAMESPACE _NAV_FFMPEG_NAMESPACE_0(_NAV_FFMPEG_VERSION)

#ifdef _NAV_FFMPEG_DISABLEMENT
#undef _NAV_FFMPEG_DISABLEMENT
#endif
#define _NAV_FFMPEG_DISABLEMENT NAV_STRINGIZE(_NAV_FFMPEG_DISABLEMENT_0(_NAV_FFMPEG_VERSION))

#endif
