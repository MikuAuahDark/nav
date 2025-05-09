// Note: We're using certain API 28 NdkMedia functions. This means NAV requires Android API 28 for Android NDK
// backend. However we can't tell users to compile using API 28 as they may require supporting older Android.
// We're using function pointers anyway so the Android NDK backend will be unavailable at runtime on Android < API 28.
#ifndef __ANDROID_UNAVAILABLE_SYMBOLS_ARE_WEAK__
#define __ANDROID_UNAVAILABLE_SYMBOLS_ARE_WEAK__ 1 // NOLINT(bugprone-reserved-identifier)
#endif

// We really want this to be 1 though!
#if __ANDROID_UNAVAILABLE_SYMBOLS_ARE_WEAK__ != 1
#undef __ANDROID_UNAVAILABLE_SYMBOLS_ARE_WEAK__
#define __ANDROID_UNAVAILABLE_SYMBOLS_ARE_WEAK__ 1
#endif

#include "AndroidNDKBackend.hpp"

#ifdef NAV_BACKEND_ANDROIDNDK

#include <stdexcept>

#include "Error.hpp"
#include "AndroidNDKInternal.hpp"
#include "Common.hpp"

#define NAV_FFCALL(n) f->ptr_##n

constexpr int64_t MICROSECOND = 1000000;
constexpr int64_t DEFAULT_TIMEOUT = 1000LL; // 1 millisecond.

const char *getMediaStatusText(media_status_t code)
{
	switch (code)
	{
		case AMEDIA_OK:
			return "The requested media operation completed successfully. (AMEDIA_OK)";
		case AMEDIACODEC_ERROR_INSUFFICIENT_RESOURCE:
			return "Required resource was not able to be allocated. (AMEDIACODEC_ERROR_INSUFFICIENT_RESOURCE)";
		case AMEDIACODEC_ERROR_RECLAIMED:
			return "The resource manager reclaimed the media resource used by the codec. (AMEDIACODEC_ERROR_RECLAIMED)";
		case AMEDIA_ERROR_UNKNOWN:
		default:
			return "The called media function failed with an unknown error. (AMEDIA_ERROR_UNKNOWN)";
		case AMEDIA_ERROR_MALFORMED:
			return "The input media data is corrupt or incomplete. (AMEDIA_ERROR_MALFORMED)";
		case AMEDIA_ERROR_UNSUPPORTED:
			return "The required operation or media formats are not supported. (AMEDIA_ERROR_UNSUPPORTED)";
		case AMEDIA_ERROR_INVALID_OBJECT:
			return "An invalid (or already closed) object is used in the function call. (AMEDIA_ERROR_INVALID_OBJECT)";
		case AMEDIA_ERROR_INVALID_PARAMETER:
			return "At least one of the invalid parameters is used. (AMEDIA_ERROR_INVALID_PARAMETER)";
		case AMEDIA_ERROR_INVALID_OPERATION:
			return "The media object is not in the right state for the required operation. (AMEDIA_ERROR_INVALID_OPERATION)";
		case AMEDIA_ERROR_END_OF_STREAM:
			return "Media stream ends while processing the requested operation. (AMEDIA_ERROR_END_OF_STREAM)";
		case AMEDIA_ERROR_IO:
			return "An Error occurred when the Media object is carrying IO operation. (AMEDIA_ERROR_IO)";
		case AMEDIA_ERROR_WOULD_BLOCK:
			return "The required operation would have to be blocked (on I/O or others) but blocking is not enabled. (AMEDIA_ERROR_WOULD_BLOCK)";
		case AMEDIA_DRM_NOT_PROVISIONED:
			return "AMEDIA_DRM_NOT_PROVISIONED";
		case AMEDIA_DRM_RESOURCE_BUSY:
			return "AMEDIA_DRM_RESOURCE_BUSY";
		case AMEDIA_DRM_DEVICE_REVOKED:
			return "AMEDIA_DRM_DEVICE_REVOKED";
		case AMEDIA_DRM_SHORT_BUFFER:
			return "AMEDIA_DRM_SHORT_BUFFER";
		case AMEDIA_DRM_SESSION_NOT_OPENED:
			return "AMEDIA_DRM_SESSION_NOT_OPENED";
		case AMEDIA_DRM_TAMPER_DETECTED:
			return "AMEDIA_DRM_TAMPER_DETECTED";
		case AMEDIA_DRM_VERIFY_FAILED:
			return "AMEDIA_DRM_VERIFY_FAILED";
		case AMEDIA_DRM_NEED_KEY:
			return "AMEDIA_DRM_NEED_KEY";
		case AMEDIA_DRM_LICENSE_EXPIRED:
			return "AMEDIA_DRM_LICENSE_EXPIRED";
		case AMEDIA_IMGREADER_NO_BUFFER_AVAILABLE:
			return "There are no more image buffers to read/write image data. (AMEDIA_IMGREADER_NO_BUFFER_AVAILABLE)";
		case AMEDIA_IMGREADER_MAX_IMAGES_ACQUIRED:
			return "The AImage object has used up the allowed maximum image buffers. (AMEDIA_IMGREADER_MAX_IMAGES_ACQUIRED)";
		case AMEDIA_IMGREADER_CANNOT_LOCK_IMAGE:
			return "The required image buffer could not be locked to read. (AMEDIA_IMGREADER_CANNOT_LOCK_IMAGE)";
		case AMEDIA_IMGREADER_CANNOT_UNLOCK_IMAGE:
			return "The media data or buffer could not be unlocked. (AMEDIA_IMGREADER_CANNOT_UNLOCK_IMAGE)";
		case AMEDIA_IMGREADER_IMAGE_NOT_LOCKED:
			return "The media/buffer needs to be locked to perform the required operation. (AMEDIA_IMGREADER_IMAGE_NOT_LOCKED)";
	}
}

