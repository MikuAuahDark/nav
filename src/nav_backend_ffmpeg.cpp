#include "nav_config.hpp"

#ifdef NAV_BACKEND_FFMPEG

#include <memory>
#include <stdexcept>
#include <string>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>

#include "nav_internal.hpp"
#include "nav_error.hpp"
#include "nav_backend_ffmpeg.hpp"
#include "nav_backend_ffmpeg_internal.hpp"

static std::string getLibName(const char *compname, int ver)
{
	char buf[64];

#if defined(_WIN32)
	sprintf(buf, "%s-%d", compname, ver);
#elif defined(__ANDROID__)
	sprintf(buf, "lib%s.so", compname);
#else
	sprintf(buf, "lib%s.so.%d", compname, ver);
#endif

	return std::string(buf);
}

namespace nav::ffmpeg
{

using UniqueAVFormatContext = std::unique_ptr<AVFormatContext, decltype(&avformat_free_context)>;
using UniqueAVIOContext = std::unique_ptr<AVIOContext, decltype(&avio_close)>;

FFmpegBackend::FFmpegBackend()
: avutil(getLibName("avutil", LIBAVUTIL_VERSION_MAJOR))
, avcodec(getLibName("avcodec", LIBAVCODEC_VERSION_MAJOR))
, avformat(getLibName("avformat", LIBAVFORMAT_VERSION_MAJOR))
{
	if (
		!avformat.get("avformat_alloc_context", &avformat_alloc_context) ||
		!avformat.get("avformat_free_context", &avformat_free_context)
	)
		throw std::runtime_error("Cannot load FFmpeg function pointer");
}

State *FFmpegBackend::open(nav_input *input, const char *filename)
{
	UniqueAVFormatContext formatContext(avformat_alloc_context(), avformat_free_context);
	if (!formatContext)
		throw std::runtime_error("Cannot allocate AVFormatContext");

	UniqueAVIOContext ioContext(avio_alloc_context())
	formatContext->pb
	return nullptr;
}

Backend *create()
{
	try
	{
		return new FFmpegBackend();
	}
	catch (const std::exception &e)
	{
		nav::error::set(e);
		return nullptr;
	}
}

}

#endif /* NAV_BACKEND_FFMPEG */