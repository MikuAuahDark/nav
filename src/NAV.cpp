#include <exception>
#include <mutex>
#include <numeric>
#include <thread>
#include <vector>

#include "Internal.hpp"
#include "Backend.hpp"
#include "Common.hpp"
#include "androidndk/AndroidNDKBackend.hpp"
#include "ffmpeg4/FFmpeg4Backend.hpp"
#include "ffmpeg5/FFmpeg5Backend.hpp"
#include "ffmpeg6/FFmpeg6Backend.hpp"
#include "ffmpeg7/FFmpeg7Backend.hpp"
#include "ffmpeg8/FFmpeg8Backend.hpp"
#include "gstreamer/GStreamerBackend.hpp"
#include "mediafoundation/MediaFoundationBackend.hpp"
#include "Error.hpp"
#include "InputFile.hpp"
#include "InputMemory.hpp"

#include "nav/nav.h"

static class BackendContainer
{
public:
	BackendContainer(std::initializer_list<nav::Backend*(*)()> backendlist)
	: initialized(false)
	, activeBackend()
	, factory(backendlist)
	, mutex()
	{}

	~BackendContainer()
	{
		for (nav::Backend *b: activeBackend)
			delete b;
	}

	void init()
	{
		std::lock_guard lg(mutex);

		if (!initialized)
		{
			for (auto func: factory)
			{
				if (func)
				{
					nav::Backend *b = func();

					if (b != nullptr)
						activeBackend.push_back(b);
				}
			}

			for (size_t i = 0; i < activeBackend.size(); i++)
				defaultOrder.push_back(i + 1);
			defaultOrder.push_back(0);

			defaultSettings = {
				NAV_SETTINGS_VERSION,
				nullptr,
				std::max<uint32_t>(std::thread::hardware_concurrency(), 1),
				nav::getEnvvarBool("NAV_DISABLE_HWACCEL")
			};
			if (std::optional<int> threadCount = nav::getEnvvarInt("NAV_THREAD_COUNT"))
				defaultSettings.max_threads = (uint32_t) std::max(threadCount.value(), 1);

			initialized = true;
		}

	}

	inline void ensureInit()
	{
		if (!initialized)
			init();
	}

	nav::State *open(nav_input *input, const char *filename, const nav_settings *settings)
	{
		ensureInit();

		if (settings == nullptr)
			settings = &defaultSettings;

		nav_settings newSettings = *settings;
		newSettings.max_threads = std::max<uint32_t>(newSettings.max_threads, 1);

		std::vector<std::string> errors;
		const size_t *order = newSettings.backend_order ? newSettings.backend_order : defaultOrder.data();

		for (size_t backendIndex = *order; *order; order++)
		{
			if (backendIndex > 0 && backendIndex <= activeBackend.size())
			{
				nav::Backend *b = activeBackend[backendIndex - 1];

				try
				{
					return b->open(input, filename, &newSettings);
				}
				catch (const std::exception &e)
				{
					errors.push_back(e.what());
				}
			}
		}

		if (errors.empty())
			nav::error::set("No backend available");
		else
			nav::error::set(std::reduce(errors.begin(), errors.end(), std::string("\n")));
		return nullptr;
	}

	size_t count()
	{
		ensureInit();
		return activeBackend.size();
	}

	nav::Backend *getBackend(size_t i)
	{
		ensureInit();

		if (i > 0 && i <= activeBackend.size())
			return activeBackend[i - 1];

		nav::error::set("Index out of range");
		return nullptr;
	}

	size_t getBackendIndex(nav::Backend *backend)
	{
		for (size_t i = 0; i < activeBackend.size(); i++)
		{
			if (activeBackend[i] == backend)
				return i + 1;
		}

		return 0;
	}

private:
	bool initialized;
	std::vector<nav::Backend*> activeBackend;
	std::vector<nav::Backend*(*)()> factory;
	std::vector<size_t> defaultOrder;
	std::mutex mutex;
	nav_settings defaultSettings;
} backendContainer({
#ifdef NAV_BACKEND_FFMPEG_8
	&nav::ffmpeg8::create,
#endif
#ifdef NAV_BACKEND_FFMPEG_7
	&nav::ffmpeg7::create,
#endif
#ifdef NAV_BACKEND_FFMPEG_6
	&nav::ffmpeg6::create,
#endif
#ifdef NAV_BACKEND_FFMPEG_5
	&nav::ffmpeg5::create,
#endif
#ifdef NAV_BACKEND_FFMPEG_4
	&nav::ffmpeg4::create,
#endif
#ifdef NAV_BACKEND_ANDROIDNDK
	&nav::androidndk::create,
#endif
#ifdef NAV_BACKEND_GSTREAMER
	&nav::gstreamer::create,
#endif
#ifdef NAV_BACKEND_MEDIAFOUNDATION
	&nav::mediafoundation::create,
#endif
	nullptr
});

