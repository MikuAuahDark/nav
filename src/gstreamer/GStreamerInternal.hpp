#ifndef _NAV_BACKEND_GSTREAMER_INTERNAL_H_
#define _NAV_BACKEND_GSTREAMER_INTERNAL_H_

#include "NAVConfig.hpp"

#ifdef NAV_BACKEND_GSTREAMER

#include <gst/gst.h>

#include <atomic>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <queue>

#include "Internal.hpp"
#include "Backend.hpp"
#include "Common.hpp"
#include "DynLib.hpp"

namespace nav::gstreamer
{

template<typename T>
using UniqueGst = std::unique_ptr<T, void(*)(T*)>;
template<typename T>
using UniqueGstObject = std::unique_ptr<T, decltype(&gst_object_unref)>;
using UniqueGstElement = UniqueGstObject<GstElement>;

class GStreamerBackend;

class GStreamerAudioFrame: public Frame
{
public:
	GStreamerAudioFrame(
		GStreamerBackend *backend,
		GstBuffer *buffer,
		nav_streaminfo_t *sinfo,
		size_t si,
		double pts
	);
	~GStreamerAudioFrame() override;
	size_t getStreamIndex() const noexcept override;
	const nav_streaminfo_t *getStreamInfo() const noexcept override;
	double tell() const noexcept override;
	const uint8_t *const *acquire(ptrdiff_t **strides, size_t *nplanes) override;
	void release() noexcept override;
	nav_hwacceltype getHWAccelType() const noexcept override;
	void *getHWAccelHandle() override;

private:
	AcquireData acquireData;
	GstMapInfo mapInfo;
	GstMemory *memory;
	double pts;
	GStreamerBackend *f;
	GstBuffer *buffer;
	nav_streaminfo_t *streamInfo;
	size_t streamIndex;
};

class GStreamerVideoFrame: public Frame
{
public:
	GStreamerVideoFrame(
		GStreamerBackend *backend,
		UniqueGst<GstVideoInfo> &&videoInfo,
		GstBuffer *buffer,
		nav_streaminfo_t *sinfo,
		size_t si,
		double pts
	);
	~GStreamerVideoFrame() override;
	size_t getStreamIndex() const noexcept override;
	const nav_streaminfo_t *getStreamInfo() const noexcept override;
	double tell() const noexcept override;
	const uint8_t *const *acquire(ptrdiff_t **strides, size_t *nplanes) override;
	void release() noexcept override;
	nav_hwacceltype getHWAccelType() const noexcept override;
	void *getHWAccelHandle() override;

private:
	AcquireData acquireData;
	GstVideoFrame videoFrame;
	UniqueGst<GstVideoInfo> videoInfo;
	double pts;
	GStreamerBackend *f;
	GstBuffer *buffer;
	nav_streaminfo_t *streamInfo;
	size_t streamIndex;
};

class GStreamerState: public State
{
public:
	GStreamerState(GStreamerBackend *backend, nav_input *input, const nav_settings *settings);
	~GStreamerState() override;
	Backend *getBackend() const noexcept override;
	size_t getStreamCount() const noexcept override;
	const nav_streaminfo_t *getStreamInfo(size_t index) const noexcept override;
	bool isStreamEnabled(size_t index) const noexcept override;
	bool setStreamEnabled(size_t index, bool enabled) override;
	double getDuration() noexcept override;
	double getPosition() noexcept override;
	double setPosition(double off) override;
	bool prepare() override;
	bool isPrepared() const noexcept override;
	nav_frame_t *read() override;

private:
	struct AppSinkWrapper
	{
		AppSinkWrapper() = delete;
		AppSinkWrapper(const AppSinkWrapper &other) = delete;
		AppSinkWrapper(AppSinkWrapper &&other) = default;
		AppSinkWrapper(GStreamerState *state, size_t streamIndex);
		~AppSinkWrapper();

		inline bool ok()
		{
			return decodebin && convert && sink && streamInfo.type != NAV_STREAMTYPE_UNKNOWN;
		}

		nav_streaminfo_t streamInfo;
		size_t streamIndex;
		GStreamerState *self;
		GstPad *parserPad; // Note: This is owned by GStreamerState parsebin.
		GstElement *decodebin, *convert, *sink; // Owned by pipeline. Can be either appsink or fakesink
		bool eos, enabled, sinfoOk;
	};

	struct FrameComparator
	{
		bool operator()(const Frame *lhs, const Frame *rhs) const noexcept;
	};

	GStreamerBackend *f;
	nav_input input;
	UniqueGstObject<GstBus> bus;
	UniqueGstElement pipeline;
	GstElement *source, *parsebin;
	std::vector<AppSinkWrapper*> streams;
	std::mutex streamInfoMutex;
	std::priority_queue<Frame*, std::deque<Frame*>, FrameComparator> queuedFrames;
	std::atomic<int> padProbeCount;
	bool eos, prepared, disableHWAccel;

	GstCaps *newVideoCapsForNAV();
	GstCaps *newAudioCapsForNAV();
	void clearQueuedFrames();
	void pollBus(bool noexception = false);
	Frame *dispatchDecode(GstBuffer *buffer, size_t streamIndex);
	void linkToFakesink(GstBin *bin, GstPad *pad, AppSinkWrapper &sw);
	void setupStreamInfo(AppSinkWrapper *sw, bool tryAgainLater);
	static void padAdded(GstElement *element, GstPad *newPad, GStreamerState *self);
	static void padAddedSinkWrapper(GstElement *element, GstPad *pad, AppSinkWrapper *sw);
	static void noMorePads(GstElement *element, GStreamerState *self);
	static void noMorePadsSinkWrapper(GstElement *element, AppSinkWrapper *sw);
	static void needData(GstElement *element, guint length, GStreamerState *self);
	static gboolean seekData(GstElement *element, guint64 pos, GStreamerState *self);
};

class GStreamerBackend: public Backend
{
public:
	GStreamerBackend();
	~GStreamerBackend() override;
	const char *getName() const noexcept override;
	nav_backendtype getType() const noexcept override;
	const char *getInfo() override;
	State *open(nav_input *input, const char *filename, const nav_settings *settings) override;

private:
	DynLib glib, gobject, gstreamer, gstvideo;
	std::string version;

	void checkElementFactory(const char *name);

public:
#define _NAV_PROXY_FUNCTION_POINTER(lib, n) decltype(n) *ptr_##n;
#include "GStreamerPointers.h"
#undef _NAV_PROXY_FUNCTION_POINTER
};

}

#endif /* NAV_BACKEND_GSTREAMER */
#endif /* _NAV_BACKEND_GSTREAMER_INTERNAL_H_ */
