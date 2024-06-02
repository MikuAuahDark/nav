#ifndef _NAV_INTERNAL_HPP_
#define _NAV_INTERNAL_HPP_

#ifndef _NAV_IMPLEMENTATION_
#define _NAV_IMPLEMENTATION_
#endif

#include <cstdint>

#include "nav/audioformat.h"
#include "nav/types.h"

namespace nav
{

typedef nav_t State;
typedef nav_audio_t AudioState;
typedef nav_video_t VideoState;

}

struct nav_audio_t
{
public:
	virtual ~nav_audio_t() = 0;

	virtual uint32_t getSampleRate() noexcept = 0;
	virtual uint32_t getChannelCount() noexcept = 0;
	virtual uint64_t getSampleCount() = 0;
	virtual nav_audioformat getFormat() = 0;
	virtual uint64_t tell() = 0;
	virtual bool seek(uint64_t off) = 0;
	virtual size_t decode(void *dest, size_t nsamples) = 0;
};

struct nav_video_t
{
public:
	virtual ~nav_video_t() = 0;

	virtual void getDimensions(uint32_t &width, uint32_t &height) noexcept = 0;
	virtual nav_pixelformat getPixelFormat() noexcept = 0;
	virtual double getDuration() = 0;
	virtual double tell() = 0;
	virtual bool seek(double off) = 0;
	virtual double decode(void *dest) = 0;
};

struct nav_t
{
public:
	virtual ~nav_t() = 0;

	virtual size_t getAudioStreamCount() noexcept = 0;
	virtual size_t getVideoStreamCount() noexcept = 0;
	virtual nav::AudioState *openAudioStream(size_t index) = 0;
	virtual nav::VideoState *openVideoStream(size_t index) = 0;
};

#endif /* _NAV_INTERNAL_HPP_ */
