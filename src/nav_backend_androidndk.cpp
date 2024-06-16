#include "nav_backend_androidndk.hpp"

#ifdef NAV_BACKEND_ANDROIDNDK

#include <stdexcept>

#include "nav_backend_androidndk_internal.hpp"

#define NAV_FFCALL(n) f->func_##n

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

inline void THROW_IF_ERROR(media_status_t status)
{
	if (status != AMEDIA_OK)
		throw std::runtime_error(getMediaStatusText(status));
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

#undef NAV_FFCALL
#define NAV_FFCALL(n) func_##n

ssize_t MediaSourceWrapper::getAvailableSize(void *userdata, off64_t offset)
{
	nav_input *input = (nav_input*) userdata;
	// TODO: Should we return -1 or input->sizef()?
	return (ssize_t) input->sizef();
}

AndroidNDKBackend::AndroidNDKBackend()
: mediandk("libmediandk.so")
#define _NAV_PROXY_FUNCTION_POINTER(n) func_##n(nullptr)
#include "nav_backend_androidndk_funcptr.h"
#undef _NAV_PROXY_FUNCTION_POINTER
{
	if (
#define _NAV_PROXY_FUNCTION_POINTER(n) !mediandk.get(#n, &func_##n) ||
#include "nav_backend_androidndk_funcptr.h"
#undef _NAV_PROXY_FUNCTION_POINTER
		!true // needed to fix the preprocessor stuff
	)
		throw std::runtime_error("Cannot load libmediandk.so function pointer");
}

AndroidNDKBackend::~AndroidNDKBackend()
{}

State *AndroidNDKBackend::open(nav_input *input, const char *filename)
{
	UniqueMediaExtractor extractor(NAV_FFCALL(AMediaExtractor_new)(), NAV_FFCALL(AMediaExtractor_delete));
	if (!extractor)
		throw std::runtime_error("Cannot allocate AMediaExtractor");

	MediaSourceWrapper mediaSource(this, input);
	THROW_IF_ERROR(NAV_FFCALL(AMediaExtractor_setDataSourceCustom)(extractor.get(), mediaSource.get()));

	return new AndroidNDKState(this, extractor, mediaSource);
}

Backend *create()
{
	try
	{
		return new AndroidNDKBackend();
	}
	catch(const std::exception& e)
	{
		return nullptr;
	}
}

}


#endif /* NAV_BACKEND_ANDROIDNDK */
