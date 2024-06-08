#ifndef _NAV_BACKEND_FFMPEG_INTERNAL_
#define _NAV_BACKEND_FFMPEG_INTERNAL_


#include "nav_internal.hpp"
#include "nav_backend.hpp"
#include "nav_dynlib.hpp"

namespace nav::ffmpeg
{

class FFmpegState: public State
{
public:
	~FFmpegState() override;
	size_t getStreamCount() noexcept override;
	nav_streaminfo_t *getStreamInfo(size_t index) noexcept override;
	bool isStreamEnabled(size_t index) noexcept override;
	bool setStreamEnabled(size_t index, bool enabled) override;
	double getDuration() noexcept override;
	double getPosition() noexcept override;
	double setPosition(double off) override;
	nav_packet_t *read() override;
};

class FFmpegBackend: public Backend
{
public:
	FFmpegBackend();
	~FFmpegBackend() override;
	State *open(nav_input *input, const char *filename) override;

private:
	DynLib avutil, avcodec, avformat;

#define _NAV_PROXY_FUNCTION_POINTER(n) decltype(n) *n
	_NAV_PROXY_FUNCTION_POINTER(avformat_alloc_context);
	_NAV_PROXY_FUNCTION_POINTER(avformat_free_context);
#undef _NAV_PROXY_FUNCTION_POINTER
};

}

#endif /* _NAV_BACKEND_FFMPEG_INTERNAL_ */
