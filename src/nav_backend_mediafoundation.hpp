#include "nav_config.hpp"

#ifdef NAV_BACKEND_MEDIAFOUNDATION

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <windows.h>

#include "nav_internal.hpp"
#include "nav_backend.hpp"
#include "nav_dynlib.hpp"

namespace nav::mediafoundation
{

class MediaFoundationBackend: public Backend
{
public:
	~MediaFoundationBackend() override;
	State *open(nav_input *input) override;

private:
	friend MediaFoundationBackend *create();
	MediaFoundationBackend();

	DynLib mfplat, mfreadwrite;

#define _NAV_PROXY_FUNCTION_POINTER(n) decltype(n) *n
	_NAV_PROXY_FUNCTION_POINTER(MFStartup);
	_NAV_PROXY_FUNCTION_POINTER(MFShutdown);
	_NAV_PROXY_FUNCTION_POINTER(MFCreateMFByteStreamOnStream);
	_NAV_PROXY_FUNCTION_POINTER(MFCreateSourceReaderFromByteStream);
#undef _NAV_PROXY_FUNCTION_POINTER
};

MediaFoundationBackend *create();

}

#endif /* NAV_BACKEND_MEDIAFOUNDATION */
