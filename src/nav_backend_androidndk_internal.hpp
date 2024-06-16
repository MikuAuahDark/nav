#ifndef _NAV_BACKEND_ANDROIDNDK_INTERNAL_HPP_
#define _NAV_BACKEND_ANDROIDNDK_INTERNAL_HPP_

#include "nav_backend_androidndk.hpp"

#ifdef NAV_BACKEND_ANDROIDNDK

#include <memory>
#include <media/NdkMediaExtractor.h>

#include "nav_backend.hpp"
#include "nav_dynlib.hpp"

namespace nav::androidndk
{

using UniqueMediaExtractor = std::unique_ptr<AMediaExtractor, decltype(&AMediaExtractor_delete)>;

class AndroidNDKBackend;

class MediaSourceWrapper
{
public:
	MediaSourceWrapper(const MediaSourceWrapper &other) = delete;
	MediaSourceWrapper(AndroidNDKBackend *backend, nav_input *input);
	MediaSourceWrapper(MediaSourceWrapper &&other);
	~MediaSourceWrapper();
	AMediaDataSource *get() const noexcept;

private:
	static ssize_t readAt(void *userdata, off64_t offset, void * buffer, size_t size);
	static ssize_t getSize(void *userdata);
	static void close(void *userdata);
	static ssize_t getAvailableSize(void *userdata, off64_t offset);

	AndroidNDKBackend *f;
	AMediaDataSource *datasource;
	nav_input *input;
};

class AndroidNDKState: public State
{
public:
	AndroidNDKState(AndroidNDKBackend *backend, UniqueMediaExtractor &ext, MediaSourceWrapper &ds);
	~AndroidNDKState() override;
	size_t getStreamCount() noexcept override;
	nav_streaminfo_t *getStreamInfo(size_t index) noexcept override;
	bool isStreamEnabled(size_t index) noexcept override;
	bool setStreamEnabled(size_t index, bool enabled) override;
	double getDuration() noexcept override;
	double getPosition() noexcept override;
	double setPosition(double off) override;
	nav_frame_t *read() override;

private:
	MediaSourceWrapper dataSource;
	UniqueMediaExtractor extractor;
};

class AndroidNDKBackend: public Backend
{
public:
	AndroidNDKBackend();
	~AndroidNDKBackend() override;
	State *open(nav_input *input, const char *filename) override;

private:
	friend class MediaSourceWrapper;
	DynLib mediandk;

#define _NAV_PROXY_FUNCTION_POINTER(n) decltype(n) *func_##n;
#include "nav_backend_androidndk_funcptr.h"
#undef _NAV_PROXY_FUNCTION_POINTER
};

}

#endif /* NAV_BACKEND_ANDROIDNDK */

#endif /* _NAV_BACKEND_ANDROIDNDK_INTERNAL_HPP_ */
