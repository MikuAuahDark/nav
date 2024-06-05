#ifndef _NAV_BACKEND_MEDIAFOUNDATION_INTERNAL_
#define _NAV_BACKEND_MEDIAFOUNDATION_INTERNAL_

#include <string>
#include <vector>

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#include "nav_internal.hpp"
#include "nav/input.h"
#include "nav_backend.hpp"
#include "nav_dynlib.hpp"

namespace nav::mediafoundation
{

class MediaFoundationState;
class MediaFoundationBackend;

template<typename T>
class ComPtr
{
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
		if (ptr)
			ptr->Release();

		ptr = other.ptr;
		if (ptr)
			ptr->AddRef();
	}

	ComPtr(const ComPtr<T> &&other)
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
		ptr = other.ptr
		ptr->AddRef();
		return *this;
	}

	ComPtr &operator=(ComPtr<T> &&other)
	{
		ptr = other.ptr;
		other.ptr = nullptr;
		return *this;
	}

	T *operator->()
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
		if (callRelease)
			ptr->Release();

		ptr = nullptr;
		return p;
	}

	operator bool()
	{
		return ptr;
	}
private:
	T *ptr;
};

class NavInputStream: public IStream
{
public:
	NavInputStream(nav_input *input, const char *filename, void *(__stdcall *CoTaskMemAlloc)(size_t));

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
	void *(__stdcall *CoTaskMemAlloc)(size_t);
	ULONG refc;
};

class MediaFoundationPacket: public Packet
{
public:
	MediaFoundationPacket(MediaFoundationState *state, ComPtr<IMFSample> &mfSample, DWORD streamIndex, LONGLONG timestamp);
	~MediaFoundationPacket() override;
	size_t getStreamIndex() const noexcept override;
	nav_streaminfo_t *getStreamInfo() const noexcept override;
	double tell() const noexcept override;
	nav_frame_t *decode() override;

private:
	LONGLONG timestamp;
	MediaFoundationState *parent;
	nav_streaminfo_t *streamInfo;
	ComPtr<IMFSample> mfSample;
	DWORD streamIndex;
};

class MediaFoundationState: public State
{
public:
	MediaFoundationState(MediaFoundationBackend *backend, nav_input *input, ComPtr<NavInputStream> &is, ComPtr<IMFByteStream> &mfbs, ComPtr<IMFSourceReader> &mfsr);
	~MediaFoundationState() override;
	size_t getStreamCount() noexcept override;
	nav_streaminfo_t *getStreamInfo(size_t index) noexcept override;
	bool isStreamEnabled(size_t index) noexcept override;
	bool setStreamEnabled(size_t index, bool enabled) override;
	double getDuration() noexcept override;
	double getPosition() noexcept override;
	double setPosition(double position) override;
	nav_packet_t *read() override;

private:
	MediaFoundationBackend *backend;

	nav_input *originalInput;
	ComPtr<NavInputStream> inputStream;
	ComPtr<IMFByteStream> mfByteStream;
	ComPtr<IMFSourceReader> mfSourceReader;

	std::vector<nav::StreamInfo> streamInfoList;
	UINT64 currentPosition; // in 100-nanosecond
};


class MediaFoundationBackend: public Backend
{
public:
	~MediaFoundationBackend() override;
	State *open(nav_input *input, const char *filename) override;

private:
	friend Backend *create();
	MediaFoundationBackend::MediaFoundationBackend();

	DynLib ole32, mfplat, mfreadwrite;

public:
#define _NAV_PROXY_FUNCTION_POINTER(n) decltype(n) *n
	_NAV_PROXY_FUNCTION_POINTER(CoInitializeEx);
	_NAV_PROXY_FUNCTION_POINTER(CoUninitialize);
	_NAV_PROXY_FUNCTION_POINTER(CoTaskMemAlloc);
	_NAV_PROXY_FUNCTION_POINTER(MFStartup);
	_NAV_PROXY_FUNCTION_POINTER(MFShutdown);
	_NAV_PROXY_FUNCTION_POINTER(MFCreateMediaType);
	_NAV_PROXY_FUNCTION_POINTER(MFCreateMFByteStreamOnStream);
	_NAV_PROXY_FUNCTION_POINTER(MFCreateSourceReaderFromByteStream);
#undef _NAV_PROXY_FUNCTION_POINTER
};

}

#endif /* _NAV_BACKEND_MEDIAFOUNDATION_INTERNAL_ */
