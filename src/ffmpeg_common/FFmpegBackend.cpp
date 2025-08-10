#ifdef _NAV_FFMPEG_VERSION

#include "NAVConfig.hpp"

#include <algorithm>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include "Internal.hpp"
#include "Common.hpp"
#include "Error.hpp"
#include "FFmpegBackend.hpp"
#include "FFmpegInternal.hpp"

#define NAV_FFCALL(name) f->func_##name

static std::vector<AVHWDeviceType> devicePreference = {
#ifdef _WIN32
	AV_HWDEVICE_TYPE_D3D11VA,
#endif
#if _NAV_FFMPEG_VERSION >= 5
	AV_HWDEVICE_TYPE_VULKAN,
#endif
	AV_HWDEVICE_TYPE_VAAPI
	// TODO: VideoToolbox HW acceleration.
	// Apple backend must be done first.
};

static std::set<AVPixelFormat> supportedHWAccelPixFmt = {
#if _NAV_FFMPEG_VERSION >= 5
	AV_PIX_FMT_VULKAN,
#endif
	AV_PIX_FMT_D3D11,
	AV_PIX_FMT_VAAPI
};

static ptrdiff_t getHWDevicePriority(AVHWDeviceType type)
{
    auto it = std::find(devicePreference.begin(), devicePreference.end(), type);

    if (it != devicePreference.end())
        return std::distance(devicePreference.begin(), it);
    else
        return ptrdiff_t((devicePreference.size()) + (size_t) type);
}

static int inputRead(void *nav_input, uint8_t *buf, int buf_size)
{
	struct nav_input *input = (struct nav_input*) nav_input;
	size_t readed = input->readf(buf, buf_size);

	if (readed == 0)
		return AVERROR_EOF;
	
	return (int) readed;
}

static int64_t inputSeek(void *nav_input, int64_t offset, int origin)
{
	struct nav_input *input = (struct nav_input*) nav_input;
	
	int64_t filesize = (int64_t) input->sizef();

	if (origin & AVSEEK_SIZE)
		return filesize;

	int64_t realoff = 0;

	switch (origin)
	{
		case SEEK_SET:
			realoff = offset;
			break;
		case SEEK_CUR:
			realoff = ((int64_t) input->tellf()) + offset;
			break;
		case SEEK_END:
			realoff = filesize + offset;
			break;
		default:
			return AVERROR(EINVAL);
	}

	if (!input->seekf((uint64_t) std::min<int64_t>(std::max<int64_t>(realoff, 0LL), filesize)))
		return AVERROR_UNKNOWN;

	return realoff;
}

[[noreturn]] static void throwFromAVError(decltype(&av_strerror) func_av_strerror, int code) noexcept(false)
{
	constexpr size_t BUFSIZE = 256;
	char temp[BUFSIZE];
	int r = func_av_strerror(code, temp, BUFSIZE);
	temp[BUFSIZE - 1] = '\0';

	throw std::runtime_error(r == 0 ? temp : "Unknown libav error");
}

inline int checkError(decltype(&av_strerror) func_av_strerror, int err) noexcept(false)
{
	if (err < 0)
		throwFromAVError(func_av_strerror, err);
	return err;
}

static nav_audioformat audioFormatFromAVSampleFormat(AVSampleFormat format)
{
	switch (format)
	{
		default:
			return 0;
		case AV_SAMPLE_FMT_U8:
			return nav::makeAudioFormat(8, false, false);
		case AV_SAMPLE_FMT_S16:
			return nav::makeAudioFormat(16, false, true);
		case AV_SAMPLE_FMT_S32:
			return nav::makeAudioFormat(32, false, true);
		case AV_SAMPLE_FMT_S64:
			return nav::makeAudioFormat(64, false, true);
		case AV_SAMPLE_FMT_FLT:
			return nav::makeAudioFormat(32, true, true);
		case AV_SAMPLE_FMT_DBL:
			return nav::makeAudioFormat(64, true, true);
	}
}

