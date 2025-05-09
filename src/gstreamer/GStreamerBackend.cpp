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


GStreamerState::GStreamerState(GStreamerBackend *backend, nav_input *input)
: f(backend)
, input(*input)
, bus(nullptr, NAV_FFCALL(gst_object_unref))
, pipeline(nullptr, NAV_FFCALL(gst_object_unref))
, source(nullptr)
, decoder(nullptr)
, streams()
, queuedFrames()
, padProbed(false)
, eos(false)
{
	UniqueGstElement sourceTemp {NAV_FFCALL(gst_element_factory_make)("appsrc", nullptr), NAV_FFCALL(gst_object_unref)};
	UniqueGstElement decoderTemp {NAV_FFCALL(gst_element_factory_make)("decodebin", nullptr), NAV_FFCALL(gst_object_unref)};

	pipeline.reset(NAV_FFCALL(gst_pipeline_new)(nullptr));
	if (!pipeline)
		throw std::runtime_error("Unable to create pipeline element.");

	NAV_FFCALL(g_signal_connect_data)(sourceTemp.get(), "need-data", G_CALLBACK(needData), this, nullptr, (GConnectFlags) 0);
	NAV_FFCALL(g_signal_connect_data)(sourceTemp.get(), "seek-data", G_CALLBACK(seekData), this, nullptr, (GConnectFlags) 0);
	NAV_FFCALL(g_object_set)(sourceTemp.get(),
		"emit-signals", 1,
		"size", (gint64) input->sizef(),
		nullptr
	);
	NAV_FFCALL(gst_util_set_object_arg)(G_CAST<GObject>(f, G_TYPE_OBJECT, sourceTemp.get()), "stream-type", "random-access");
	NAV_FFCALL(g_signal_connect_data)(decoderTemp.get(), "pad-added", G_CALLBACK(padAdded), this, nullptr, (GConnectFlags) 0);
	NAV_FFCALL(g_signal_connect_data)(decoderTemp.get(), "no-more-pads", G_CALLBACK(noMorePads), this, nullptr, (GConnectFlags) 0);

	// Add
	NAV_FFCALL(gst_bin_add_many)(G_CAST<GstBin>(f, NAV_FFCALL(gst_bin_get_type)(), pipeline.get()),
		sourceTemp.get(),
		decoderTemp.get(),
		nullptr
	);

	// sourceTemp and decoderTemp is no longer owned.
	source = sourceTemp.release();
	decoder = decoderTemp.release();

	// Just link the appsrc and the decodebin for now.
	if (!NAV_FFCALL(gst_element_link)(source, decoder))
		throw std::runtime_error("Unable to link source and decoder.");

	// Play
	GstStateChangeReturn ret = NAV_FFCALL(gst_element_set_state)(pipeline.get(), GST_STATE_PLAYING);
	if (ret == GST_STATE_CHANGE_FAILURE)
		throw std::runtime_error("Cannot play pipeline");

	bus.reset(NAV_FFCALL(gst_element_get_bus)(pipeline.get()));

	while (!padProbed)
	{
		try
		{
			pollBus();
			if (ret == GST_STATE_CHANGE_ASYNC)
				ret = NAV_FFCALL(gst_element_get_state)(pipeline.get(), nullptr, nullptr, GST_CLOCK_TIME_NONE);
			if (ret == GST_STATE_CHANGE_FAILURE)
				throw std::runtime_error("gst_element_get_state failed");
		}
		catch(const std::exception&)
		{
			NAV_FFCALL(gst_element_set_state)(pipeline.get(), GST_STATE_NULL);
			NAV_FFCALL(gst_element_get_state)(pipeline.get(), nullptr, nullptr, GST_CLOCK_TIME_NONE);
			throw;
		}
	}

	// Populate the stream info data
	for (std::unique_ptr<AppSinkWrapper> &sw: streams)
	{
		if (sw->ok())
		{
			UniqueGstObject<GstPad> pad {NAV_FFCALL(gst_element_get_static_pad)(sw->convert, "src"), NAV_FFCALL(gst_object_unref)};
			UniqueGst<GstCaps> caps {NAV_FFCALL(gst_pad_get_current_caps)(pad.get()), NAV_FFCALL(gst_caps_unref)};
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
		}
	}
}

