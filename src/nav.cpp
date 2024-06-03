#include <exception>

#include "nav_internal.hpp"
#include "nav_error.hpp"
#include "nav_input_file.hpp"
#include "nav_input_memory.hpp"

#include "nav.h"

template<typename T, typename C, typename... Args, typename... UArgs>
T wrapcall(C *c, T(C::*m)(Args...), T defval, UArgs... args)
{
    try
	{
		return (c->*m)(args...);
	}
    catch (const std::exception &e)
	{
		return defval;
	}
}

extern "C" void nav_input_populate_from_memory(nav_input *input, void *buf, size_t size)
{
	nav::input::memory::populate(input, buf, size);
}

extern "C" nav_bool nav_input_populate_from_file(nav_input *input, const char *filename)
{
	return (nav_bool) nav::input::file::populate(input, filename);
}

extern "C" nav_t *nav_open(nav_input *input)
{
	return nullptr;
}

extern "C" void nav_close(nav_t *state)
{
	delete state;
}

extern "C" size_t nav_naudio(nav_t *state)
{
	return state->getAudioStreamCount();
}

extern "C" size_t nav_nvideo(nav_t *state)
{
	return state->getVideoStreamCount();
}

extern "C" nav_audio_t *nav_open_audio(nav_t *state, size_t index)
{
	return wrapcall<nav_audio_t*>(state, nav::State::openAudioStream, nullptr, index);
}

extern "C" size_t nav_audio_size(int nchannels, nav_audioformat format)
{
	return (size_t) nchannels * NAV_AUDIOFORMAT_BYTESIZE(format);
}

extern "C" uint32_t nav_audio_sample_rate(nav_audio_t *astate)
{
	return astate->getSampleRate();
}

extern "C" uint32_t nav_audio_nchannels(nav_audio_t *astate)
{
	return astate->getChannelCount();
}

extern "C" uint64_t nav_audio_nsamples(nav_audio_t *astate)
{
	return wrapcall(astate, nav::AudioState::getSampleCount, (uint64_t)-1);
}

extern "C" nav_audioformat nav_audio_format(nav_audio_t *astate)
{
	return astate->getFormat();
}

extern "C" uint64_t nav_audio_tell(nav_audio_t *astate)
{
	return wrapcall(astate, nav::AudioState::tell, (uint64_t)-1);
}

extern "C" nav_bool nav_audio_seek(nav_audio_t *astate, uint64_t off)
{
	return (nav_bool) wrapcall(astate, nav::AudioState::seek, false);
}

extern "C" size_t nav_audio_get_samples(nav_audio_t *astate, void *dest, size_t nsmp)
{
	return wrapcall(astate, nav::AudioState::decode, (size_t)-1);
}

extern "C" void nav_audio_close(nav_audio_t *astate)
{
	delete astate;
}

extern "C" nav_video_t *nav_open_video(nav_t *state, size_t index)
{
	return wrapcall<nav_video_t*>(state, nav::State::openVideoStream, nullptr, index);
}

extern "C" size_t nav_video_size(uint32_t width, uint32_t height, nav_pixelformat format)
{
	size_t dimensions = (size_t) width * (size_t) height;

	switch (format)
	{
		case NAV_PIXELFORMAT_UNKNOWN:
		default:
			return 0;
		case NAV_PIXELFORMAT_RGB8:
		case NAV_PIXELFORMAT_YUV444:
			return 3 * dimensions;
		case NAV_PIXELFORMAT_YUV420:
			return dimensions + 2 * (width + 1) / 2 * (height + 1) / 2;
	}
}

extern "C" void nav_video_dimensions(nav_video_t *vstate, uint32_t *width, uint32_t *height)
{
	return vstate->getDimensions(*width, *height);
}

extern "C" nav_pixelformat nav_video_pixel_format(nav_video_t *vstate)
{
	return vstate->getPixelFormat();
}

extern "C" double nav_video_duration(nav_video_t *vstate)
{
	return wrapcall(vstate, nav::VideoState::getDuration, -1.);
}

extern "C" double nav_video_tell(nav_video_t *vstate)
{
	return wrapcall(vstate, nav::VideoState::tell, -1.);
}

extern "C" nav_bool nav_video_seek(nav_video_t *vstate, double off)
{
	return (nav_bool) wrapcall(vstate, nav::VideoState::seek, false);
}

extern "C" double nav_video_get_frame(nav_video_t *vstate, void *dst)
{
	return wrapcall(vstate, nav::VideoState::decode, -1., dst);
}

extern "C" void nav_video_close(nav_video_t *vstate)
{
	delete vstate;
}