static std::tuple<nav_pixelformat, AVPixelFormat> getBestPixelFormat(AVPixelFormat pixfmt)
{
	switch (pixfmt)
	{
		// Unknown
		default:
			return std::make_tuple(NAV_PIXELFORMAT_UNKNOWN, AV_PIX_FMT_NONE);
		// Does not require conversion
		case AV_PIX_FMT_RGB24:
			return std::make_tuple(NAV_PIXELFORMAT_RGB8, pixfmt);
		case AV_PIX_FMT_YUV420P:
			return std::make_tuple(NAV_PIXELFORMAT_YUV420, pixfmt);
		case AV_PIX_FMT_YUV444P:
			return std::make_tuple(NAV_PIXELFORMAT_YUV444, pixfmt);
		case AV_PIX_FMT_NV12:
			return std::make_tuple(NAV_PIXELFORMAT_NV12, pixfmt);
		// Require conversions
		case AV_PIX_FMT_YUYV422:
		case AV_PIX_FMT_YUV422P:
		case AV_PIX_FMT_YUVJ422P:
		case AV_PIX_FMT_YUVJ444P:
		case AV_PIX_FMT_UYVY422:
		case AV_PIX_FMT_YUV440P:
		case AV_PIX_FMT_YUVJ440P:
		case AV_PIX_FMT_YUV422P16LE:
		case AV_PIX_FMT_YUV422P16BE:
		case AV_PIX_FMT_YUV444P16LE:
		case AV_PIX_FMT_YUV444P16BE:
		case AV_PIX_FMT_YUV422P10BE:
		case AV_PIX_FMT_YUV422P10LE:
		case AV_PIX_FMT_YUV444P9BE:
		case AV_PIX_FMT_YUV444P9LE:
		case AV_PIX_FMT_YUV444P10BE:
		case AV_PIX_FMT_YUV444P10LE:
		case AV_PIX_FMT_YUV422P9BE:
		case AV_PIX_FMT_YUV422P9LE:
		case AV_PIX_FMT_YUVA422P:
		case AV_PIX_FMT_YUVA444P:
		case AV_PIX_FMT_YUVA422P9BE:
		case AV_PIX_FMT_YUVA422P9LE:
		case AV_PIX_FMT_YUVA444P9BE:
		case AV_PIX_FMT_YUVA444P9LE:
		case AV_PIX_FMT_YUVA422P10BE:
		case AV_PIX_FMT_YUVA422P10LE:
		case AV_PIX_FMT_YUVA444P10BE:
		case AV_PIX_FMT_YUVA444P10LE:
		case AV_PIX_FMT_YUVA422P16BE:
		case AV_PIX_FMT_YUVA422P16LE:
		case AV_PIX_FMT_YUVA444P16BE:
		case AV_PIX_FMT_YUVA444P16LE:
		case AV_PIX_FMT_NV16:
		case AV_PIX_FMT_NV20LE:
		case AV_PIX_FMT_NV20BE:
		case AV_PIX_FMT_YVYU422:
		case AV_PIX_FMT_YUV422P12BE:
		case AV_PIX_FMT_YUV422P12LE:
		case AV_PIX_FMT_YUV422P14BE:
		case AV_PIX_FMT_YUV422P14LE:
		case AV_PIX_FMT_YUV444P12BE:
		case AV_PIX_FMT_YUV444P12LE:
		case AV_PIX_FMT_YUV444P14BE:
		case AV_PIX_FMT_YUV444P14LE:
		case AV_PIX_FMT_YUV440P10LE:
		case AV_PIX_FMT_YUV440P10BE:
		case AV_PIX_FMT_YUV440P12LE:
		case AV_PIX_FMT_YUV440P12BE:
		case AV_PIX_FMT_AYUV64LE:
		case AV_PIX_FMT_AYUV64BE:
#if _NAV_FFMPEG_VERSION >= 5
		case AV_PIX_FMT_YUVA422P12BE:
		case AV_PIX_FMT_YUVA422P12LE:
		case AV_PIX_FMT_YUVA444P12BE:
		case AV_PIX_FMT_YUVA444P12LE:
		case AV_PIX_FMT_Y210BE:
		case AV_PIX_FMT_Y210LE:
		case AV_PIX_FMT_P210BE:
		case AV_PIX_FMT_P210LE:
		case AV_PIX_FMT_P410BE:
		case AV_PIX_FMT_P410LE:
		case AV_PIX_FMT_P216BE:
		case AV_PIX_FMT_P216LE:
		case AV_PIX_FMT_P416BE:
		case AV_PIX_FMT_P416LE:
#endif /* _NAV_FFMPEG_VERSION >= 5 */
#if _NAV_FFMPEG_VERSION >= 6
		case AV_PIX_FMT_VUYA:
		case AV_PIX_FMT_VUYX:
		case AV_PIX_FMT_Y212BE:
		case AV_PIX_FMT_Y212LE:
		case AV_PIX_FMT_XV30BE:
		case AV_PIX_FMT_XV30LE:
		case AV_PIX_FMT_XV36BE:
		case AV_PIX_FMT_XV36LE:
#endif /* _NAV_FFMPEG_VERSION >= 6 */
			return std::make_tuple(NAV_PIXELFORMAT_YUV444, AV_PIX_FMT_YUV444P);
		case AV_PIX_FMT_BGR24:
		case AV_PIX_FMT_GRAY8:
		case AV_PIX_FMT_MONOWHITE:
		case AV_PIX_FMT_MONOBLACK:
		case AV_PIX_FMT_PAL8:
		case AV_PIX_FMT_BGR8:
		case AV_PIX_FMT_BGR4:
		case AV_PIX_FMT_BGR4_BYTE:
		case AV_PIX_FMT_RGB8:
		case AV_PIX_FMT_RGB4:
		case AV_PIX_FMT_RGB4_BYTE:
		case AV_PIX_FMT_ARGB:
		case AV_PIX_FMT_RGBA:
		case AV_PIX_FMT_ABGR:
		case AV_PIX_FMT_BGRA:
		case AV_PIX_FMT_GRAY16BE:
		case AV_PIX_FMT_GRAY16LE:
		case AV_PIX_FMT_RGB48BE:
		case AV_PIX_FMT_RGB48LE:
		case AV_PIX_FMT_RGB565BE:
		case AV_PIX_FMT_RGB565LE:
		case AV_PIX_FMT_RGB555BE:
		case AV_PIX_FMT_RGB555LE:
		case AV_PIX_FMT_BGR565BE:
		case AV_PIX_FMT_BGR565LE:
		case AV_PIX_FMT_BGR555BE:
		case AV_PIX_FMT_BGR555LE:
		case AV_PIX_FMT_RGB444LE:
		case AV_PIX_FMT_RGB444BE:
		case AV_PIX_FMT_BGR444LE:
		case AV_PIX_FMT_BGR444BE:
		case AV_PIX_FMT_YA8:
		case AV_PIX_FMT_BGR48BE:
		case AV_PIX_FMT_BGR48LE:
		case AV_PIX_FMT_GBRP:
		case AV_PIX_FMT_GBRP9BE:
		case AV_PIX_FMT_GBRP9LE:
		case AV_PIX_FMT_GBRP10BE:
		case AV_PIX_FMT_GBRP10LE:
		case AV_PIX_FMT_GBRP16BE:
		case AV_PIX_FMT_GBRP16LE:
		case AV_PIX_FMT_XYZ12LE:
		case AV_PIX_FMT_XYZ12BE:
		case AV_PIX_FMT_RGBA64BE:
		case AV_PIX_FMT_RGBA64LE:
		case AV_PIX_FMT_BGRA64BE:
		case AV_PIX_FMT_BGRA64LE:
		case AV_PIX_FMT_YA16BE:
		case AV_PIX_FMT_YA16LE:
		case AV_PIX_FMT_GBRAP:
		case AV_PIX_FMT_GBRAP16BE:
		case AV_PIX_FMT_GBRAP16LE:
		case AV_PIX_FMT_0RGB:
		case AV_PIX_FMT_RGB0:
		case AV_PIX_FMT_0BGR:
		case AV_PIX_FMT_BGR0:
		case AV_PIX_FMT_GBRP12BE:
		case AV_PIX_FMT_GBRP12LE:
		case AV_PIX_FMT_GBRP14BE:
		case AV_PIX_FMT_GBRP14LE:
		case AV_PIX_FMT_BAYER_BGGR8:
		case AV_PIX_FMT_BAYER_RGGB8:
		case AV_PIX_FMT_BAYER_GBRG8:
		case AV_PIX_FMT_BAYER_GRBG8:
		case AV_PIX_FMT_BAYER_BGGR16LE:
		case AV_PIX_FMT_BAYER_BGGR16BE:
		case AV_PIX_FMT_BAYER_RGGB16LE:
		case AV_PIX_FMT_BAYER_RGGB16BE:
		case AV_PIX_FMT_BAYER_GBRG16LE:
		case AV_PIX_FMT_BAYER_GBRG16BE:
		case AV_PIX_FMT_BAYER_GRBG16LE:
		case AV_PIX_FMT_BAYER_GRBG16BE:
		case AV_PIX_FMT_GBRAP12BE:
		case AV_PIX_FMT_GBRAP12LE:
		case AV_PIX_FMT_GBRAP10BE:
		case AV_PIX_FMT_GBRAP10LE:
		case AV_PIX_FMT_GRAY12BE:
		case AV_PIX_FMT_GRAY12LE:
		case AV_PIX_FMT_GRAY10BE:
		case AV_PIX_FMT_GRAY10LE:
		case AV_PIX_FMT_GRAY9BE:
		case AV_PIX_FMT_GRAY9LE:
		case AV_PIX_FMT_GBRPF32BE:
		case AV_PIX_FMT_GBRPF32LE:
		case AV_PIX_FMT_GBRAPF32BE:
		case AV_PIX_FMT_GBRAPF32LE:
#if _NAV_FFMPEG_VERSION >= 5
		case AV_PIX_FMT_GRAY14BE:
		case AV_PIX_FMT_GRAY14LE:
		case AV_PIX_FMT_GRAYF32BE:
		case AV_PIX_FMT_GRAYF32LE:
		case AV_PIX_FMT_X2RGB10LE:
		case AV_PIX_FMT_X2RGB10BE:
		case AV_PIX_FMT_X2BGR10LE:
		case AV_PIX_FMT_X2BGR10BE:
#endif /* _NAV_FFMPEG_VERSION >= 5 */
#if _NAV_FFMPEG_VERSION >= 6
		case AV_PIX_FMT_RGBAF16BE:
		case AV_PIX_FMT_RGBAF16LE:
		case AV_PIX_FMT_RGBF32BE:
		case AV_PIX_FMT_RGBF32LE:
		case AV_PIX_FMT_RGBAF32BE:
		case AV_PIX_FMT_RGBAF32LE:
#endif /* _NAV_FFMPEG_VERSION >= 6 */
			return std::make_tuple(NAV_PIXELFORMAT_RGB8, AV_PIX_FMT_RGB24);
		case AV_PIX_FMT_YUV410P:
		case AV_PIX_FMT_YUV411P:
		case AV_PIX_FMT_YUVJ420P:
		case AV_PIX_FMT_UYYVYY411:
		case AV_PIX_FMT_YUVA420P:
		case AV_PIX_FMT_YUV420P16LE:
		case AV_PIX_FMT_YUV420P16BE:
		case AV_PIX_FMT_YUV420P9BE:
		case AV_PIX_FMT_YUV420P9LE:
		case AV_PIX_FMT_YUV420P10BE:
		case AV_PIX_FMT_YUV420P10LE:
		case AV_PIX_FMT_YUVA420P9BE:
		case AV_PIX_FMT_YUVA420P9LE:
		case AV_PIX_FMT_YUVA420P10BE:
		case AV_PIX_FMT_YUVA420P10LE:
		case AV_PIX_FMT_YUVA420P16BE:
		case AV_PIX_FMT_YUVA420P16LE:
		case AV_PIX_FMT_YUV420P12BE:
		case AV_PIX_FMT_YUV420P12LE:
		case AV_PIX_FMT_YUV420P14BE:
		case AV_PIX_FMT_YUV420P14LE:
		case AV_PIX_FMT_YUVJ411P:
			return std::make_tuple(NAV_PIXELFORMAT_YUV420, AV_PIX_FMT_YUV420P);
		case AV_PIX_FMT_NV21:
		case AV_PIX_FMT_P010LE:
		case AV_PIX_FMT_P010BE:
		case AV_PIX_FMT_P016LE:
		case AV_PIX_FMT_P016BE:
#if _NAV_FFMPEG_VERSION >= 5
		case AV_PIX_FMT_NV24:
		case AV_PIX_FMT_NV42:
#endif /* _NAV_FFMPEG_VERSION >= 5 */
#if _NAV_FFMPEG_VERSION >= 6
		case AV_PIX_FMT_P012LE:
		case AV_PIX_FMT_P012BE:
#endif /* _NAV_FFMPEG_VERSION >= 6 */
			return std::make_tuple(NAV_PIXELFORMAT_NV12, AV_PIX_FMT_NV12);
	}
}