GStreamerState::~GStreamerState()
{
	NAV_FFCALL(gst_element_set_state)(pipeline.get(), GST_STATE_NULL);
	NAV_FFCALL(gst_element_get_state)(pipeline.get(), nullptr, nullptr, GST_CLOCK_TIME_NONE);
	clearQueuedFrames();
	pollBus(true);

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
		for (std::unique_ptr<AppSinkWrapper> &sw: streams)
		{
			UniqueGst<GstSample> sample {nullptr, NAV_FFCALL(gst_sample_unref)};
			gboolean eos = 0;
			GstSample *sampleRaw = nullptr;
			NAV_FFCALL(g_signal_emit_by_name)(sw->sink, "try-pull-sample", 1000, &sampleRaw); // wait 1us
			sample.reset(sampleRaw);

			if (sw->enabled)
			{
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
	std::unique_ptr<AppSinkWrapper> &sw = streams[streamIndex];

	if (sw->streamInfo.type == NAV_STREAMTYPE_VIDEO)
	{
		UniqueGstObject<GstPad> pad {NAV_FFCALL(gst_element_get_static_pad)(sw->convert, "src"), NAV_FFCALL(gst_object_unref)};
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

#undef NAV_FFCALL
#define NAV_FFCALL(n) self->f->ptr_##n

void GStreamerState::padAdded(GstElement *element, GstPad *pad, GStreamerState *self)
{
	if (self->padProbed)
		return;

	UniqueGst<GstCaps> cap {NAV_FFCALL(gst_pad_get_current_caps)(pad), NAV_FFCALL(gst_caps_unref)};
	GstStructure *s = NAV_FFCALL(gst_caps_get_structure)(cap.get(), 0);
	const gchar *mediaType = NAV_FFCALL(gst_structure_get_name)(s);

	bool videoStream = memcmp(mediaType, "video/", 6) == 0;
	bool audioStream = memcmp(mediaType, "audio/", 6) == 0;

	self->streams.emplace_back(new AppSinkWrapper(self, self->streams.size()));

	if (videoStream || audioStream)
	{
		// FIXME: Create a fakesink for disabled streams such that
		// appsource -> (demux) -> fakesink, if the stream is disabled.
		std::unique_ptr<AppSinkWrapper> &streamWrapper = self->streams.back();
		GstElement *queue = NAV_FFCALL(gst_element_factory_make)("queue", nullptr);
		GstElement *converter = NAV_FFCALL(gst_element_factory_make)(videoStream ? "videoconvert" : "audioconvert", nullptr);
		GstElement *sink = NAV_FFCALL(gst_element_factory_make)("appsink", nullptr);
		GstCaps *targetCap = nullptr;
		
		// Populate caps
		if (videoStream)
			targetCap = self->newVideoCapsForNAV();
		else if (audioStream)
			targetCap = self->newAudioCapsForNAV();

		NAV_FFCALL(g_object_set)(sink,
			"caps", targetCap,
			"max-buffers", 1,
			"sync", (gboolean) 0,
			nullptr
		);
		NAV_FFCALL(gst_caps_unref)(targetCap);

		// Add to pipeline
		GstBin *binFromPipeline = G_CAST<GstBin>(self->f, NAV_FFCALL(gst_bin_get_type)(), self->pipeline.get());
		NAV_FFCALL(gst_bin_add_many)(binFromPipeline, queue, converter, sink, nullptr);

		GstPad *queueSinkPad = NAV_FFCALL(gst_element_get_static_pad)(queue, "sink");
		// Link the pad to the converter and the converter to the app sink
		if (
			GST_PAD_LINK_FAILED(NAV_FFCALL(gst_pad_link)(pad, queueSinkPad)) ||
			!NAV_FFCALL(gst_element_link_many)(queue, converter, sink, nullptr)
		)
		{
			NAV_FFCALL(gst_bin_remove_many)(binFromPipeline, queue, converter, sink, nullptr);
			return;
		}

		streamWrapper->queue = queue;
		streamWrapper->convert = converter;
		streamWrapper->sink = sink;

		NAV_FFCALL(gst_element_set_state)(queue, GST_STATE_PLAYING);
		NAV_FFCALL(gst_element_set_state)(converter, GST_STATE_PLAYING);
		NAV_FFCALL(gst_element_set_state)(sink, GST_STATE_PLAYING);

		// Populate stream info type.
		if (videoStream)
		{
			streamWrapper->streamInfo.type = NAV_STREAMTYPE_VIDEO;
			streamWrapper->enabled = true;
		}
		else if (audioStream)
		{
			streamWrapper->streamInfo.type = NAV_STREAMTYPE_AUDIO;
			streamWrapper->enabled = true;
		}
	}
}

void GStreamerState::noMorePads(GstElement *element, GStreamerState *self)
{
	self->padProbed = true;
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
	return (gboolean) self->input.seekf((uint64_t) pos);
}

#undef NAV_FFCALL
#define NAV_FFCALL(n) sw->self->f->ptr_##n

GStreamerState::AppSinkWrapper::AppSinkWrapper(GStreamerState *state, size_t streamIndex)
: streamInfo()
, streamIndex(streamIndex)
, self(state)
, queue(nullptr)
, convert(nullptr)
, sink(nullptr)
, eos(false)
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

#define ENSURE_HAS_ELEMENT_FACTORY(name) \
	if (GstElementFactory *factory = NAV_FFCALL(gst_element_factory_find)(name)) \
		NAV_FFCALL(gst_object_unref)(factory); \
	else \
		throw std::runtime_error("Element factory for " #name " not found.");

	ENSURE_HAS_ELEMENT_FACTORY("appsrc");
	ENSURE_HAS_ELEMENT_FACTORY("decodebin");
	ENSURE_HAS_ELEMENT_FACTORY("queue");
	ENSURE_HAS_ELEMENT_FACTORY("audioconvert");
	ENSURE_HAS_ELEMENT_FACTORY("videoconvert");
	ENSURE_HAS_ELEMENT_FACTORY("appsink");

#undef ENSURE_HAS_ELEMENT_FACTORY

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

State *GStreamerBackend::open(nav_input *input, const char *filename, [[maybe_unused]] const nav_settings *settings)
{
	return new GStreamerState(this, input);
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
