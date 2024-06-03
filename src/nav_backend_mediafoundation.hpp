#include "nav_config.hpp"

#ifdef NAV_BACKEND_MEDIAFOUNDATION

#include <mfapi.h>
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

#define NAV_PROXY_FUNCTION_POINTER(n) decltype(n) *n
	NAV_PROXY_FUNCTION_POINTER(MFStartup);
	NAV_PROXY_FUNCTION_POINTER(MFShutdown);
#undef NAV_PROXY_FUNCTION_POINTER
};

MediaFoundationBackend *create();

}

#endif /* NAV_BACKEND_MEDIAFOUNDATION */
