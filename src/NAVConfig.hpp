#ifndef _NAV_CONFIG_HPP_
#define _NAV_CONFIG_HPP_

#ifdef HAVE_CONFIG_H
#	include "config.h"
#else

	// Backend enablements
#	ifdef _WIN32
#		define NAV_BACKEND_MEDIAFOUNDATION
#	endif

#	ifdef __ANDROID__
#		define NAV_BACKEND_ANDROIDNDK
#	endif

#	ifdef HAVE_MULTIFFMPEGCONFIG_H
#		include "multiffmpegconfig.h"
#	endif
	// Test for __has_include support
#	if (defined(_MSC_VER) && _MSC_VER >= 1911) || defined(__GNUC__) || defined(__clang__)
		// Check for FFmpeg
#		if !defined(HAVE_MULTIFFMPEGCONFIG_H) && \
		__has_include(<libavcodec/version.h>) && \
		__has_include(<libavformat/version.h>) && \
		__has_include(<libavutil/version.h>) && \
		__has_include(<libswresample/version.h>) && \
		__has_include(<libswscale/version.h>)
extern "C" {
#			include <libavcodec/version.h>
#			include <libavformat/version.h>
#			include <libavutil/version.h>
#			include <libswresample/version.h>
#			include <libswscale/version.h>
}
#			if \
			(LIBAVCODEC_VERSION_MAJOR == 61 && LIBAVCODEC_VERSION_MINOR >= 3) && \
			(LIBAVFORMAT_VERSION_MAJOR == 61 && LIBAVFORMAT_VERSION_MINOR >= 1) && \
			(LIBAVUTIL_VERSION_MAJOR == 59 && LIBAVUTIL_VERSION_MINOR >= 8) && \
			(LIBSWRESAMPLE_VERSION_MAJOR == 5 && LIBSWRESAMPLE_VERSION_MINOR >= 1) && \
			(LIBSWSCALE_VERSION_MAJOR == 8 && LIBSWSCALE_VERSION_MINOR >= 1)
#				define NAV_BACKEND_FFMPEG_7
#			elif \
			(LIBAVCODEC_VERSION_MAJOR == 60 && LIBAVCODEC_VERSION_MINOR >= 3) && \
			(LIBAVFORMAT_VERSION_MAJOR == 60 && LIBAVFORMAT_VERSION_MINOR >= 3) && \
			(LIBAVUTIL_VERSION_MAJOR == 58 && LIBAVUTIL_VERSION_MINOR >= 2) && \
			(LIBSWRESAMPLE_VERSION_MAJOR == 4 && LIBSWRESAMPLE_VERSION_MINOR >= 10) && \
			(LIBSWSCALE_VERSION_MAJOR == 7 && LIBSWSCALE_VERSION_MINOR >= 1)
#				define NAV_BACKEND_FFMPEG_6
#			elif \
			(LIBAVCODEC_VERSION_MAJOR == 59 && LIBAVCODEC_VERSION_MINOR >= 18) && \
			(LIBAVFORMAT_VERSION_MAJOR == 59 && LIBAVFORMAT_VERSION_MINOR >= 16) && \
			(LIBAVUTIL_VERSION_MAJOR == 57 && LIBAVUTIL_VERSION_MINOR >= 17) && \
			(LIBSWRESAMPLE_VERSION_MAJOR == 4 && LIBSWRESAMPLE_VERSION_MINOR >= 3) && \
			(LIBSWSCALE_VERSION_MAJOR == 6 && LIBSWSCALE_VERSION_MINOR >= 4)
#				define NAV_BACKEND_FFMPEG_5
#			elif \
			(LIBAVCODEC_VERSION_MAJOR == 58 && LIBAVCODEC_VERSION_MINOR >= 18) && \
			(LIBAVFORMAT_VERSION_MAJOR == 58 && LIBAVFORMAT_VERSION_MINOR >= 12) && \
			(LIBAVUTIL_VERSION_MAJOR == 56 && LIBAVUTIL_VERSION_MINOR >= 14) && \
			(LIBSWRESAMPLE_VERSION_MAJOR == 3 && LIBSWRESAMPLE_VERSION_MINOR >= 1) && \
			(LIBSWSCALE_VERSION_MAJOR == 5 && LIBSWSCALE_VERSION_MINOR >= 1)
#				define NAV_BACKEND_FFMPEG_4
#			endif
#		endif /* __has_include(ffmpeg includes) */

		// Check for GStreamer
#		if __has_include(<gst/gst.h>)
#			define NAV_BACKEND_GSTREAMER
#		endif
#	endif /* (defined(_MSC_VER) && _MSC_VER >= 1911) || defined(__GNUC__) || defined(__clang__) */

#endif /* HAVE_CONFIG_H */

#endif /* _NAV_CONFIG_HPP_ */
