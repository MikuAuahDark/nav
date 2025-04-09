#ifndef _NAV_BACKEND_MEDIAFOUNDATION_INTERNAL_
#define _NAV_BACKEND_MEDIAFOUNDATION_INTERNAL_

#include <string>
#include <type_traits>
#include <vector>

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <d3d11.h>

#include "Internal.hpp"
#include "nav/input.h"
#include "Backend.hpp"
#include "DynLib.hpp"

namespace nav::mediafoundation
{

class MediaFoundationState;
class MediaFoundationBackend;

template<typename T>
class ComPtr
{
	static_assert(std::is_base_of_v<IUnknown, T>, "ComPtr template must derive from IUnknown");
public:
	ComPtr()
	: ptr(nullptr)
	{}

	ComPtr(T *raw, bool retain = true)
	: ptr(raw)
	{
		if (retain && raw != nullptr)
			ptr->AddRef();
	}

	ComPtr(const ComPtr<T> &other)
	{
		ptr = other.ptr;
		if (ptr)
			ptr->AddRef();
	}

	ComPtr(ComPtr<T> &&other)
	: ptr(other.ptr)
	{
		other.ptr = nullptr;
	}

	~ComPtr()
	{
		if (ptr)
			ptr->Release();
	}

	ComPtr &operator=(const ComPtr<T> &other)
	{
		if (other.ptr)
			other.ptr->AddRef();

		if (ptr)
			ptr->Release();

		ptr = other.ptr;
		return *this;
	}

	ComPtr &operator=(ComPtr<T> &&other)
	{
		ptr = other.ptr;
		other.ptr = nullptr;
		return *this;
	}

	T *operator->() const
	{
		return ptr;
	}

	T *get()
	{
		return ptr;
	}

	T **dptr()
	{
		return &ptr;
	}

	T *release(bool callRelease)
	{
		T *p = ptr;
		if (callRelease && ptr)
			ptr->Release();

		ptr = nullptr;
		return p;
	}

	template<typename U>
	HRESULT cast(const GUID &iid, ComPtr<U> &dest) const
	{
		U *a;
		HRESULT hr = ptr->QueryInterface(iid, (void**) &a);
		if (SUCCEEDED(hr))
		{
			dest.release(true);
			dest.ptr = a;
		}

		return hr;
	}

	template<typename U>
	ComPtr<U> dcast(const GUID& iid)
	{
		ComPtr<U> result;
		cast(iid, result);
		return result;
	}

	operator bool()
	{
		return ptr;
	}
private:
	template<typename> friend class ComPtr;
	T *ptr;
};

class NavInputStream: public IStream
{
public:
	NavInputStream(nav_input *input, const char *filename);

	/* IUnknown */
	ULONG STDMETHODCALLTYPE AddRef() override;
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **out) override;
	ULONG STDMETHODCALLTYPE Release() override;

	/* ISequentialStream */
	HRESULT STDMETHODCALLTYPE Read(void *dest, ULONG size, ULONG *readed) override;
	HRESULT STDMETHODCALLTYPE Write(const void *src, ULONG size, ULONG *written) override;

	/* IStream */
	HRESULT STDMETHODCALLTYPE Clone(IStream **out) override;
	HRESULT STDMETHODCALLTYPE Commit(DWORD flags) override;
    HRESULT STDMETHODCALLTYPE CopyTo(IStream *pstm, ULARGE_INTEGER cb, ULARGE_INTEGER *readed, ULARGE_INTEGER *written) override;
    HRESULT STDMETHODCALLTYPE LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) override;
    HRESULT STDMETHODCALLTYPE Revert(void) override;
    HRESULT STDMETHODCALLTYPE Seek(LARGE_INTEGER offset, DWORD origin, ULARGE_INTEGER *newpos) override;
    HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER libNewSize) override;
    HRESULT STDMETHODCALLTYPE Stat(STATSTG *pstatstg, DWORD grfStatFlag) override;
    HRESULT STDMETHODCALLTYPE UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) override;
private:
	std::string filename;
	nav_input *input;
	ULONG refc;
};

struct HWAccelState
{
	ComPtr<ID3D11Device> d3d11dev;
	ComPtr<IMFDXGIDeviceManager> dxgidm;
	UINT resetToken;
	bool active;
};

class MediaFoundationState: public State
{
public:
	MediaFoundationState(
		MediaFoundationBackend *backend,
		nav_input *input,
		ComPtr<NavInputStream> &is,
		ComPtr<IMFByteStream> &mfbs,
		ComPtr<IMFSourceReader> &mfsr,
		HWAccelState *hwaccelState
	);
	~MediaFoundationState() override;
	size_t getStreamCount() noexcept override;
	nav_streaminfo_t *getStreamInfo(size_t index) noexcept override;
	bool isStreamEnabled(size_t index) noexcept override;
	bool setStreamEnabled(size_t index, bool enabled) override;
	double getDuration() noexcept override;
	double getPosition() noexcept override;
	double setPosition(double position) override;
	nav_frame_t *read() override;

private:
	nav_frame_t *decode(ComPtr<IMFSample> &mfSample, size_t streamIndex, LONGLONG timestamp);
	nav_frame_t *decode2D(ComPtr<IMF2DBuffer> &buf2d, size_t streamIndex, LONGLONG timestamp);

	MediaFoundationBackend *backend;

	nav_input *originalInput;
	ComPtr<NavInputStream> inputStream;
	ComPtr<IMFByteStream> mfByteStream;
	ComPtr<IMFSourceReader> mfSourceReader;
	HWAccelState hwaccel;

	std::vector<nav::StreamInfo> streamInfoList;
	UINT64 currentPosition; // in 100-nanosecond
};


class MediaFoundationBackend: public Backend
{
public:
	~MediaFoundationBackend() override;
	const char *getName() const noexcept override;
	nav_backendtype getType() const noexcept override;
	const char *getInfo() override;
	State *open(nav_input *input, const char *filename, const nav_settings *settings) override;

private:
	friend Backend *create();
	MediaFoundationBackend();

	DynLib d3d11, mfplat, mfreadwrite;
	bool callCoUninitialize;

public:
#define _NAV_PROXY_FUNCTION_POINTER(lib, n) decltype(n) *func_##n;
#include "MediaFoundationPointers.h"
#undef _NAV_PROXY_FUNCTION_POINTER
};

}

#endif /* _NAV_BACKEND_MEDIAFOUNDATION_INTERNAL_ */
