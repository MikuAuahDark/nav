#ifdef _NAV_FFMPEG_VERSION

#ifndef _NAV_BACKEND_FFMPEG_INTERNAL_
#define _NAV_BACKEND_FFMPEG_INTERNAL_

#include <memory>
#include <string>
#include <vector>

#include "Internal.hpp"
#include "Backend.hpp"
#include "FFmpegBackend.hpp"
#include "FFmpegCommon.hpp"
#include "DynLib.hpp"

namespace nav::_NAV_FFMPEG_NAMESPACE
{
	
using UniqueAVFormatContext = std::unique_ptr<AVFormatContext, decltype(&avformat_free_context)>;
using UniqueAVIOContext = std::unique_ptr<AVIOContext, ffmpeg_common::DoublePointerDeleter<AVIOContext>>;
using UniqueAVCodecContext = std::unique_ptr<AVCodecContext, ffmpeg_common::DoublePointerDeleter<AVCodecContext>>;
using UniqueAVPacket = std::unique_ptr<AVPacket, ffmpeg_common::DoublePointerDeleter<AVPacket>>;
using UniqueAVFrame = std::unique_ptr<AVFrame, ffmpeg_common::DoublePointerDeleter<AVFrame>>;

class FFmpegBackend;

class FFmpegFrame: public Frame
{
public:
	FFmpegFrame(FFmpegBackend *backend, nav_streaminfo_t *sinfo, AVFrame *frame, double pts, size_t si);
	~FFmpegFrame() override;
	size_t getStreamIndex() const noexcept override;
	const nav_streaminfo_t *getStreamInfo() const noexcept override;
	double tell() const noexcept override;
	const uint8_t *const *acquire(ptrdiff_t **strides, size_t *nplanes) override;
	void release() noexcept override;

private:
	AcquireData acquireData;
	double pts;
	AVFrame *frame;
	FFmpegBackend *f;
	nav_streaminfo_t *streamInfo;
	size_t index;
};

class FFmpegState: public State
{
public:
	FFmpegState(FFmpegBackend *backend, UniqueAVFormatContext &formatContext, UniqueAVIOContext &ioContext, const nav_settings &settings);
	~FFmpegState() override;
	Backend *getBackend() const noexcept override;
	size_t getStreamCount() const noexcept override;
	const nav_streaminfo_t *getStreamInfo(size_t index) const noexcept override;
	bool isStreamEnabled(size_t index) const noexcept override;
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
	bool eof;

	std::vector<nav_streaminfo_t> streamInfo;
	std::vector<AVCodecContext*> decoders;
	std::vector<SwrContext*> resamplers;
	std::vector<SwsContext*> rescalers;
	std::vector<bool> streamEofs;
};

class FFmpegBackend: public Backend
{
public:
	FFmpegBackend();
	~FFmpegBackend() override;
	const char *getName() const noexcept override;
	nav_backendtype getType() const noexcept override;
	const char *getInfo() override;
	State *open(nav_input *input, const char *filename, const nav_settings *settings) override;

private:
	friend class FFmpegState;
	friend class FFmpegFrame;

	DynLib avutil, avcodec, avformat, swscale, swresample;
	std::string info;

#define _NAV_PROXY_FUNCTION_POINTER(lib, n) decltype(n) *func_##n;
#include "FFmpegPointers.h"
#undef _NAV_PROXY_FUNCTION_POINTER
};

}

#endif /* _NAV_BACKEND_FFMPEG_INTERNAL_ */
#endif /* _NAV_FFMPEG_VERSION */
