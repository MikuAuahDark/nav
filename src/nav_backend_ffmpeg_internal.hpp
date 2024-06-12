#ifndef _NAV_BACKEND_FFMPEG_INTERNAL_
#define _NAV_BACKEND_FFMPEG_INTERNAL_

#include <memory>
#include <vector>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include "nav_internal.hpp"
#include "nav_backend.hpp"
#include "nav_dynlib.hpp"

namespace nav::ffmpeg
{

template<typename T>
struct DoublePointerDeleter
{
	inline void operator()(T *ptr)
	{
		if (ptr)
		{
			T *temp = ptr;
			deleter(&temp);
		}
	}

	void(*deleter)(T**);
};
	
using UniqueAVFormatContext = std::unique_ptr<AVFormatContext, decltype(&avformat_free_context)>;
using UniqueAVIOContext = std::unique_ptr<AVIOContext, DoublePointerDeleter<AVIOContext>>;
using UniqueAVCodecContext = std::unique_ptr<AVCodecContext, DoublePointerDeleter<AVCodecContext>>;
using UniqueAVPacket = std::unique_ptr<AVPacket, DoublePointerDeleter<AVPacket>>;
using UniqueAVFrame = std::unique_ptr<AVFrame, DoublePointerDeleter<AVFrame>>;

class FFmpegBackend;

class FFmpegState: public State
{
public:
	FFmpegState(FFmpegBackend *backend, UniqueAVFormatContext &formatContext, UniqueAVIOContext &ioContext);
	~FFmpegState() override;
	size_t getStreamCount() noexcept override;
	nav_streaminfo_t *getStreamInfo(size_t index) noexcept override;
	bool isStreamEnabled(size_t index) noexcept override;
	bool setStreamEnabled(size_t index, bool enabled) override;
	double getDuration() noexcept override;
	double getPosition() noexcept override;
	double setPosition(double off) override;
	nav_frame_t *read() override;

private:
	nav_frame_t *decode(AVFrame *frame, size_t index);
	bool canDecode(size_t index);

	FFmpegBackend *f;
	UniqueAVFormatContext formatContext;
	UniqueAVIOContext ioContext;
	UniqueAVPacket tempPacket;
	UniqueAVFrame tempFrame;
	double position;

	std::vector<nav_streaminfo_t> streamInfo;
	std::vector<AVCodecContext*> decoders;
	std::vector<SwrContext*> resamplers;
	std::vector<SwsContext*> rescalers;
};

class FFmpegBackend: public Backend
{
public:
	FFmpegBackend();
	~FFmpegBackend() override;
	State *open(nav_input *input, const char *filename) override;

private:
	friend class FFmpegState;

	DynLib avutil, avcodec, avformat, swscale, swresample;

#define _NAV_PROXY_FUNCTION_POINTER_FFMPEG(lib, n) decltype(n) *n;
#include "nav_backend_ffmpeg_funcptr.h"
#undef _NAV_PROXY_FUNCTION_POINTER_FFMPEG
};

}

#endif /* _NAV_BACKEND_FFMPEG_INTERNAL_ */