template<typename T, typename C, typename... Args, typename... UArgs>
T wrapcall(C *c, T(C::*m)(Args...), T defval, UArgs... args)
{
    try
	{
		nav::error::set("");
		T result = (c->*m)(args...);
		return result;
	}
    catch (const std::exception &e)
	{
		nav::error::set(e.what());
		return defval;
	}
}

extern "C" uint32_t nav_version()
{
	return NAV_VERSION;
}

extern "C" const char *nav_version_string()
{
	return NAV_STRINGIZE(NAV_VERSION_MAJOR) "." NAV_STRINGIZE(NAV_VERSION_MINOR) "." NAV_STRINGIZE(NAV_VERSION_PATCH);
}

extern "C" const char *nav_error()
{
	return nav::error::get();
}

extern "C" void nav_input_populate_from_memory(nav_input *input, void *buf, size_t size)
{
	nav::error::set("");
	nav::input::memory::populate(input, buf, size);
}

extern "C" nav_bool nav_input_populate_from_file(nav_input *input, const char *filename)
{
	nav::error::set("");
	return (nav_bool) nav::input::file::populate(input, filename);
}

extern "C" size_t nav_backend_count()
{
	nav::error::set("");
	return backendContainer.count();
}

extern "C" const char *nav_backend_name(size_t index)
{
	nav::error::set("");
	nav::Backend *backend = backendContainer.getBackend(index);
	return backend ? backend->getName() : nullptr;
}

extern "C" nav_backendtype nav_backend_type(size_t index)
{
	nav::error::set("");
	nav::Backend *backend = backendContainer.getBackend(index);
	return backend ? backend->getType() : NAV_BACKENDTYPE_UNKNOWN;
}

extern "C" const char *nav_backend_info(size_t index)
{
	nav::error::set("");
	nav::Backend *backend = backendContainer.getBackend(index);
	return backend ? wrapcall<const char*>(backend, &nav::Backend::getInfo, nullptr) : nullptr;
}

extern "C" nav_t *nav_open(nav_input *input, const char *filename, const nav_settings *settings)
{
	return wrapcall<nav_t*>(&backendContainer, &BackendContainer::open, nullptr, input, filename, settings);
}

extern "C" void nav_close(nav_t *state)
{
	nav::error::set("");
	delete state;
}

extern "C" size_t nav_backend_index(const nav_t *state)
{
	nav::error::set("");
	nav::Backend *b = state->getBackend();
	return backendContainer.getBackendIndex(b);
}

extern "C" size_t nav_nstreams(const nav_t *state)
{
	nav::error::set("");
	return state->getStreamCount();
}

extern "C" const nav_streaminfo_t *nav_stream_info(const nav_t *state, size_t index)
{
	nav::error::set("");
	return state->getStreamInfo(index);
}

extern "C" nav_bool nav_stream_is_enabled(const nav_t *state, size_t index)
{
	nav::error::set("");
	return (nav_bool) state->isStreamEnabled(index);
}

extern "C" nav_bool nav_stream_enable(nav_t *state, size_t index, nav_bool enable)
{
	return (nav_bool) wrapcall(state, &nav::State::setStreamEnabled, false, index, enable);
}

extern "C" double nav_tell(nav_t *state)
{
	nav::error::set("");
	return state->getPosition();
}

extern "C" double nav_duration(nav_t *state)
{
	nav::error::set("");
	return state->getDuration();
}

extern "C" double nav_seek(nav_t *state, double position)
{
	return wrapcall(state, &nav::State::setPosition, -1., position);
}

extern "C" bool nav_prepare(nav_t *state)
{
	return wrapcall(state, &nav::State::prepare, false);
}

extern "C" bool nav_is_prepared(const nav_t *state)
{
	nav::error::set("");
	return state->isPrepared();
}

extern "C" nav_frame_t *nav_read(nav_t *state)
{
	if (!nav_prepare(state))
		return nullptr;

	return wrapcall<nav_frame_t*>(state, &nav::State::read, nullptr);
}

extern "C" nav_streamtype nav_streaminfo_type(const nav_streaminfo_t *sinfo)
{
	nav::error::set("");
	return sinfo->type;
}

extern "C" size_t nav_video_plane_count(nav_pixelformat fmt)
{
	nav::error::set("");
	return nav::planeCount(fmt);
}