nav_pixelformat getNDKPixelFormat(int format)
{
	// https://developer.android.com/reference/android/media/MediaCodecInfo.CodecCapabilities
	switch (format)
	{
		default:
			return NAV_PIXELFORMAT_UNKNOWN;
		case 11: // COLOR_Format24bitRGB888
			return NAV_PIXELFORMAT_RGB8;
		case 19: // COLOR_FormatYUV420Planar
			return NAV_PIXELFORMAT_YUV420;
		case 21: // COLOR_FormatYUV420SemiPlanar
			return NAV_PIXELFORMAT_NV12;
	}
}

inline void checkError(media_status_t status)
{
	if (status != AMEDIA_OK)
		throw std::runtime_error(getMediaStatusText(status));
}

inline bool setErrorOnFail(media_status_t status)
{
	if (status != AMEDIA_OK)
		nav::error::set(getMediaStatusText(status));

	return status == AMEDIA_OK;
}

inline nav_audioformat audioFormatFromPCMEncoding(int encoding)
{
	// https://cs.android.com/android/platform/superproject/main/+/main:frameworks/av/media/module/foundation/include/media/stagefright/foundation/MediaDefs.h;l=138;drc=d5137445c0d4067406cb3e38aade5507ff2fcd16
	switch (encoding)
	{
		case 2:
		default: // Eh what could go wrong
			return nav::makeAudioFormat(16, false, true);
		case 3:
			return nav::makeAudioFormat(8, false, false);
		case 4:
			return nav::makeAudioFormat(32, true, true);
		case 21:
			return nav::makeAudioFormat(24, false, true);
		case 22:
			return nav::makeAudioFormat(32, false, true);
	}
}

