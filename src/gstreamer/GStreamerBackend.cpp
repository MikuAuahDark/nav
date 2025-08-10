#include "GStreamerBackend.hpp"

#ifdef NAV_BACKEND_GSTREAMER

#include <sstream>
#include <stdexcept>

#include <gst/video/video.h>

#include "GStreamerInternal.hpp"
#include "Error.hpp"

namespace nav::gstreamer
{

#define NAV_FFCALL(n) f->ptr_##n

// These are GStreamer internals, but we have no choice.
// We do this because we REALLY don't want to link with GStreamer.
#define NAV_GST_TYPE_INT_RANGE (*NAV_FFCALL(_gst_int_range_type))
#define NAV_GST_TYPE_LIST (*NAV_FFCALL(_gst_value_list_type))
#define NAV_GST_TYPE_FRACTION_RANGE (*NAV_FFCALL(_gst_fraction_range_type))

constexpr struct NAVGstPixelFormatMap
{
	const char *gstName;
	nav_pixelformat format;
} NAV_PIXELFORMAT_MAP[] = {
	// When NAV supports more pixel format, add it here.
	{"Y444", NAV_PIXELFORMAT_YUV444},
	{"I420", NAV_PIXELFORMAT_YUV420},
	{"NV12", NAV_PIXELFORMAT_NV12},
	{"RGB", NAV_PIXELFORMAT_RGB8}
};

constexpr struct NAVGstAudioFormatMap
{
	const char *gstName;
	nav_audioformat format;
} NAV_AUDIOFORMAT_MAP[] = {
	// TODO: Add support for big endian and other exotic formats?
	{"F64LE", makeAudioFormat(64, true, true)},
	{"F32LE", makeAudioFormat(32, true, true)},
	{"S32LE", makeAudioFormat(32, false, true)},
	{"S16LE", makeAudioFormat(16, false, true)},
	{"U8", makeAudioFormat(8, false, false)}
};

template<typename T>
T *G_CAST(GStreamerBackend *f, GType type, void *obj)
{
	return (T*) NAV_FFCALL(g_type_check_instance_cast)((GTypeInstance*) obj, type);
}

template<typename T>
std::string G_TOSTRCALL(GStreamerBackend *f, gchar*(*func)(T *obj), T *obj)
{
	gchar *chr = func(obj);
	std::string result = (char*) chr;
	NAV_FFCALL(g_free)(chr);
	return result;
}

inline std::string G_TOSTRCALL(GStreamerBackend *f, gchar*(*func)())
{
	gchar *chr = func();
	std::string result = (char*) chr;
	NAV_FFCALL(g_free)(chr);
	return result;
}

struct GstMemoryMapLock: GstMapInfo
{
	GstMemoryMapLock(const GstMemoryMapLock &) = delete;
	GstMemoryMapLock(const GstMemoryMapLock &&) = delete;
	GstMemoryMapLock(GStreamerBackend *f, GstMemory *mem, GstMapFlags flags, bool throwOnFail)
	: GstMapInfo({})
	, f(f)
	, memory(mem)
	, success(false)
	{
		success = NAV_FFCALL(gst_memory_map)(mem, this, flags);

		if (!success && throwOnFail)
			throw std::runtime_error("Cannot map memory");
	}

	~GstMemoryMapLock()
	{
		if (success)
			NAV_FFCALL(gst_memory_unmap)(memory, this);
	}