extern "C" size_t nav_audio_size(const nav_streaminfo_t *sinfo)
{
	if (sinfo->type != NAV_STREAMTYPE_AUDIO)
	{
		nav::error::set("Not an audio stream");
		return 0;
	}

	nav::error::set("");
	return sinfo->audio.size();
}

extern "C" uint32_t nav_audio_sample_rate(const nav_streaminfo_t *sinfo)
{
	if (sinfo->type != NAV_STREAMTYPE_AUDIO)
	{
		nav::error::set("Not an audio stream");
		return 0;
	}

	nav::error::set("");
	return sinfo->audio.sample_rate;
}

extern "C" uint32_t nav_audio_nchannels(const nav_streaminfo_t *sinfo)
{
	if (sinfo->type != NAV_STREAMTYPE_AUDIO)
	{
		nav::error::set("Not an audio stream");
		return 0;
	}

	nav::error::set("");
	return sinfo->audio.nchannels;
}

extern "C" nav_audioformat nav_audio_format(const nav_streaminfo_t *sinfo)
{
	if (sinfo->type != NAV_STREAMTYPE_AUDIO)
	{
		nav::error::set("Not an audio stream");
		return 0;
	}

	nav::error::set("");
	return sinfo->audio.format;
}

extern "C" size_t nav_video_size(const nav_streaminfo_t *sinfo)
{
	if (sinfo->type != NAV_STREAMTYPE_VIDEO)
	{
		nav::error::set("Not a video stream");
		return 0;
	}

	nav::error::set("");
	size_t result = 0;
	for (size_t i = 0; i < nav::planeCount(sinfo->video.format); i++)
		result += sinfo->plane_width(i) * sinfo->plane_height(i);
	
	return result;
}

extern "C" void nav_video_plane_dimensions(const nav_streaminfo_t *sinfo, size_t index, size_t *width, size_t *height)
{
	if (sinfo->type != NAV_STREAMTYPE_VIDEO)
	{
		nav::error::set("Not a video stream");
		return;
	}

	nav::error::set("");
	*width = sinfo->plane_width(index);
	*height = sinfo->plane_height(index);
}

extern "C" void nav_video_dimensions(const nav_streaminfo_t *sinfo, uint32_t *width, uint32_t *height)
{
	if (sinfo->type != NAV_STREAMTYPE_VIDEO)
	{
		nav::error::set("Not a video stream");
		*width = *height = 0;
		return;
	}

	nav::error::set("");
	*width = sinfo->video.width;
	*height = sinfo->video.height;
}

extern "C" nav_pixelformat nav_video_pixel_format(const nav_streaminfo_t *sinfo)
{
	if (sinfo->type != NAV_STREAMTYPE_VIDEO)
	{
		nav::error::set("Not a video stream");
		return NAV_PIXELFORMAT_UNKNOWN;
	}

	nav::error::set("");
	return sinfo->video.format;
}

extern "C" double nav_video_fps(const nav_streaminfo_t *sinfo)
{
	if (sinfo->type != NAV_STREAMTYPE_VIDEO)
	{
		nav::error::set("Not a video stream");
		return 0.0;
	}

	nav::error::set("");
	return sinfo->video.fps;
}

extern "C" size_t nav_frame_streamindex(const nav_frame_t *frame)
{
	nav::error::set("");
	return frame->getStreamIndex();
}

extern "C" const nav_streaminfo_t *nav_frame_streaminfo(const nav_frame_t *frame)
{
	nav::error::set("");
	return frame->getStreamInfo();
}

extern "C" double nav_frame_tell(const nav_frame_t *frame)
{
	nav::error::set("");
	return frame->tell();
}

extern "C" nav_hwacceltype nav_frame_hwacceltype(const nav_frame_t *frame)
{
	nav::error::set("");
	return frame->getHWAccelType();
}

extern "C" const uint8_t *const *nav_frame_acquire(nav_frame_t *frame, ptrdiff_t **strides, size_t *nplanes)
{
	return wrapcall<const uint8_t *const *>(frame, &nav::Frame::acquire, nullptr, strides, nplanes);
}

extern "C" void *nav_frame_hwhandle(nav_frame_t *frame)
{
	return wrapcall<void*>(frame, &nav::Frame::getHWAccelHandle, nullptr);
}

extern "C" void nav_frame_release(nav_frame_t *frame)
{
	nav::error::set("");
	return frame->release();
}

extern "C" void nav_frame_free(nav_frame_t *frame)
{
	frame->release();
	delete frame;
}