namespace nav::androidndk
{

MediaSourceWrapper::MediaSourceWrapper(AndroidNDKBackend *backend, nav_input *input)
: f(backend)
, datasource(NAV_FFCALL(AMediaDataSource_new)())
, input(input)
{
	if (datasource == nullptr)
		throw std::runtime_error("cannot allocate AMediaDataSource");
	
	NAV_FFCALL(AMediaDataSource_setClose)(datasource, close);
	NAV_FFCALL(AMediaDataSource_setGetAvailableSize)(datasource, getAvailableSize);
	NAV_FFCALL(AMediaDataSource_setGetSize)(datasource, getSize);
	NAV_FFCALL(AMediaDataSource_setReadAt)(datasource, readAt);
	NAV_FFCALL(AMediaDataSource_setUserdata)(datasource, input);
}

MediaSourceWrapper::MediaSourceWrapper(MediaSourceWrapper &&other)
: f(other.f)
, datasource(other.datasource)
, input(other.input)
{
	other.datasource = nullptr;
	other.input = nullptr;
}

MediaSourceWrapper::~MediaSourceWrapper()
{
	if (datasource)
	{
		if (input && input->userdata)
			NAV_FFCALL(AMediaDataSource_close)(datasource);
		NAV_FFCALL(AMediaDataSource_delete)(datasource);
	}
}

AMediaDataSource *MediaSourceWrapper::get() const noexcept
{
	return datasource;
}

ssize_t MediaSourceWrapper::readAt(void *userdata, off64_t offset, void *buffer, size_t size)
{
	nav_input *input = (nav_input*) userdata;
	if (input->tellf() != (uint64_t) offset)
		if (!input->seekf((uint64_t) offset))
			return -1;
	
	size_t readed = input->readf(buffer, size);
	return readed > 0 ? ((ssize_t) readed) : (-1);
}

ssize_t MediaSourceWrapper::getSize(void *userdata)
{
	nav_input *input = (nav_input*) userdata;
	return (ssize_t) input->sizef();
}

void MediaSourceWrapper::close(void *userdata)
{
	nav_input *input = (nav_input*) userdata;
	input->closef();
}

ssize_t MediaSourceWrapper::getAvailableSize(void *userdata, off64_t offset)
{
	nav_input *input = (nav_input*) userdata;
	// TODO: Should we return -1 or input->sizef()?
	return (ssize_t) input->sizef();
}

AndroidNDKState::AndroidNDKState(AndroidNDKBackend *backend, UniqueMediaExtractor &ext, MediaSourceWrapper &ds)
: f(backend)
, durationUs(0)
, positionUs(0)
, needAdvance(false)
, activeStream()
, hasEOS()
, streamInfo()
, decoders()
, extractor(std::move(ext))
, dataSource(std::move(ds))
{
	size_t tracks = NAV_FFCALL(AMediaExtractor_getTrackCount)(extractor.get());
	activeStream.resize(tracks);
	hasEOS.resize(tracks);
	streamInfo.resize(tracks);
	decoders.reserve(tracks);

	for (size_t i = 0; i < tracks; i++)
	{
		decoders.emplace_back(nullptr, NAV_FFCALL(AMediaCodec_delete));

		// Most of these keys are taken from:
		// https://developer.android.com/reference/android/media/MediaFormat

		UniqueMediaFormat format(NAV_FFCALL(AMediaExtractor_getTrackFormat)(extractor.get(), i), NAV_FFCALL(AMediaFormat_delete));
		const char *mime = nullptr;
		nav_streamtype streamType = NAV_STREAMTYPE_UNKNOWN;
		bool r = NAV_FFCALL(AMediaFormat_getString)(format.get(), *NAV_FFCALL(AMEDIAFORMAT_KEY_MIME), &mime);

		if (r && mime)
		{
			if (strstr(mime, "video/") == mime)
				streamType = NAV_STREAMTYPE_VIDEO;
			else if (strstr(mime, "audio/") == mime)
				streamType = NAV_STREAMTYPE_AUDIO;
		}

		UniqueMediaCodec codec(nullptr, NAV_FFCALL(AMediaCodec_delete));
		if (r && streamType != NAV_STREAMTYPE_UNKNOWN)
		{
			codec.reset(NAV_FFCALL(AMediaCodec_createDecoderByType)(mime));
			r = codec.get();
		}

		if (!r || streamType == NAV_STREAMTYPE_UNKNOWN)
		{
			// No MIME type
			NAV_FFCALL(AMediaExtractor_unselectTrack)(extractor.get(), i);
			streamInfo[i].type = streamType;
			activeStream[i] = false;
			continue;
		}

        // Try with low-latency.
        NAV_FFCALL(AMediaFormat_setInt32)(format.get(), "low-latency", 1);
		// https://android.googlesource.com/platform/cts/+/23949c5/tests/tests/media/libmediandkjni/native-media-jni.cpp
		if (
			NAV_FFCALL(AMediaCodec_configure)(codec.get(), format.get(), nullptr, nullptr, 0) != AMEDIA_OK ||
			NAV_FFCALL(AMediaCodec_start)(codec.get()) != AMEDIA_OK
		)
		{
			// Maybe low latency is not supported
			NAV_FFCALL(AMediaFormat_setInt32)(format.get(), "low-latency", 0);
			if (
				NAV_FFCALL(AMediaCodec_configure)(codec.get(), format.get(), nullptr, nullptr, 0) != AMEDIA_OK ||
				NAV_FFCALL(AMediaCodec_start)(codec.get()) != AMEDIA_OK
			)
			{
				// Cannot configure decoder
				NAV_FFCALL(AMediaExtractor_unselectTrack)(extractor.get(), i);
				streamInfo[i].type = streamType;
				activeStream[i] = false;
				continue;
			}
		}

		UniqueMediaFormat outputFormat(NAV_FFCALL(AMediaCodec_getOutputFormat)(codec.get()), NAV_FFCALL(AMediaFormat_delete));
		
		int32_t val32 = 0;
		int64_t val64 = 0;
		float valf = 0.0;

		// Ensure the output format is something NAV currently understood
		if (streamType == NAV_STREAMTYPE_VIDEO)
		{
			nav_pixelformat outputPixFmt = NAV_PIXELFORMAT_UNKNOWN;

			if (NAV_FFCALL(AMediaFormat_getInt32)(outputFormat.get(), *NAV_FFCALL(AMEDIAFORMAT_KEY_COLOR_FORMAT), &val32))
				outputPixFmt = getNDKPixelFormat(val32);
			if (outputPixFmt == NAV_PIXELFORMAT_UNKNOWN)
			{
				// Abort. We don't understand this pixel format.
				// TODO: Convert some plausible pixel format to something better, if it's lossless.
				NAV_FFCALL(AMediaCodec_stop)(codec.get());
				NAV_FFCALL(AMediaExtractor_unselectTrack)(extractor.get(), i);
				streamInfo[i].type = streamType;
				activeStream[i] = false;
				continue;
			}

			streamInfo[i].video.format = outputPixFmt;
		}

		streamInfo[i].type = streamType;
		if (NAV_FFCALL(AMediaFormat_getInt64)(format.get(), *NAV_FFCALL(AMEDIAFORMAT_KEY_DURATION), &val64))
			durationUs = std::max(durationUs, val64);

		switch (streamType)
		{
			default:
				throw std::runtime_error("internal error @ " __FILE__ ":" NAV_STRINGIZE(__LINE__) ". Please report!");
			case NAV_STREAMTYPE_AUDIO:
			{
				streamInfo[i].audio.sample_rate =
					NAV_FFCALL(AMediaFormat_getInt32)(format.get(), *NAV_FFCALL(AMEDIAFORMAT_KEY_SAMPLE_RATE), &val32)
					? uint32_t(val32)
					: 0;
				streamInfo[i].audio.nchannels =
					NAV_FFCALL(AMediaFormat_getInt32)(format.get(), *NAV_FFCALL(AMEDIAFORMAT_KEY_CHANNEL_COUNT), &val32)
					? uint32_t(val32)
					: 0;
				streamInfo[i].audio.format =
					NAV_FFCALL(AMediaFormat_getInt32)(outputFormat.get(), *NAV_FFCALL(AMEDIAFORMAT_KEY_PCM_ENCODING), &val32)
					? audioFormatFromPCMEncoding(val32)
					: makeAudioFormat(16, false, true);
				break;
			}
			case NAV_STREAMTYPE_VIDEO:
			{
				streamInfo[i].video.width = NAV_FFCALL(AMediaFormat_getInt32)(format.get(), *NAV_FFCALL(AMEDIAFORMAT_KEY_WIDTH), &val32)
					? uint32_t(val32)
					: 0;
				streamInfo[i].video.height = NAV_FFCALL(AMediaFormat_getInt32)(format.get(), *NAV_FFCALL(AMEDIAFORMAT_KEY_HEIGHT), &val32)
					? uint32_t(val32)
					: 0;
				streamInfo[i].video.fps = NAV_FFCALL(AMediaFormat_getFloat)(format.get(), *NAV_FFCALL(AMEDIAFORMAT_KEY_FRAME_RATE), &valf)
					? double(valf)
					: 0.0;
				// format already set
				break;
			}
		}

		activeStream[i] = true;
		decoders[i] = std::move(codec);
	}
}

AndroidNDKState::~AndroidNDKState()
{
	for (const UniqueMediaCodec &codec: decoders)
	{
		if (codec)
			NAV_FFCALL(AMediaCodec_stop)(codec.get());
	}
}

Backend *AndroidNDKState::getBackend() const noexcept
{
	return f;
}

size_t AndroidNDKState::getStreamCount() const noexcept
{
	return activeStream.size();
}

const nav_streaminfo_t *AndroidNDKState::getStreamInfo(size_t index) const noexcept
{
	if (index >= streamInfo.size())
	{
		nav::error::set("Stream index out of range");
		return nullptr;
	}

	return &streamInfo[index];
}

bool AndroidNDKState::isStreamEnabled(size_t i) const noexcept
{
	if (i >= activeStream.size())
	{
		nav::error::set("Stream index out of range");
		return false;
	}

	return activeStream.at(i);
}

bool AndroidNDKState::setStreamEnabled(size_t i, bool enabled)
{
	if (i >= activeStream.size())
	{
		nav::error::set("Stream index out of range");
		return false;
	}

	media_status_t status;
    if (enabled)
        status = NAV_FFCALL(AMediaExtractor_selectTrack)(extractor.get(), i);
    else
        status = NAV_FFCALL(AMediaExtractor_unselectTrack)(extractor.get(), i);

	if (setErrorOnFail(status))
		activeStream[i] = enabled;

	return status == AMEDIA_OK;
}

double AndroidNDKState::getDuration() noexcept
{
	return derationalize<int64_t>(durationUs, MICROSECOND);
}

double AndroidNDKState::getPosition() noexcept
{
	return derationalize<int64_t>(positionUs, MICROSECOND);
}

double AndroidNDKState::setPosition(double off)
{
	int64_t targetPosUs = (int64_t) (off * double(MICROSECOND));
	media_status_t result = NAV_FFCALL(AMediaExtractor_seekTo)(extractor.get(), targetPosUs, AMEDIAEXTRACTOR_SEEK_PREVIOUS_SYNC);
	checkError(result);

	for (const UniqueMediaCodec &codec: decoders)
	{
		if (codec)
			NAV_FFCALL(AMediaCodec_flush)(codec.get());
	}

	for (std::vector<bool>::reference v: hasEOS)
		v = false;

	positionUs = targetPosUs;
	return off;
}

nav_frame_t *AndroidNDKState::read()
{
	ssize_t bufferIndex = -1;
	nav::error::set("");

	while (true)
	{
		int index = NAV_FFCALL(AMediaExtractor_getSampleTrackIndex)(extractor.get());
		if (index == -1)
		{
			// EOF
			nav::error::set("");
			return nullptr;
		}

		UniqueMediaCodec &codec = decoders[index];
		if (codec && !hasEOS[index])
		{
			// Step 1: Pull decoded data from codec.
			AMediaCodecBufferInfo bufferInfo = {};
			bufferIndex = NAV_FFCALL(AMediaCodec_dequeueOutputBuffer)(codec.get(), &bufferInfo, DEFAULT_TIMEOUT);

			if (bufferIndex < AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED)
			{
                setErrorOnFail((media_status_t) bufferIndex);
                return nullptr;
            }
            else if (bufferIndex >= 0)
            {
				hasEOS[index] = bufferInfo.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM;

				// Pull buffer
				size_t bufferSize = 0;
				uint8_t *buffer = NAV_FFCALL(AMediaCodec_getOutputBuffer)(codec.get(), bufferIndex, &bufferSize);
				FrameVector *result = nullptr;
				try
				{
					result = new FrameVector(
						&streamInfo[index],
						index,
						derationalize(bufferInfo.presentationTimeUs, MICROSECOND),
						buffer,
						bufferSize
					);
				} catch (const std::exception &e)
				{
					nav::error::set(e);
				}

				NAV_FFCALL(AMediaCodec_releaseOutputBuffer)(codec.get(), bufferIndex, false);
				return result;
			}

			if (needAdvance)
				NAV_FFCALL(AMediaExtractor_advance)(extractor.get());
			needAdvance = true;
			index = NAV_FFCALL(AMediaExtractor_getSampleTrackIndex)(extractor.get());
			if (index == -1)
			{
				// EOF
				nav::error::set("");
				return nullptr;
			}

			// Step 2: Push encoded data to codec.
			if (!hasEOS[index])
			{
				bufferIndex = NAV_FFCALL(AMediaCodec_dequeueInputBuffer)(codec.get(), -1);
				if (bufferIndex < AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED)
				{
					setErrorOnFail((media_status_t) bufferIndex);
					return nullptr;
				}

				size_t bufferSize = 0;
				uint8_t *bufferData = NAV_FFCALL(AMediaCodec_getInputBuffer)(codec.get(), bufferIndex, &bufferSize);
				ssize_t sampleSize = NAV_FFCALL(AMediaExtractor_readSampleData)(extractor.get(), bufferData, bufferSize);
				int64_t presentationTimeUs = NAV_FFCALL(AMediaExtractor_getSampleTime)(extractor.get());
				bool eos = sampleSize < 0;
				sampleSize = std::max<ssize_t>(sampleSize, 0);

				NAV_FFCALL(AMediaCodec_queueInputBuffer)(
					codec.get(),
					bufferIndex,
					0,
					sampleSize,
					presentationTimeUs,
					eos * AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM
				);
			}
		}
	}
}

#undef NAV_FFCALL
#define NAV_FFCALL(n) ptr_##n

AndroidNDKBackend::AndroidNDKBackend()
: mediandk("libmediandk.so")
#define _NAV_PROXY_FUNCTION_POINTER(n) , ptr_##n(nullptr)
#include "AndroidNDKPointers.h"
#undef _NAV_PROXY_FUNCTION_POINTER
{
	if (
#define _NAV_PROXY_FUNCTION_POINTER(n) !mediandk.get(#n, &ptr_##n) ||
#include "AndroidNDKPointers.h"
#undef _NAV_PROXY_FUNCTION_POINTER
		!true // needed to fix the preprocessor stuff
	)
		throw std::runtime_error("Cannot load libmediandk.so function pointer");
}

AndroidNDKBackend::~AndroidNDKBackend()
{}

State *AndroidNDKBackend::open(nav_input *input, const char *filename, [[maybe_unused]] const nav_settings *settings)
{
	UniqueMediaExtractor extractor(NAV_FFCALL(AMediaExtractor_new)(), NAV_FFCALL(AMediaExtractor_delete));
	if (!extractor)
		throw std::runtime_error("Cannot allocate AMediaExtractor");

	MediaSourceWrapper mediaSource(this, input);
	checkError(NAV_FFCALL(AMediaExtractor_setDataSourceCustom)(extractor.get(), mediaSource.get()));

	return new AndroidNDKState(this, extractor, mediaSource);
}

const char *AndroidNDKBackend::getName() const noexcept
{
	return "android";
}

nav_backendtype AndroidNDKBackend::getType() const noexcept
{
	return NAV_BACKENDTYPE_OS_API;
}

const char *AndroidNDKBackend::getInfo()
{
	return nullptr;
}

Backend *create()
{
	if (checkBackendDisabled("ANDROIDNDK"))
		return nullptr;

	try
	{
		return new AndroidNDKBackend();
	}
	catch(const std::exception& e)
	{
		nav::error::set(e);
		return nullptr;
	}
}

}


#endif /* NAV_BACKEND_ANDROIDNDK */