	GStreamerBackend *f;
	GstMemory *memory;
	bool success;
};



GStreamerAudioFrame::GStreamerAudioFrame(
	GStreamerBackend *backend,
	GstBuffer *buf,
	nav_streaminfo_t *sinfo,
	size_t si,
	double pts
)
: acquireData()
, mapInfo(GST_MAP_INFO_INIT)
, memory(nullptr)
, pts(pts)
, f(backend)
, buffer(nullptr)
, streamInfo(sinfo)
, streamIndex(si)
{
	buffer = NAV_FFCALL(gst_buffer_ref)(buf);
	memory = NAV_FFCALL(gst_buffer_get_all_memory)(buffer);
}

GStreamerAudioFrame::~GStreamerAudioFrame()
{
	NAV_FFCALL(gst_buffer_unref)(buffer);
}

size_t GStreamerAudioFrame::getStreamIndex() const noexcept
{
	return streamIndex;
}

const nav_streaminfo_t *GStreamerAudioFrame::getStreamInfo() const noexcept
{
	return streamInfo;
}

double GStreamerAudioFrame::tell() const noexcept
{
	return pts;
}

const uint8_t *const *GStreamerAudioFrame::acquire(ptrdiff_t **strides, size_t *nplanes)
{
	if (acquireData.source == nullptr)
	{
		if (!NAV_FFCALL(gst_memory_map)(memory, &mapInfo, GST_MAP_READ))
			throw std::runtime_error("Cannot map memory");

		acquireData.source = (uint8_t *) mapInfo.data;
		acquireData.planes.push_back(acquireData.source);
		acquireData.strides.push_back((ptrdiff_t) mapInfo.size);
	}

	if (nplanes)
		*nplanes = acquireData.planes.size();
	*strides = acquireData.strides.data();
	return acquireData.planes.data();
}

void GStreamerAudioFrame::release() noexcept
{
	if (acquireData.source)
	{
		NAV_FFCALL(gst_memory_unmap)(memory, &mapInfo);
		acquireData = {};
	}
}

nav_hwacceltype GStreamerAudioFrame::getHWAccelType() const noexcept
{
	return NAV_HWACCELTYPE_NONE;
}

void *GStreamerAudioFrame::getHWAccelHandle()
{
	nav::error::set("Not hardware accelerated");
	return nullptr;
}


GStreamerVideoFrame::GStreamerVideoFrame(
	GStreamerBackend *backend,
	UniqueGst<GstVideoInfo> &&videoInfo,
	GstBuffer *buffer,
	nav_streaminfo_t *sinfo,
	size_t si,
	double pts
)
: acquireData()
, videoFrame()
, videoInfo(std::move(videoInfo))
, pts(pts)
, f(backend)
, buffer(nullptr)
, streamInfo(sinfo)
, streamIndex(si)
{
	this->buffer = NAV_FFCALL(gst_buffer_ref)(buffer);
}

GStreamerVideoFrame::~GStreamerVideoFrame()
{
	NAV_FFCALL(gst_buffer_unref)(buffer);
}

size_t GStreamerVideoFrame::getStreamIndex() const noexcept
{
	return streamIndex;
}

const nav_streaminfo_t *GStreamerVideoFrame::getStreamInfo() const noexcept
{
	return streamInfo;
}

double GStreamerVideoFrame::tell() const noexcept
{
	return pts;
}

const uint8_t *const *GStreamerVideoFrame::acquire(ptrdiff_t **strides, size_t *nplanes)
{
	if (!acquireData.source)
	{
		if (!NAV_FFCALL(gst_video_frame_map)(&videoFrame, videoInfo.get(), buffer, GST_MAP_READ))
			throw std::runtime_error("Cannot map video frame");

		acquireData.source = (uint8_t *) videoFrame.buffer; // Just for marking
		size_t nplanes = planeCount(streamInfo->video.format);
		if (nplanes != GST_VIDEO_FRAME_N_PLANES(&videoFrame))
			throw std::logic_error("Planes does not match");

		acquireData.planes.resize(nplanes);
		acquireData.strides.resize(nplanes);

		for (size_t i = 0; i < nplanes; i++)
		{
			acquireData.planes[i] = (uint8_t *) GST_VIDEO_FRAME_PLANE_DATA(&videoFrame, i);
			acquireData.strides[i] = GST_VIDEO_FRAME_PLANE_STRIDE(&videoFrame, i);
		}
	}

	if (nplanes)
		*nplanes = acquireData.planes.size();
	*strides = acquireData.strides.data();
	return acquireData.planes.data();
}

void GStreamerVideoFrame::release() noexcept
{
	if (acquireData.source)
	{
		NAV_FFCALL(gst_video_frame_unmap)(&videoFrame);
		acquireData = {};
	}
}

nav_hwacceltype GStreamerVideoFrame::getHWAccelType() const noexcept
{
	return NAV_HWACCELTYPE_NONE;
}

void *GStreamerVideoFrame::getHWAccelHandle()
{
	nav::error::set("Not yet implemented");
	return nullptr;
}


GStreamerState::GStreamerState(GStreamerBackend *backend, nav_input *input, const nav_settings *settings)
: f(backend)
, input(*input)
, bus(nullptr, NAV_FFCALL(gst_object_unref))
, pipeline(nullptr, NAV_FFCALL(gst_object_unref))
, source(nullptr)
, parsebin(nullptr)
, streams()
, streamInfoMutex()
, queuedFrames()
, padProbeCount(0)
, eos(false)
, prepared(false)
, disableHWAccel(settings->disable_hwaccel)
{
	// The pipeline idea:
	// appsrc -> parsebin -> (for each stream) [decode]
	//
	// [decode] pipeline is defined as follows:
	// ... -> output-selector -> decodebin -> appsink (enabled stream)
	//                   | or -> fakesink (in case of disabled stream)

	UniqueGstElement sourceUq {
		NAV_FFCALL(gst_element_factory_make)("appsrc", nullptr),
		NAV_FFCALL(gst_object_unref)
	};
	if (!sourceUq)
		throw std::runtime_error("Unable to create appsrc element.");

	UniqueGstElement parsebinUq {
		NAV_FFCALL(gst_element_factory_make)("parsebin", nullptr),
		NAV_FFCALL(gst_object_unref)
	};
	if (!parsebinUq)
		throw std::runtime_error("Unable to create parsebin element.");

	pipeline.reset(NAV_FFCALL(gst_pipeline_new)(nullptr));
	if (!pipeline)
		throw std::runtime_error("Unable to create pipeline element.");

	NAV_FFCALL(g_signal_connect_data)(sourceUq.get(), "need-data", G_CALLBACK(needData), this, nullptr, (GConnectFlags) 0);
	NAV_FFCALL(g_signal_connect_data)(sourceUq.get(), "seek-data", G_CALLBACK(seekData), this, nullptr, (GConnectFlags) 0);
	NAV_FFCALL(g_object_set)(sourceUq.get(),
		"emit-signals", 1,
		"size", (gint64) input->sizef(),
		nullptr
	);
	NAV_FFCALL(gst_util_set_object_arg)(G_CAST<GObject>(f, G_TYPE_OBJECT, sourceUq.get()), "stream-type", "random-access");
	NAV_FFCALL(g_signal_connect_data)(parsebinUq.get(), "pad-added", G_CALLBACK(padAdded), this, nullptr, (GConnectFlags) 0);
	NAV_FFCALL(g_signal_connect_data)(parsebinUq.get(), "no-more-pads", G_CALLBACK(noMorePads), this, nullptr, (GConnectFlags) 0);

	// Add
	NAV_FFCALL(gst_bin_add_many)(G_CAST<GstBin>(f, NAV_FFCALL(gst_bin_get_type)(), pipeline.get()),
		sourceUq.get(),
		parsebinUq.get(),
		nullptr
	);

	// Note: gst_bin_add_many takes ownership of the element.
	source = sourceUq.release();
	parsebin = parsebinUq.release();

	// Just link the appsrc and the decodebin for now.
	if (!NAV_FFCALL(gst_element_link)(source, parsebin))
		throw std::runtime_error("Unable to link source and parsebin.");

	// Pause
	padProbeCount++;
	GstStateChangeReturn ret = NAV_FFCALL(gst_element_set_state)(pipeline.get(), GST_STATE_PLAYING);
	if (ret == GST_STATE_CHANGE_FAILURE)
		throw std::runtime_error("Cannot initiate pipeline");

	bus.reset(NAV_FFCALL(gst_element_get_bus)(pipeline.get()));

	while (padProbeCount > 0)
	{
		try
		{
			pollBus();
			if (ret == GST_STATE_CHANGE_ASYNC)
				ret = NAV_FFCALL(gst_element_get_state)(pipeline.get(), nullptr, nullptr, GST_CLOCK_TIME_NONE);
			if (ret == GST_STATE_CHANGE_FAILURE)
				throw std::runtime_error("gst_element_get_state failed");
			
			{
				std::lock_guard lg(streamInfoMutex);

				// Setup and pull streams to unclog it
				for (AppSinkWrapper *sw: streams)
				{
					if (!sw->sinfoOk)
						setupStreamInfo(sw, true);

					if (sw->sinfoOk && !sw->eos && sw->streamInfo.type != NAV_STREAMTYPE_UNKNOWN)
					{
						// Pull sample to unclog
						GstSample *sampleRaw = nullptr;
						NAV_FFCALL(g_signal_emit_by_name)(sw->sink, "try-pull-sample", 0ULL, &sampleRaw);

						if (sampleRaw)
						{
							GstBuffer *buffer = NAV_FFCALL(gst_sample_get_buffer)(sampleRaw);
							queuedFrames.push(dispatchDecode(buffer, sw->streamIndex));
							NAV_FFCALL(gst_sample_unref)(sampleRaw);
						}
						else
						{
							NAV_FFCALL(g_object_get)(sw->sink, "eos", &eos, nullptr);
							if (eos)
								sw->eos = true;
						}
					}
				}
			}
		}
		catch(const std::exception&)
		{
			NAV_FFCALL(gst_element_set_state)(pipeline.get(), GST_STATE_NULL);
			NAV_FFCALL(gst_element_get_state)(pipeline.get(), nullptr, nullptr, GST_CLOCK_TIME_NONE);
			throw;
		}
	}

	// Populate the stream info data
	for (AppSinkWrapper *sw: streams)
	{
		if (sw->ok())
			setupStreamInfo(sw, false);
	}
}

GStreamerState::~GStreamerState()
{
	NAV_FFCALL(gst_element_set_state)(pipeline.get(), GST_STATE_NULL);
	NAV_FFCALL(gst_element_get_state)(pipeline.get(), nullptr, nullptr, GST_CLOCK_TIME_NONE);
	clearQueuedFrames();
	pollBus(true);

	for (AppSinkWrapper *sw: streams)
		delete sw;

	if (input.userdata)
		input.closef();
}

Backend *GStreamerState::getBackend() const noexcept
{
	return f;
}

size_t GStreamerState::getStreamCount() const noexcept
{
	return streams.size();
}

const nav_streaminfo_t *GStreamerState::getStreamInfo(size_t index) const noexcept
{
	if (index >= streams.size())
	{
		nav::error::set("Stream index out of range");
		return nullptr;
	}

	return &streams[index]->streamInfo;
}

bool GStreamerState::isStreamEnabled(size_t index) const noexcept
{
	if (index >= streams.size())
	{
		nav::error::set("Stream index out of range");
		return false;
	}

	return streams[index]->enabled;
}

bool GStreamerState::setStreamEnabled(size_t index, bool enabled)
{
	if (prepared)
	{
		nav::error::set("Decoder already initialized");
		return false;
	}

	if (index >= streams.size())
	{
		nav::error::set("Stream index out of range");
		return false;
	}

	streams[index]->enabled = enabled;
	return true;
}

double GStreamerState::getDuration() noexcept
{
	gint64 dur = 0;

	if (NAV_FFCALL(gst_element_query_duration)(pipeline.get(), GST_FORMAT_TIME, &dur))
		return derationalize<gint64>(dur, GST_SECOND);
	
	nav::error::set("gst_element_query_duration failed");
	return -1;
}

double GStreamerState::getPosition() noexcept
{
	gint64 pos = 0;

	if (NAV_FFCALL(gst_element_query_position)(pipeline.get(), GST_FORMAT_TIME, &pos))
		return derationalize<gint64>(pos, GST_SECOND);
	
	nav::error::set("gst_element_query_position failed");
	return -1;
}

double GStreamerState::setPosition(double off)
{
	gint64 pos = (gint64) (off * GST_SECOND);
	if (!NAV_FFCALL(gst_element_seek_simple)(
		pipeline.get(),
		GST_FORMAT_TIME,
		(GstSeekFlags) (GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT | GST_SEEK_FLAG_SNAP_BEFORE),
		pos
	))
	{
		nav::error::set("gst_element_seek_simple failed");
		return -1;
	}

	eos = false;
	clearQueuedFrames();
	return getPosition();
}

bool GStreamerState::prepare()
{
	if (!prepared)
	{
		GstBin *binFromPipeline = G_CAST<GstBin>(f, NAV_FFCALL(gst_bin_get_type)(), pipeline.get());

		// Insert fakesink for each disabled streams.
		for (size_t i = 0; i < streams.size(); i++)
		{
			AppSinkWrapper *sw = streams[i];
			if (sw->enabled)
				continue;

			UniqueGstElement fakesink {
				NAV_FFCALL(gst_element_factory_make)("fakesink", nullptr),
				NAV_FFCALL(gst_object_unref)
			};
			if (!fakesink)
			{
				nav::error::set("Unable to create fakesink for disabled stream " + std::to_string(i));
				return false;
			}

			if (sw->decodebin)
			{
				// There's decodebin. Unlink all of them first.
				NAV_FFCALL(gst_element_unlink_many)(
					parsebin,
					sw->decodebin,
					sw->convert,
					sw->sink,
					nullptr
				);
				// gst_bin_remove_many unrefs then frees the elements
				NAV_FFCALL(gst_bin_remove_many)(binFromPipeline, sw->decodebin, sw->convert, sw->sink, nullptr);
				sw->decodebin = nullptr;
				sw->convert = nullptr;
				sw->sink = nullptr;
			}

			// Insert fakesink after the parsebin pad
			linkToFakesink(binFromPipeline, sw->parserPad, *sw);
			NAV_FFCALL(gst_element_sync_state_with_parent)(fakesink.get());
		}

		GstStateChangeReturn ret = NAV_FFCALL(gst_element_set_state)(pipeline.get(), GST_STATE_PLAYING);
		if (ret == GST_STATE_CHANGE_FAILURE)
		{
			nav::error::set("Cannot play pipeline");
			return false;
		}

		prepared = true;
	}

	return true;
}

bool GStreamerState::isPrepared() const noexcept
{
	return prepared;
}

nav_frame_t *GStreamerState::read()
{
	while (!eos || !queuedFrames.empty())
	{
		size_t neos = 0;
		size_t nactive = 0;

		// Pull queued frames.
		if (!queuedFrames.empty())
		{
			Frame *front = queuedFrames.top();
			queuedFrames.pop();
			return front;
		}

		// Pull samples from sinks
		for (AppSinkWrapper *sw: streams)
		{
			UniqueGst<GstSample> sample {nullptr, NAV_FFCALL(gst_sample_unref)};
			gboolean eos = 0;

			if (sw->enabled)
			{
				GstSample *sampleRaw = nullptr;
				NAV_FFCALL(g_signal_emit_by_name)(sw->sink, "try-pull-sample", 1000ULL, &sampleRaw); // wait 1us
				sample.reset(sampleRaw);
				nactive++;

				if (sample)
				{
					GstBuffer *buffer = NAV_FFCALL(gst_sample_get_buffer)(sample.get());
					queuedFrames.push(dispatchDecode(buffer, sw->streamIndex));
				}
				else
				{
					NAV_FFCALL(g_object_get)(sw->sink, "eos", &eos, nullptr);
					if (eos)
						sw->eos = true;
				}

				neos = neos + sw->eos;
			}
		}

		if (neos == nactive)
		{
			eos = true;
			return nullptr;
		}

		// Read buses
		pollBus();
	}

	return nullptr;
}

GstCaps *GStreamerState::newVideoCapsForNAV()
{
	GValue format = G_VALUE_INIT;
	NAV_FFCALL(g_value_init)(&format, NAV_GST_TYPE_LIST);

	for (const NAVGstPixelFormatMap &map: NAV_PIXELFORMAT_MAP)
	{
		GValue v = G_VALUE_INIT;
		NAV_FFCALL(g_value_init)(&v, G_TYPE_STRING);
		NAV_FFCALL(g_value_set_static_string)(&v, map.gstName);
		NAV_FFCALL(gst_value_list_append_and_take_value)(&format, &v);
	}

	GstStructure *s = NAV_FFCALL(gst_structure_new)("video/x-raw",
		"width", NAV_GST_TYPE_INT_RANGE, 1, G_MAXINT,
		"height", NAV_GST_TYPE_INT_RANGE, 1, G_MAXINT,
		"interlace-mode", G_TYPE_STRING, "progressive",
		nullptr
	);
	NAV_FFCALL(gst_structure_take_value)(s, "format", &format);

	GstCaps *caps = NAV_FFCALL(gst_caps_new_empty)();
	NAV_FFCALL(gst_caps_append_structure)(caps, s);
	return caps;
}

GstCaps *GStreamerState::newAudioCapsForNAV()
{
	GValue format = G_VALUE_INIT;
	NAV_FFCALL(g_value_init)(&format, NAV_GST_TYPE_LIST);

	for (const NAVGstAudioFormatMap &map: NAV_AUDIOFORMAT_MAP)
	{
		GValue v = G_VALUE_INIT;
		NAV_FFCALL(g_value_init)(&v, G_TYPE_STRING);
		NAV_FFCALL(g_value_set_static_string)(&v, map.gstName);
		NAV_FFCALL(gst_value_list_append_and_take_value)(&format, &v);
	}

	GstStructure *s = NAV_FFCALL(gst_structure_new)("audio/x-raw",
		"rate", NAV_GST_TYPE_INT_RANGE, 1, G_MAXINT,
		"channels", NAV_GST_TYPE_INT_RANGE, 1, G_MAXINT,
		"layout", G_TYPE_STRING, "interleaved",
		nullptr
	);
	NAV_FFCALL(gst_structure_take_value)(s, "format", &format);

	GstCaps *caps = NAV_FFCALL(gst_caps_new_empty)();
	NAV_FFCALL(gst_caps_append_structure)(caps, s);
	return caps;
}

void GStreamerState::clearQueuedFrames()
{
	while (!queuedFrames.empty())
	{
		Frame *fv = queuedFrames.top();
		delete fv;
		queuedFrames.pop();
	}
}

void GStreamerState::pollBus(bool noexc)
{
	while (UniqueGst<GstMessage> message {
		NAV_FFCALL(gst_bus_pop_filtered)(bus.get(), GST_MESSAGE_ANY), NAV_FFCALL(gst_message_unref)
	})
	{
		switch (message->type)
		{
			case GST_MESSAGE_EOS:
				eos = true;
				break;
			case GST_MESSAGE_ERROR:
			{
				if (!noexc)
				{
					gchar *debug_info;
					GError *error;
					NAV_FFCALL(gst_message_parse_error)(message.get(), &error, &debug_info);
					std::stringstream errorMessage;
					errorMessage << error->message;

					if (debug_info)
						errorMessage << " (" << debug_info << ")";

					NAV_FFCALL(g_free)(debug_info);
					NAV_FFCALL(g_error_free)(error);

					throw std::runtime_error(errorMessage.str());
				}
				break;
			}
			default:
				break;
		}
	}
}

Frame *GStreamerState::dispatchDecode(GstBuffer *buffer, size_t streamIndex)
{
	AppSinkWrapper *sw = streams[streamIndex];

	if (sw->streamInfo.type == NAV_STREAMTYPE_VIDEO)
	{
		UniqueGstObject<GstPad> pad {
			NAV_FFCALL(gst_element_get_static_pad)(sw->convert, "src"),
			NAV_FFCALL(gst_object_unref)
		};
		UniqueGst<GstCaps> caps {NAV_FFCALL(gst_pad_get_current_caps)(pad.get()), NAV_FFCALL(gst_caps_unref)};
		UniqueGst<GstVideoInfo> videoInfo{NAV_FFCALL(gst_video_info_new)(), NAV_FFCALL(gst_video_info_free)};

		if (!NAV_FFCALL(gst_video_info_from_caps)(videoInfo.get(), caps.get()))
			return nullptr;

		return new GStreamerVideoFrame(
			f,
			std::move(videoInfo),
			buffer,
			&sw->streamInfo,
			streamIndex,
			derationalize<guint64>(buffer->pts, GST_SECOND)
		);
	}
	else
	{
		return new GStreamerAudioFrame(
			f,
			buffer,
			&sw->streamInfo,
			sw->streamIndex,
			derationalize<guint64>(buffer->pts, GST_SECOND)
		);
	}
}

void GStreamerState::linkToFakesink(GstBin *bin, GstPad *pad, AppSinkWrapper &sw)
{
	UniqueGstElement fakesink {
		NAV_FFCALL(gst_element_factory_make)("fakesink", nullptr),
		NAV_FFCALL(gst_object_unref)
	};
	if (!fakesink)
		return;

	if (!NAV_FFCALL(gst_bin_add)(bin, fakesink.get()))
		return;
		
	sw.sink = fakesink.release();

	UniqueGstObject<GstPad> fakesinkPad {
		NAV_FFCALL(gst_element_get_static_pad)(fakesink.get(), "sink"),
		NAV_FFCALL(gst_object_unref)
	};
	if (GST_PAD_LINK_FAILED(NAV_FFCALL(gst_pad_link)(pad, fakesinkPad.get())))
	{
		NAV_FFCALL(gst_bin_remove)(bin, sw.sink);
		sw.sink = nullptr; // gst_bin_add takes the ownership, gst_bin_remove unrefs it
		return;
	}
}

void GStreamerState::setupStreamInfo(AppSinkWrapper *sw, bool tryAgainLater)
{
	if (!sw->convert && tryAgainLater)
		return;

	UniqueGstObject<GstPad> pad {NAV_FFCALL(gst_element_get_static_pad)(sw->convert, "src"), NAV_FFCALL(gst_object_unref)};
	UniqueGst<GstCaps> caps {NAV_FFCALL(gst_pad_get_current_caps)(pad.get()), NAV_FFCALL(gst_caps_unref)};
	if (tryAgainLater && !caps)
		return;

	while (!caps)
	{
		pollBus();
		caps.reset(NAV_FFCALL(gst_pad_get_current_caps)(pad.get()));
	}

	GstStructure *s = NAV_FFCALL(gst_caps_get_structure)(caps.get(), 0);
	gint temp = 0, temp2 = 0;

	switch (sw->streamInfo.type)
	{
		case NAV_STREAMTYPE_AUDIO:
		{
			const gchar *format = NAV_FFCALL(gst_structure_get_string)(s, "format");
			bool hasFormat = false;

			for (const NAVGstAudioFormatMap &map: NAV_AUDIOFORMAT_MAP)
			{
				if (strcmp(format, map.gstName) == 0)
				{
					sw->streamInfo.audio.format = map.format;
					hasFormat = true;
					break;
				}
			}

			if (hasFormat)
			{
				NAV_FFCALL(gst_structure_get_int)(s, "rate", &temp);
				sw->streamInfo.audio.sample_rate = (uint32_t) temp;
				NAV_FFCALL(gst_structure_get_int)(s, "channels", &temp);
				sw->streamInfo.audio.nchannels = (uint32_t) temp;
			}
			else
			{
				sw->streamInfo.type = NAV_STREAMTYPE_UNKNOWN;
				sw->enabled = false;
			}

			break;
		}
		case NAV_STREAMTYPE_VIDEO:
		{
			const gchar *format = NAV_FFCALL(gst_structure_get_string)(s, "format");
			bool hasFormat = false;

			for (const NAVGstPixelFormatMap &map: NAV_PIXELFORMAT_MAP)
			{
				if (strcmp(format, map.gstName) == 0)
				{
					sw->streamInfo.video.format = map.format;
					hasFormat = true;
					break;
				}
			}

			if (hasFormat)
			{
				gdouble d = 0;

				NAV_FFCALL(gst_structure_get_int)(s, "width", &temp);
				sw->streamInfo.video.width = (uint32_t) temp;
				NAV_FFCALL(gst_structure_get_int)(s, "height", &temp);
				sw->streamInfo.video.height = (uint32_t) temp;

				if (NAV_FFCALL(gst_structure_get_fraction)(s, "framerate", &temp, &temp2))
					sw->streamInfo.video.fps = derationalize(temp, temp2);
				else if (NAV_FFCALL(gst_structure_get_double)(s, "framerate", &d))
					sw->streamInfo.video.fps = d;
				else
					sw->streamInfo.video.fps = 0;
			}
			else
			{
				sw->streamInfo.type = NAV_STREAMTYPE_UNKNOWN;
				sw->enabled = false;
			}
		}
	}

	sw->sinfoOk = true;
}

#undef NAV_FFCALL
#define NAV_FFCALL(n) self->f->ptr_##n


void GStreamerState::padAdded(GstElement *element, GstPad *pad, GStreamerState *self)
{
	if (self->padProbeCount == 0)
		return;

	size_t si = self->streams.size();
	AppSinkWrapper *sw = new (std::nothrow) AppSinkWrapper(self, si);
	if (sw == nullptr)
		return;

	{
		std::lock_guard lg(self->streamInfoMutex);
		self->streams.push_back(sw);
	}

	UniqueGstElement decodebinUq {
		NAV_FFCALL(gst_element_factory_make)("decodebin", nullptr),
		NAV_FFCALL(gst_object_unref)
	};

	sw->parserPad = pad;
	sw->enabled = false;

	if (!decodebinUq)
		return;

	// Set decodebin HW acceleration
	NAV_FFCALL(g_object_set)(decodebinUq.get(), "force-sw-decoders", (gboolean) self->disableHWAccel, nullptr);
	
	// Setup pad addition callback
	NAV_FFCALL(g_signal_connect_data)(
		decodebinUq.get(),
		"pad-added",
		G_CALLBACK(padAddedSinkWrapper),
		sw,
		nullptr,
		(GConnectFlags) 0
	);
	NAV_FFCALL(g_signal_connect_data)(
		decodebinUq.get(),
		"no-more-pads",
		G_CALLBACK(noMorePadsSinkWrapper),
		sw,
		nullptr,
		(GConnectFlags) 0
	);

	// Add to pipeline
	GstBin *binFromPipeline = G_CAST<GstBin>(self->f, NAV_FFCALL(gst_bin_get_type)(), self->pipeline.get());
	NAV_FFCALL(gst_bin_add)(binFromPipeline, decodebinUq.get());

	// Note: gst_bin_add_many takes the ownership
	sw->decodebin = decodebinUq.release();

	// Link the current stream pad to the decodebin
	UniqueGstObject<GstPad> decodebinSinkPad {
		NAV_FFCALL(gst_element_get_static_pad)(sw->decodebin, "sink"),
		NAV_FFCALL(gst_object_unref)
	};
	if (GST_PAD_LINK_FAILED(NAV_FFCALL(gst_pad_link)(pad, decodebinSinkPad.get())))
	{
		NAV_FFCALL(gst_bin_remove)(binFromPipeline, sw->decodebin);
		sw->decodebin = nullptr;
		self->linkToFakesink(binFromPipeline, pad, *sw);
		return;
	}

	self->padProbeCount++;
	NAV_FFCALL(gst_element_sync_state_with_parent)(sw->decodebin);
}

void GStreamerState::padAddedSinkWrapper(GstElement *element, GstPad *pad, AppSinkWrapper *sw)
{
	GStreamerState *self = sw->self;
	if (self->padProbeCount == 0)
		return;

	UniqueGst<GstCaps> cap {NAV_FFCALL(gst_pad_get_current_caps)(sw->parserPad), NAV_FFCALL(gst_caps_unref)};
	GstStructure *s = NAV_FFCALL(gst_caps_get_structure)(cap.get(), 0);
	const gchar *mediaType = NAV_FFCALL(gst_structure_get_name)(s);

	bool videoStream = memcmp(mediaType, "video/", 6) == 0;
	bool audioStream = memcmp(mediaType, "audio/", 6) == 0;

	UniqueGst<GstCaps> targetCap {nullptr, NAV_FFCALL(gst_caps_unref)};
	nav_streamtype stype = NAV_STREAMTYPE_UNKNOWN;
	// Populate caps
	if (videoStream)
	{
		targetCap.reset(self->newVideoCapsForNAV());
		stype = NAV_STREAMTYPE_VIDEO;
	}
	else if (audioStream)
	{
		targetCap.reset(self->newAudioCapsForNAV());
		stype = NAV_STREAMTYPE_AUDIO;
	}
	else
		return; // unreachable

	UniqueGstElement convertUq {
		NAV_FFCALL(gst_element_factory_make)(
			videoStream
			? "videoconvert"
			: "audioconvert",
			nullptr
		),
		NAV_FFCALL(gst_object_unref)
	};
	UniqueGstElement realsinkUq {
		NAV_FFCALL(gst_element_factory_make)("appsink", nullptr),
		NAV_FFCALL(gst_object_unref)
	};

	if (!(convertUq && realsinkUq))
		return;

	NAV_FFCALL(g_object_set)(realsinkUq.get(),
		"emit-signals", (gboolean) 1,
		"caps", targetCap.get(),
		"max-buffers", 15,
		"sync", (gboolean) 0,
		nullptr
	);

	// Add to pipeline
	GstBin *binFromPipeline = G_CAST<GstBin>(self->f, NAV_FFCALL(gst_bin_get_type)(), self->pipeline.get());
	NAV_FFCALL(gst_bin_add_many)(binFromPipeline,
		convertUq.get(),
		realsinkUq.get(),
		nullptr
	);

	// Note: gst_bin_add_many takes the ownership
	sw->convert = convertUq.release();
	sw->sink = realsinkUq.release();

	// Link the decodebin pad to the audio/videoconvert
	UniqueGstObject<GstPad> convertSinkPad {
		NAV_FFCALL(gst_element_get_static_pad)(sw->convert, "sink"),
		NAV_FFCALL(gst_object_unref)
	};
	if (GST_PAD_LINK_FAILED(NAV_FFCALL(gst_pad_link)(pad, convertSinkPad.get())))
	{
		NAV_FFCALL(gst_bin_remove_many)(binFromPipeline,
			sw->convert,
			sw->sink,
			nullptr
		);
		sw->convert = nullptr;
		sw->sink = nullptr;
		return;
	}

	// Link the audio/videoconvert -> appsink
	if (!NAV_FFCALL(gst_element_link_many)(sw->convert, sw->sink, nullptr))
	{
		NAV_FFCALL(gst_pad_unlink)(pad, convertSinkPad.get());
		NAV_FFCALL(gst_bin_remove_many)(binFromPipeline,
			sw->convert,
			sw->sink,
			nullptr
		);
		sw->convert = nullptr;
		sw->sink = nullptr;
		return;
	}
	
	NAV_FFCALL(gst_element_sync_state_with_parent)(sw->convert);
	NAV_FFCALL(gst_element_sync_state_with_parent)(sw->sink);
	sw->enabled = true;
	sw->streamInfo.type = stype;
}

void GStreamerState::noMorePads(GstElement *element, GStreamerState *self)
{
	self->padProbeCount--;
}

void GStreamerState::noMorePadsSinkWrapper(GstElement *element, AppSinkWrapper *sw)
{
	sw->self->padProbeCount--;
}

void GStreamerState::needData(GstElement *element, guint length, GStreamerState *self)
{
	constexpr size_t DEFAULT_SIZE = 4096; // if length is -1
	size_t toRead = length == (guint)-1 ? DEFAULT_SIZE : length;
	GstFlowReturn ret;

	// TODO: Error checking
	GstMapInfo mapInfo = {};
	GstMemory *memory = NAV_FFCALL(gst_allocator_alloc)(nullptr, (gsize) toRead, nullptr);
	// Read
	size_t readed = 0;
	{
		GstMemoryMapLock mapInfo(self->f, memory, GST_MAP_WRITE, false);
		readed = self->input.readf(mapInfo.data, toRead);
	}

	if (readed < toRead)
	{
		if (readed == 0)
		{
			// EOF
			NAV_FFCALL(gst_memory_unref)(memory);
			NAV_FFCALL(g_signal_emit_by_name)(element, "end-of-stream", &ret);
			return;
		}

		NAV_FFCALL(gst_memory_resize)(memory, 0, (gsize) readed);
	}

	uint64_t newpos = self->input.tellf();

	// Allocate buffer
	GstBuffer *buffer = NAV_FFCALL(gst_buffer_new)();
	NAV_FFCALL(gst_buffer_append_memory)(buffer, memory);
	GST_BUFFER_OFFSET_END(buffer) = newpos;
	GST_BUFFER_OFFSET(buffer) = newpos - (uint64_t) readed;

	NAV_FFCALL(g_signal_emit_by_name)(element, "push-buffer", buffer, &ret);
	NAV_FFCALL(gst_buffer_unref)(buffer);
}

gboolean GStreamerState::seekData(GstElement *element, guint64 pos, GStreamerState *self)
{
	bool r = self->input.seekf((uint64_t) pos);
	return (gboolean) r;
}

#undef NAV_FFCALL
#define NAV_FFCALL(n) self->f->ptr_##n

GStreamerState::AppSinkWrapper::AppSinkWrapper(GStreamerState *state, size_t streamIndex)
: streamInfo()
, streamIndex(streamIndex)
, self(state)
, parserPad(nullptr)
, decodebin(nullptr)
, convert(nullptr)
, sink(nullptr)
, eos(false)
, enabled(false)
, sinfoOk(false)
{
	streamInfo.type = NAV_STREAMTYPE_UNKNOWN;
}

GStreamerState::AppSinkWrapper::~AppSinkWrapper()
{
}

bool GStreamerState::FrameComparator::operator()(const Frame *lhs, const Frame *rhs) const noexcept
{
	return lhs->operator<(*rhs);
}

#undef NAV_FFCALL
#define NAV_FFCALL(n) ptr_##n

GStreamerBackend::GStreamerBackend()
: glib("libglib-2.0.so.0")
, gobject("libgobject-2.0.so.0")
, gstreamer("libgstreamer-1.0.so.0")
, gstvideo("libgstvideo-1.0.so.0")
, version("")
#define _NAV_PROXY_FUNCTION_POINTER(lib, n) , ptr_##n(nullptr)
#include "GStreamerPointers.h"
#undef _NAV_PROXY_FUNCTION_POINTER
{
	if (
#define _NAV_PROXY_FUNCTION_POINTER(lib, n) !lib.get(#n, &ptr_##n) ||
#include "GStreamerPointers.h"
#undef _NAV_PROXY_FUNCTION_POINTER
		!true // needed to fix the preprocessor stuff
	)
		throw std::runtime_error("Cannot load necessary function pointer");
	
	GError *err = nullptr;
	if (!NAV_FFCALL(gst_init_check)(nullptr, nullptr, &err))
	{
		std::string errmsg = err->message;
		NAV_FFCALL(g_error_free)(err);
		throw std::runtime_error(errmsg);
	}

	checkElementFactory("appsrc");
	checkElementFactory("parsebin");
	checkElementFactory("decodebin");
	checkElementFactory("audioconvert");
	checkElementFactory("videoconvert");
	checkElementFactory("appsink");
	checkElementFactory("fakesink");

	version = G_TOSTRCALL(this, NAV_FFCALL(gst_version_string));
}

GStreamerBackend::~GStreamerBackend()
{
	NAV_FFCALL(gst_deinit)();
}

const char *GStreamerBackend::getName() const noexcept
{
	return "gstreamer";
}

nav_backendtype GStreamerBackend::getType() const noexcept
{
	return NAV_BACKENDTYPE_3RD_PARTY;
}

const char *GStreamerBackend::getInfo()
{
	return version.c_str();
}

State *GStreamerBackend::open(nav_input *input, const char *filename, const nav_settings *settings)
{
	return new GStreamerState(this, input, settings);
}

void GStreamerBackend::checkElementFactory(const char *name)
{
	if (GstElementFactory *factory = NAV_FFCALL(gst_element_factory_find)(name))
		NAV_FFCALL(gst_object_unref)(factory);
	else
		throw std::runtime_error("Element factory for " + std::string(name) + " not found.");
}

#undef NAV_FFCALL

Backend *create()
{
	if (checkBackendDisabled("GSTREAMER"))
		return nullptr;

	try
	{
		return new GStreamerBackend();
	}
	catch (const std::exception &e)
	{
		nav::error::set(e);
		return nullptr;
	}
}

}

#endif /* NAV_BACKEND_GSTREAMER */