constexpr std::tuple<unsigned int, unsigned int> extractVersion(unsigned int ver)
{
	return std::make_tuple(ver >> 16, (ver >> 8) & 0xFF);
}

template<unsigned int ver>
bool isVersionCompatible(unsigned int(*func)())
{
	auto [cmaj, cmin] = extractVersion(ver);
	auto [rmaj, rmin] = extractVersion(func());
	return rmaj == cmaj && rmin >= cmin;
}

template<typename T>
struct CallOnLeave
{
	CallOnLeave(void(*func)(T*), T *ptr)
	: func(func)
	, ptr(ptr)
	{}

	~CallOnLeave()
	{
		func(ptr);
	}

	void(*func)(T*);
	T *ptr;
};

namespace nav::_NAV_FFMPEG_NAMESPACE
{

FFmpegFrame::FFmpegFrame(FFmpegBackend *f, nav_streaminfo_t *sinfo, AVFrame *frame, const AVCodecContext *cctx, double pts, size_t si)
: f(f) // must be first
, acquireData()
, pts(pts)
, frame(NAV_FFCALL(av_frame_clone)(frame))
, streamInfo(sinfo)
, index(si)
, codecContext(cctx)
{
	if (this->frame == nullptr)
		throw std::runtime_error("Cannot clone AVFrame");
}

size_t FFmpegFrame::getStreamIndex() const noexcept
{
	return index;
}

const nav_streaminfo_t *FFmpegFrame::getStreamInfo() const noexcept
{
	return streamInfo;
}

double FFmpegFrame::tell() const noexcept
{
	return pts;
}

const uint8_t *const *FFmpegFrame::acquire(ptrdiff_t **strides, size_t *nplanes)
{
	if (acquireData.source == nullptr)
	{
		AVFrame *targetFrame = frame;

		if (codecContext->hw_device_ctx)
		{
			// Hardware accelerated. Download the frame to CPU.
			AVFrame *swFrame = NAV_FFCALL(av_frame_alloc)();
			if (swFrame == nullptr)
			{
				nav::error::set("Cannot allocate CPU frame.");
				return nullptr;
			}

			if (int r = NAV_FFCALL(av_hwframe_transfer_data)(swFrame, frame, 0); r < 0)
			{
				NAV_FFCALL(av_frame_free)(&swFrame);
				throwFromAVError(NAV_FFCALL(av_strerror), r);
			}

			targetFrame = swFrame;
		}

		// AVFrame is already in CPU
		acquireData.planes.clear();
		acquireData.strides.clear();
		for (size_t i = 0; targetFrame->data[i]; i++)
		{
			acquireData.planes.push_back(targetFrame->data[i]);
			acquireData.strides.push_back(targetFrame->linesize[i]);
		}

		acquireData.source = (uint8_t*) targetFrame;
	}

	if (nplanes)
		*nplanes = acquireData.planes.size();
	*strides = acquireData.strides.data();
	return acquireData.planes.data();
}

void FFmpegFrame::release() noexcept
{
	if (codecContext->hw_device_ctx && acquireData.source)
	{
		AVFrame *swFrame = (AVFrame *) acquireData.source;
		NAV_FFCALL(av_frame_free)(&swFrame);
		acquireData.source = nullptr;
	}
}

nav_hwacceltype FFmpegFrame::getHWAccelType() const noexcept
{
	switch ((AVPixelFormat) frame->format)
	{
		case AV_PIX_FMT_D3D11:
			return NAV_HWACCELTYPE_D3D11;
#if _NAV_FFMPEG_VERSION >= 5
		case AV_PIX_FMT_VULKAN:
			return NAV_HWACCELTYPE_VULKAN;
#endif
		case AV_PIX_FMT_VAAPI:
			return NAV_HWACCELTYPE_VAAPI;
		default:
			return NAV_HWACCELTYPE_NONE;
	}
}

void *FFmpegFrame::getHWAccelHandle()
{
	switch ((AVPixelFormat) frame->format)
	{
		case AV_PIX_FMT_D3D11:
			return frame->data[0];
#if _NAV_FFMPEG_VERSION >= 5
		case AV_PIX_FMT_VULKAN:
			nav::error::set("NYI");
			return nullptr; // TODO: this should be AVVKFrame
#endif
		case AV_PIX_FMT_VAAPI:
			return frame->data[3];
	}
	nav::error::set("Not hardware accelerated or unknown hardware acceleration");
	return nullptr;
}

FFmpegFrame::~FFmpegFrame()
{
	if (frame)
	{
		release();
		NAV_FFCALL(av_frame_unref)(frame);
		NAV_FFCALL(av_frame_free)(&frame);
	}
}



FFmpegState::FFmpegState(FFmpegBackend *backend, UniqueAVFormatContext &fmtctx, UniqueAVIOContext &ioctx, const nav_settings &settings)
: f(backend)
, formatContext(std::move(fmtctx))
, ioContext(std::move(ioctx))
, tempPacket(NAV_FFCALL(av_packet_alloc)(), {NAV_FFCALL(av_packet_free)})
, tempFrame(NAV_FFCALL(av_frame_alloc)(), {NAV_FFCALL(av_frame_free)})
, position(0.0)
, eof(false)
, prepared(false)
, streamInfo()
, decoders()
, resamplers()
, rescalers()
, streamEofs()
{
	if (!tempPacket)
		throw std::runtime_error("Cannot allocate AVPacket");

	checkError(NAV_FFCALL(av_strerror), NAV_FFCALL(avformat_find_stream_info)(formatContext.get(), nullptr));

	streamInfo.reserve(formatContext->nb_streams);
	decoders.reserve(formatContext->nb_streams);
	resamplers.reserve(formatContext->nb_streams);
	rescalers.reserve(formatContext->nb_streams);
	streamEofs.resize(formatContext->nb_streams);

	for (unsigned int i = 0; i < formatContext->nb_streams; i++)
	{
		nav_streaminfo_t sinfo = {NAV_STREAMTYPE_UNKNOWN};
		AVStream *stream = formatContext->streams[i];
		const AVCodec *codec = nullptr;
		AVCodecContext *codecContext = nullptr;
		SwsContext *rescaler = nullptr;
		SwrContext *resampler = nullptr;
		bool good = true;

		switch (stream->codecpar->codec_type)
		{
			case AVMEDIA_TYPE_AUDIO:
			case AVMEDIA_TYPE_VIDEO:
			{
				codec = NAV_FFCALL(avcodec_find_decoder)(stream->codecpar->codec_id);
				good = codec != nullptr;
				decltype(AVCodecContext::get_format) oldFormat = nullptr;

				if (good)
				{
					codecContext = NAV_FFCALL(avcodec_alloc_context3)(codec);
					good = codecContext;
				}

				if (good)
				{
					good = NAV_FFCALL(avcodec_parameters_to_context)(codecContext, stream->codecpar) >= 0;
					oldFormat = codecContext->get_format;
					codecContext->get_format = pickPixelFormat;
				}

				if (good && !settings.disable_hwaccel && stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
				{
					// Try enable hardware acceleration
					std::vector<AVHWDeviceType> devices = getHWAccels();

					for (AVHWDeviceType hwacceltype: devices)
					{
						for (int j = 0; j < std::numeric_limits<int>::max(); j++)
						{
							const AVCodecHWConfig *hwconfig = NAV_FFCALL(avcodec_get_hw_config)(codec, j);
							if (hwconfig == nullptr)
								break;

							if (hwconfig->device_type != hwacceltype)
								continue;
							
							if (supportedHWAccelPixFmt.find(hwconfig->pix_fmt) == supportedHWAccelPixFmt.end())
								// Not a supported hardware pixel format
								continue;

							if (hwconfig->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)
							{
								if (NAV_FFCALL(av_hwdevice_ctx_create)(
									&codecContext->hw_device_ctx,
									hwacceltype,
									nullptr,
									nullptr,
									0
								) == 0)
								{
									// Got one
									codecContext->pix_fmt = hwconfig->pix_fmt;
									// FIXME: Query the hardware constraints.
									// But for now, assume NV12.
									codecContext->sw_pix_fmt = AV_PIX_FMT_NV12;
									break;
								}
							}
						}

						if (codecContext->hw_device_ctx)
							// Got HW accel device
							break;
					}
				}

				if (good)
				{
					if (!codecContext->hw_device_ctx)
						codecContext->get_format = oldFormat;

					codecContext->thread_count = (int) settings.max_threads;
					good = NAV_FFCALL(avcodec_open2)(codecContext, codec, nullptr) >= 0;
				}

				if (good)
				{
					if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
					{
						AVSampleFormat originalFormat = (AVSampleFormat) stream->codecpar->format;
						AVSampleFormat packedFormat = NAV_FFCALL(av_get_packed_sample_fmt)(originalFormat);
						if (packedFormat != originalFormat)
						{
							// Need to resample
#if _NAV_FFMPEG_VERSION >= 6
							good = NAV_FFCALL(swr_alloc_set_opts2)(
								&resampler,
								&stream->codecpar->ch_layout,
								packedFormat,
								stream->codecpar->sample_rate,
								&stream->codecpar->ch_layout,
								originalFormat,
								stream->codecpar->sample_rate,
								0, nullptr
							) >= 0;
#else
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
							resampler = NAV_FFCALL(swr_alloc_set_opts)(
								nullptr,
								stream->codecpar->channel_layout,
								packedFormat,
								stream->codecpar->sample_rate,
								stream->codecpar->channel_layout,
								originalFormat,
								stream->codecpar->sample_rate,
								0, nullptr
							);
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
							good = resampler;
#endif
						}

						if (good)
							good = NAV_FFCALL(swr_init)(resampler) >= 0;

						if (good)
						{
							// Audio stream
							sinfo.audio.format = audioFormatFromAVSampleFormat(packedFormat);
							sinfo.audio.sample_rate = stream->codecpar->sample_rate;
#if _NAV_FFMPEG_VERSION >= 6
							sinfo.audio.nchannels = stream->codecpar->ch_layout.nb_channels;
#else
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
							sinfo.audio.nchannels = stream->codecpar->channels;
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
#endif
							sinfo.type = NAV_STREAMTYPE_AUDIO;
						}
					}
					else
					{
						AVPixelFormat originalFormat = codecContext->hw_device_ctx
							? codecContext->sw_pix_fmt
							: ((AVPixelFormat) stream->codecpar->format);
						AVPixelFormat rescaleFormat = originalFormat;
						std::tie(sinfo.video.format, rescaleFormat) = getBestPixelFormat(originalFormat);

						// Need to rescale
						if (rescaleFormat != originalFormat)
						{
							if (codecContext->hw_device_ctx)
								// This is not what we've agreed beforehand
								good = false;
							else
							{
								rescaler = NAV_FFCALL(sws_getContext)(
									stream->codecpar->width,
									stream->codecpar->height,
									originalFormat,
									stream->codecpar->width,
									stream->codecpar->height,
									rescaleFormat,
									SWS_BICUBIC, nullptr, nullptr, nullptr
								);
								good = rescaler != nullptr;
							}
						}

						if (good)
						{
							// Video stream
							sinfo.type = NAV_STREAMTYPE_VIDEO;
							sinfo.video.width = (uint32_t) stream->codecpar->width;
							sinfo.video.height = (uint32_t) stream->codecpar->height;
							sinfo.video.fps = ffmpeg_common::derationalize(stream->avg_frame_rate);
							// sinfo.video.format is already set
						}
					}
				}

				break;
			}
			default:
				good = false;
				break;
		}

		if (!good)
		{
			stream->discard = AVDISCARD_ALL;
			NAV_FFCALL(avcodec_free_context)(&codecContext);

			if (rescaler)
			{
				NAV_FFCALL(swr_free)(&resampler);
				NAV_FFCALL(sws_freeContext)(rescaler);
				rescaler = nullptr;
			}
		}

		streamInfo.push_back(sinfo);
		decoders.push_back(codecContext);
		resamplers.push_back(resampler);
		rescalers.push_back(rescaler);
	}
}

FFmpegState::~FFmpegState()
{
	for (AVCodecContext *&decoder: decoders)
		NAV_FFCALL(avcodec_free_context)(&decoder);
	for (SwrContext *&resampler: resamplers)
		NAV_FFCALL(swr_free)(&resampler);
	for (SwsContext *&rescaler: rescalers)
		NAV_FFCALL(sws_freeContext)(rescaler);
}

Backend *FFmpegState::getBackend() const noexcept
{
	return f;
}

size_t FFmpegState::getStreamCount() const noexcept
{
	return formatContext->nb_streams;
}

const nav_streaminfo_t *FFmpegState::getStreamInfo(size_t index) const noexcept
{
	if (index >= streamInfo.size())
	{
		nav::error::set("Stream index out of range");
		return nullptr;
	}

	return &streamInfo[index];
}

bool FFmpegState::isStreamEnabled(size_t index) const noexcept
{
	if (index >= streamInfo.size())
	{
		nav::error::set("Stream index out of range");
		return false;
	}

	return formatContext->streams[index]->discard != AVDISCARD_ALL;
}

bool FFmpegState::setStreamEnabled(size_t index, bool enabled)
{
	if (index >= streamInfo.size())
	{
		nav::error::set("Stream index out of range");
		return false;
	}

	if (prepared)
	{
		nav::error::set("Decoder already initialized");
		return false;
	}

	formatContext->streams[index]->discard = enabled ? AVDISCARD_DEFAULT : AVDISCARD_ALL;
	return true;
}

double FFmpegState::getDuration() noexcept
{
	return derationalize<int64_t>(formatContext->duration, AV_TIME_BASE);
}

double FFmpegState::getPosition() noexcept
{
	return position;
}

double FFmpegState::setPosition(double off)
{
	int64_t pos = int64_t(off * AV_TIME_BASE);

	checkError(NAV_FFCALL(av_strerror), NAV_FFCALL(avformat_flush)(formatContext.get()));
	checkError(
		NAV_FFCALL(av_strerror),
		NAV_FFCALL(avformat_seek_file)(
			formatContext.get(),
			-1,
			std::numeric_limits<int64_t>::min(),
			pos,
			std::numeric_limits<int64_t>::max(),
			0
		)
	);

	for (AVCodecContext *decoder: decoders)
	{
		if (decoder)
			NAV_FFCALL(avcodec_flush_buffers)(decoder);
	}
	// No need to flush swr as we're not doing sample-rate conversion.

	position = derationalize<int64_t>(pos, AV_TIME_BASE);
	eof = false;
	return position;
}

bool FFmpegState::prepare()
{
	if (!prepared)
	{
		// Free inactive stream resources.
		for (size_t i = 0; i < getStreamCount(); i++)
		{
			if (formatContext->streams[i]->discard == AVDISCARD_ALL)
			{
				NAV_FFCALL(avcodec_free_context)(&decoders[i]);
				NAV_FFCALL(swr_free)(&resamplers[i]);
				NAV_FFCALL(sws_freeContext)(rescalers[i]);
				// Leave the streaminfo intact though, don't modify it.
			}
		}

		prepared = true;
	}

	return true;
}

bool FFmpegState::isPrepared() const noexcept
{
	return prepared;
}

nav_frame_t *FFmpegState::read()
{
	while (true)
	{
		int err = 0;

		// We have existing packet lingering around?
		if (tempPacket->buf)
		{
			// Pull frames
			err = NAV_FFCALL(avcodec_receive_frame)(decoders[tempPacket->stream_index], tempFrame.get());
			if (err >= 0)
			{
				// Has frame
				CallOnLeave<AVFrame> frameGuard(NAV_FFCALL(av_frame_unref), tempFrame.get());
				position = ffmpeg_common::derationalize(
					tempFrame->pts,
					formatContext->streams[tempPacket->stream_index]->time_base
				);
				return decode(tempFrame.get(), tempPacket->stream_index);
			}
			else
			{
				// No frame for now
				NAV_FFCALL(av_packet_unref)(tempPacket.get());

				if (err != AVERROR_EOF && err != AVERROR(EAGAIN))
					// Other error
					checkError(NAV_FFCALL(av_strerror), err);
			}
		}

		if (eof)
		{
			// Drain codec context
			for (size_t i = 0; i < decoders.size(); i++)
			{
				AVCodecContext *codecContext = decoders[i];

				if (codecContext && !streamEofs[i])
				{
					err = NAV_FFCALL(avcodec_receive_frame)(codecContext, tempFrame.get());
					if (err >= 0)
					{
						// Has frame
						CallOnLeave<AVFrame> frameGuard(NAV_FFCALL(av_frame_unref), tempFrame.get());
						position = ffmpeg_common::derationalize(tempFrame->pts, formatContext->streams[i]->time_base);
						return decode(tempFrame.get(), i);
					}
					else if (err == AVERROR_EOF)
						// No more frames
						streamEofs[i] = true;
					else
						// Unhandled error
						checkError(NAV_FFCALL(av_strerror), err);
				}
			}

			// This will be reached after all codecs are flushed.
			return nullptr;
		}
		else
		{
			// Read packet
			err = NAV_FFCALL(av_read_frame)(formatContext.get(), tempPacket.get());
			if (err == 0)
			{
				if (formatContext->streams[tempPacket->stream_index]->discard == AVDISCARD_ALL)
					NAV_FFCALL(av_packet_unref)(tempPacket.get());
				else
					checkError(NAV_FFCALL(av_strerror), NAV_FFCALL(avcodec_send_packet)(decoders[tempPacket->stream_index], tempPacket.get()));
			}
			else if (err == AVERROR_EOF)
			{
				// No more frames
				for (AVCodecContext *decoder: decoders)
				{
					if (decoder)
						NAV_FFCALL(avcodec_send_packet)(decoder, nullptr);
				}

				eof = true;
			}
			else
				checkError(NAV_FFCALL(av_strerror), err);
		}
	}
}

nav_frame_t *FFmpegState::decode(AVFrame *frame, size_t index)
{
	nav_streaminfo_t *streamInfo = &this->streamInfo[index];

	switch (streamInfo->type)
	{
		case NAV_STREAMTYPE_AUDIO:
		{
			// Decode audio
			SwrContext *resampler = resamplers[index];
			if (resampler == nullptr)
				// Skipping conversion
				return new FFmpegFrame(f, streamInfo, tempFrame.get(), decoders[index], position, index);

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
			std::unique_ptr<FrameVector> result(new FrameVector(
				streamInfo,
				index,
				position,
				resampler ? nullptr : frame->data[0],
				(
					((size_t) frame->nb_samples)
#if _NAV_FFMPEG_VERSION >= 6
					* frame->ch_layout.nb_channels
#else
					* frame->channels
#endif
					* NAV_AUDIOFORMAT_BYTESIZE(streamInfo->audio.format)
				)
			));
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

			if (resampler)
			{
				uint8_t *tempBuffer[AV_NUM_DATA_POINTERS] = {result->pointer(), nullptr};

				checkError(
					NAV_FFCALL(av_strerror),
					NAV_FFCALL(swr_convert)(resampler, tempBuffer, frame->nb_samples, (const uint8_t**) frame->data, frame->nb_samples)
				);
			}

			return result.release();
		}
		case NAV_STREAMTYPE_VIDEO:
		{
			// Decode video
			SwsContext *rescaler = rescalers[index];
			if (rescaler == nullptr)
				// Skipping conversion
				return new FFmpegFrame(f, streamInfo, tempFrame.get(), decoders[index], position, index);

			size_t needSize = streamInfo->video.size();
			uint8_t *bufferSetup[AV_NUM_DATA_POINTERS] = {nullptr};
			size_t dimension = ((size_t) streamInfo->video.width) * streamInfo->video.height;
			int linesizeSetup[AV_NUM_DATA_POINTERS] = {0};
			std::unique_ptr<FrameVector> result(new FrameVector(streamInfo, index, position, nullptr, needSize));

			// Setup buffers and linesize
			switch (streamInfo->video.format)
			{
				default:
					throw std::runtime_error("internal error @ " __FILE__ ":" NAV_STRINGIZE(__LINE__) ". Please report!");
				case NAV_PIXELFORMAT_RGB8:
				{
					bufferSetup[0] = result->pointer();
					linesizeSetup[0] = (int) streamInfo->video.width * 3;
					break;
				}
				case NAV_PIXELFORMAT_YUV420:
				{
					size_t halfdim = ((size_t) (streamInfo->video.width + 1) / 2) * ((streamInfo->video.height + 1) / 2);
					bufferSetup[0] = result->pointer();
					linesizeSetup[0] = streamInfo->video.width;
					bufferSetup[1] = bufferSetup[0] + dimension;
					linesizeSetup[1] = (streamInfo->video.width + 1) / 2;
					bufferSetup[2] = bufferSetup[1] + halfdim;
					linesizeSetup[2] = linesizeSetup[1];
					break;
				}
				case NAV_PIXELFORMAT_YUV444:
				{
					bufferSetup[0] = result->pointer();
					linesizeSetup[0] = streamInfo->video.width;
					bufferSetup[1] = bufferSetup[0] + dimension;
					linesizeSetup[1] = streamInfo->video.width;
					bufferSetup[2] = bufferSetup[1] + dimension;
					linesizeSetup[2] = linesizeSetup[1];
					break;
				}
				case NAV_PIXELFORMAT_NV12:
				{
					bufferSetup[0] = result->pointer();
					linesizeSetup[0] = streamInfo->video.width;
					bufferSetup[1] = bufferSetup[0] + dimension;
					linesizeSetup[1] = ((streamInfo->video.width + 1) / 2) * 2;
					break;
				}
			}

			// Rescale handles flip.
			checkError(
				NAV_FFCALL(av_strerror),
				NAV_FFCALL(sws_scale)(
					rescaler,
					frame->data,
					frame->linesize,
					0,
					streamInfo->video.height,
					bufferSetup,
					linesizeSetup
				)
			);
			return result.release();
		}
		default:
			throw std::runtime_error("internal error @ " __FILE__ ":" NAV_STRINGIZE(__LINE__) ". Please report!");
	}
}

bool FFmpegState::canDecode(size_t index)
{
	return streamInfo[index].type != NAV_STREAMTYPE_UNKNOWN && formatContext->streams[index]->discard == AVDISCARD_ALL;
}

std::vector<AVHWDeviceType> FFmpegState::getHWAccels()
{
	std::vector<AVHWDeviceType> devices;

	AVHWDeviceType type = AV_HWDEVICE_TYPE_NONE;
	while ((type = NAV_FFCALL(av_hwdevice_iterate_types)(type)) != AV_HWDEVICE_TYPE_NONE)
		devices.push_back(type);

	std::sort(devices.begin(), devices.end(), [](AVHWDeviceType a, AVHWDeviceType b)
	{
		return getHWDevicePriority(a) < getHWDevicePriority(b);
	});

	return devices;
}

AVPixelFormat FFmpegState::pickPixelFormat(AVCodecContext *s, const AVPixelFormat *fmt) noexcept
{
	for (int i = 0; fmt[i] != AV_PIX_FMT_NONE; i++)
	{
		if (supportedHWAccelPixFmt.find(fmt[i]) != supportedHWAccelPixFmt.end())
			return fmt[i];
	}

	return AV_PIX_FMT_NONE;
}

#undef NAV_FFCALL
#define NAV_FFCALL(n) this->func_##n

FFmpegBackend::FFmpegBackend()
: avutil(_NAV_FFMPEG_LIB_NAME("avutil", LIBAVUTIL_VERSION_MAJOR))
, avcodec(_NAV_FFMPEG_LIB_NAME("avcodec", LIBAVCODEC_VERSION_MAJOR))
, avformat(_NAV_FFMPEG_LIB_NAME("avformat", LIBAVFORMAT_VERSION_MAJOR))
, swresample(_NAV_FFMPEG_LIB_NAME("swresample", LIBSWRESAMPLE_VERSION_MAJOR))
, swscale(_NAV_FFMPEG_LIB_NAME("swscale", LIBSWSCALE_VERSION_MAJOR))
, info()
#define _NAV_PROXY_FUNCTION_POINTER(lib, n) , func_##n(nullptr)
#include "FFmpegPointers.h"
#undef _NAV_PROXY_FUNCTION_POINTER
{
	if (
#define _NAV_PROXY_FUNCTION_POINTER(lib, n) !lib.get(#n, &func_##n) ||
#include "FFmpegPointers.h"
#undef _NAV_PROXY_FUNCTION_POINTER
		!true // needed to fix the preprocessor stuff
	)
		throw std::runtime_error("Cannot load FFmpeg function pointer");

	if (!isVersionCompatible<LIBAVCODEC_VERSION_INT>(NAV_FFCALL(avcodec_version)))
		throw std::runtime_error("avcodec version mismatch");
	if (!isVersionCompatible<LIBAVFORMAT_VERSION_INT>(NAV_FFCALL(avformat_version)))
		throw std::runtime_error("avformat version mismatch");
	if (!isVersionCompatible<LIBAVUTIL_VERSION_INT>(NAV_FFCALL(avutil_version)))
		throw std::runtime_error("avutil version mismatch");
	if (!isVersionCompatible<LIBSWRESAMPLE_VERSION_INT>(NAV_FFCALL(swresample_version)))
		throw std::runtime_error("swresample version mismatch");
	if (!isVersionCompatible<LIBSWSCALE_VERSION_INT>(NAV_FFCALL(swscale_version)))
		throw std::runtime_error("swscale version mismatch");
}

FFmpegBackend::~FFmpegBackend()
{}

State *FFmpegBackend::open(nav_input *input, const char *filename, const nav_settings *settings)
{
	constexpr int BUFSIZE = 4096;

	UniqueAVFormatContext formatContext(NAV_FFCALL(avformat_alloc_context)(), NAV_FFCALL(avformat_free_context));
	if (!formatContext)
		throw std::runtime_error("Cannot allocate AVFormatContext");

	UniqueAVIOContext ioContext(
		NAV_FFCALL(avio_alloc_context)(
			(unsigned char*) NAV_FFCALL(av_malloc)(BUFSIZE),
			BUFSIZE,
			0,
			input,
			inputRead,
			nullptr,
			inputSeek
		),
		{NAV_FFCALL(avio_context_free)}
	);
	if (!ioContext)
		throw std::runtime_error("Cannot allocate AVIOContext");

	formatContext->pb = ioContext.get();
	formatContext->flags |= AVFMT_FLAG_CUSTOM_IO;

	AVFormatContext *tempFormatContext = formatContext.get();
	int errcode = 0;

	if ((errcode = NAV_FFCALL(avformat_open_input)(&tempFormatContext, filename, nullptr, nullptr)) < 0)
	{
		formatContext.release(); // prevent double-free
		throwFromAVError(NAV_FFCALL(av_strerror), errcode);
	}

	return new FFmpegState(this, formatContext, ioContext, *settings);
}

const char *FFmpegBackend::getName() const noexcept
{
	return NAV_STRINGIZE(_NAV_FFMPEG_NAMESPACE);
}

nav_backendtype FFmpegBackend::getType() const noexcept
{
	return NAV_BACKENDTYPE_3RD_PARTY;
}

const char *FFmpegBackend::getInfo()
{

	if (info.empty())
	{
		struct VersionInfo
		{
			const char *name;
			unsigned int(*func)();
		} verinfos[] = {
			{"avutil", NAV_FFCALL(avutil_version)},
			{"avcodec", NAV_FFCALL(avcodec_version)},
			{"avformat", NAV_FFCALL(avformat_version)},
			{"swscale", NAV_FFCALL(swscale_version)},
			{"swresample", NAV_FFCALL(swresample_version)}
		};

		std::stringstream infobuf;

		for (const VersionInfo &vinfo: verinfos)
		{
			unsigned int ver = vinfo.func();
			infobuf
				<< vinfo.name << " "
				<< AV_VERSION_MAJOR(ver) << "."
				<< AV_VERSION_MINOR(ver) << "."
				<< AV_VERSION_MICRO(ver)<< "; ";
		}

		info = infobuf.str();
	}

	return info.c_str();
}

Backend *create()
{
	if (checkBackendDisabled(_NAV_FFMPEG_DISABLEMENT) || checkBackendDisabled("FFMPEG"))
		return nullptr;

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

#endif /* _NAV_FFMPEG_VERSION */
