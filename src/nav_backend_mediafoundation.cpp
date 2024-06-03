#include "nav_backend_mediafoundation.hpp"

#ifdef NAV_BACKEND_MEDIAFOUNDATION

#include <new>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include <mfidl.h>
#include <mfreadwrite.h>

namespace MFID
{

// 73647561-0000-0010-8000-00aa00389b71
const GUID MediaType_Audio = {0x73647561, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71}};
// 00000001-0000-0010-8000-00aa00389b71
const GUID AudioFormat_PCM = {0x00000001, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
// 48eba18e-f8c9-4687-bf11-0a74c9f96a8f
const GUID MT_MAJOR_TYPE = {0x48eba18e, 0xf8c9, 0x4687, {0xbf, 0x11, 0x0a, 0x74, 0xc9, 0xf9, 0x6a, 0x8f}};
// f7e34c9a-42e8-4714-b74b-cb29d72c35e5
const GUID MT_SUBTYPE = {0xf7e34c9a, 0x42e8, 0x4714, {0xb7, 0x4b, 0xcb, 0x29, 0xd7, 0x2c, 0x35, 0xe5}};
// 37e48bf5-645e-4c5b-89de-ada9e29b696a
const GUID MT_AUDIO_NUM_CHANNELS = {0x37e48bf5, 0x645e, 0x4c5b, {0x89, 0xde, 0xad, 0xa9, 0xe2, 0x9b, 0x69, 0x6a}};
// 5faeeae7-0290-4c31-9e8a-c534f68d9dba
const GUID MT_AUDIO_SAMPLES_PER_SECOND = {0x5faeeae7, 0x0290, 0x4c31, {0x9e, 0x8a, 0xc5, 0x34, 0xf6, 0x8d, 0x9d, 0xba}};
// f2deb57f-40fa-4764-aa33-ed4f2d1ff669
const GUID MT_AUDIO_BITS_PER_SAMPLE = {0xf2deb57f, 0x40fa, 0x4764, {0xaa, 0x33, 0xed, 0x4f, 0x2d, 0x1f, 0xf6, 0x69}};
// fc358289-3cb6-460c-a424-b6681260375a
const GUID BYTESTREAM_CONTENT_TYPE = {0xfc358289, 0x3cb6, 0x460c, {0xa4, 0x24, 0xb6, 0x68, 0x12, 0x60, 0x37, 0x5a}};
// 2cd2d921-c447-44a7-a13c-4adabfc247e3
const IID MFAttributes = {0x2cd2d921, 0xc447, 0x44a7, {0xa1, 0x3c, 0x4a, 0xda, 0xbf, 0xc2, 0x47, 0xe3}};

}

namespace
{

inline void unixTimestampToFILETIME(FILETIME &ft, uint64_t t = 0LL)
{
	ULARGE_INTEGER tv;
	tv.QuadPart = (t * 10000000LL) + 116444736000000000LL;
	ft.dwHighDateTime = tv.HighPart;
	ft.dwLowDateTime = tv.LowPart;
}

class NavInputStream: public IStream
{
public:
	NavInputStream(nav_input *input)
	: input(input)
	, refc(1)
	{}

	/* IUnknown */
	ULONG STDMETHODCALLTYPE AddRef() override
	{
		return ++refc;
	}

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **out) override
	{
		if (riid == IID_IUnknown || riid == IID_ISequentialStream || riid == IID_IStream)
		{
			*out = this;
			return S_OK;
		}

		return E_NOINTERFACE;
	}

	ULONG STDMETHODCALLTYPE Release() override
	{
		ULONG newref = --refc;
		if (refc == 0)
			delete this;
		
		return newref;
	}

	/* ISequentialStream */
	HRESULT STDMETHODCALLTYPE Read(void *dest, ULONG size, ULONG *readed) override
	{
		ULONG r = (ULONG) input->readf(dest, size);
		if (readed)
			*readed = r;
		return r == size ? S_OK : S_FALSE;
	}

	HRESULT STDMETHODCALLTYPE Write(const void *src, ULONG size, ULONG *written)
	{
		if (written)
			*written = 0;
		return STG_E_INVALIDFUNCTION;
	}

	/* IStream */
	HRESULT STDMETHODCALLTYPE Clone(IStream **out) override
	{
		/* Is this needed? */
		*out = nullptr;
		return STG_E_INVALIDFUNCTION;
	}

	HRESULT STDMETHODCALLTYPE Commit(DWORD flags) override
	{
		return STG_E_INVALIDFUNCTION;
	}
    
    HRESULT STDMETHODCALLTYPE CopyTo(IStream *pstm, ULARGE_INTEGER cb, ULARGE_INTEGER *readed, ULARGE_INTEGER *written) override
	{
		uint8_t *buf = new (std::nothrow) uint8_t[(size_t) cb.QuadPart];
		if (buf == nullptr)
			return E_OUTOFMEMORY;

		ULONG wr = 0;
		size_t rd = input->readf(buf, cb.QuadPart);
		HRESULT res = pstm->Write(buf, (ULONG) rd, &wr);
		delete[] buf;

		if (FAILED(res))
			return res;
		
		if (readed)
			readed->QuadPart = rd;
		if (written)
			written->QuadPart = wr;
		return ((ULONG) rd) == wr ? S_OK : S_FALSE;
	}
    
    HRESULT STDMETHODCALLTYPE LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) override
	{
		return STG_E_INVALIDFUNCTION;
	}

    HRESULT STDMETHODCALLTYPE Revert(void) override
	{
		return S_OK;
	}
	
    HRESULT STDMETHODCALLTYPE Seek(LARGE_INTEGER offset, DWORD origin, ULARGE_INTEGER *newpos) override
	{
		uint64_t realoff = 0;

		switch (origin)
		{
			case STREAM_SEEK_SET:
			{
				if(offset.QuadPart < 0)
					return STG_E_INVALIDFUNCTION;
				realoff = (uint64_t) offset.QuadPart;
				break;
			}
			case STREAM_SEEK_CUR:
			{
				int64_t curpos = (int64_t) input->tellf();
				if (offset.QuadPart < 0 && (-offset.QuadPart) > curpos)
					return STG_E_INVALIDFUNCTION;
				realoff = uint64_t(curpos + offset.QuadPart);
				break;
			}
			case STREAM_SEEK_END:
			{
				if (offset.QuadPart > 0)
					return STG_E_INVALIDFUNCTION;

				int64_t curpos = (int64_t) input->sizef();
				realoff = uint64_t(curpos + offset.QuadPart);
				break;
			}
			default:
				return STG_E_INVALIDPARAMETER;
		}

		if (!input->seekf(realoff))
			return E_FAIL;
		
		if (newpos)
			newpos->QuadPart = input->tellf();

		return S_OK;
	}
    
    HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER libNewSize) override
	{
		return STG_E_INVALIDFUNCTION;
	}
    
    HRESULT STDMETHODCALLTYPE Stat(STATSTG *pstatstg, DWORD grfStatFlag) override
	{
		if (pstatstg == nullptr)
			return STG_E_INVALIDPOINTER;
		
		if (!(grfStatFlag | STATFLAG_NONAME))
		{
			wchar_t *mem = (wchar_t*) CoTaskMemAlloc(1);
			if (mem == nullptr)
				return E_OUTOFMEMORY;

			mem[0] = 0;
			pstatstg->pwcsName = mem;
		}
		else
			pstatstg->pwcsName = nullptr;

		pstatstg->type = STGTY_STREAM;
		pstatstg->cbSize.QuadPart = input->sizef();
		unixTimestampToFILETIME(pstatstg->mtime);
		unixTimestampToFILETIME(pstatstg->ctime);
		unixTimestampToFILETIME(pstatstg->atime);
		pstatstg->grfMode = STGM_READ;
		pstatstg->grfLocksSupported = 0;
		pstatstg->clsid = CLSID_NULL;
		pstatstg->grfStateBits = 0;
		return S_OK;
	}
    
    HRESULT STDMETHODCALLTYPE UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) override
	{
		return STG_E_INVALIDFUNCTION;
	}
private:
	nav_input *input;
	ULONG refc;	
};

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
		if (retain)
			ptr->AddRef();
	}

	ComPtr(const ComPtr<T> &other)
	: ptr(other.ptr)
	{
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
private:
	T *ptr;
};

}

namespace nav::mediafoundation
{

class MediaFoundationState: public State
{
public:
	MediaFoundationState(MediaFoundationBackend *backend, nav_input *input, ComPtr<NavInputStream> &is, ComPtr<IMFByteStream> &mfbs, ComPtr<IMFSourceReader> &mfsr)
	: backend(backend)
	, originalInput(input)
	, inputStream(is)
	, mfByteStream(mfbs)
	, mfSourceReader(mfsr)
	{
		if (FAILED(mfsr->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, TRUE)))
			throw std::runtime_error("INFSourceReader::SetStreamSelection failed");
	}

	~MediaFoundationState() override
	{}
	
	size_t getAudioStreamCount() noexcept override
	{
		return 0;
	}
	size_t getVideoStreamCount() noexcept override
	{
		return 0;
	}

	nav::AudioState *openAudioStream(size_t index) override;
	nav::VideoState *openVideoStream(size_t index) override;

private:
	MediaFoundationBackend *backend;

	nav_input *originalInput;
	ComPtr<NavInputStream> inputStream;
	ComPtr<IMFByteStream> mfByteStream;
	ComPtr<IMFSourceReader> mfSourceReader;

	std::vector<DWORD> videoStreamMap, audioStreamMap;
};

MediaFoundationBackend::MediaFoundationBackend()
: mfplat("mfplat.dll")
, mfreadwrite("mfreadwrite.dll")
, MFStartup(nullptr)
, MFShutdown(nullptr)
, MFCreateMFByteStreamOnStream(nullptr)
, MFCreateSourceReaderFromByteStream(nullptr)
{
	if (
		!mfplat.get("MFStartup", &MFStartup) ||
		!mfplat.get("MFShutdown", &MFShutdown) ||
		!mfplat.get("MFCreateMFByteStreamOnStream", &MFCreateMFByteStreamOnStream) ||
		!mfreadwrite.get("MFCreateSourceReaderFromByteStream", &MFCreateSourceReaderFromByteStream)
	)
		throw std::runtime_error("cannot load MediaFoundation function pointer");

	if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)))
		throw std::runtime_error("CoInitializeEx failed");

	if (FAILED(MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET)))
	{
		CoUninitialize();
		throw std::runtime_error("MFStartup failed");
	}
}

MediaFoundationBackend::~MediaFoundationBackend()
{
	MFShutdown();
	CoUninitialize();
}

State *MediaFoundationBackend::open(nav_input *input)
{
	input->seekf(0);

	ComPtr<NavInputStream> istream(new NavInputStream(input), false);
	
	IMFByteStream *byteStream_noguard;
	if (FAILED(MFCreateMFByteStreamOnStream(istream.get(), &byteStream_noguard)))
		throw std::runtime_error("MFCreateMFByteStreamOnStream failed");
	ComPtr<IMFByteStream> byteStream(byteStream_noguard, false);

	IMFSourceReader *sourceReader_noguard;
	if (FAILED(MFCreateSourceReaderFromByteStream(byteStream.get(), nullptr, &sourceReader_noguard)))
		throw std::runtime_error("MFCreateSourceReaderFromByteStream failed");
	ComPtr<IMFSourceReader> sourceReader(sourceReader_noguard, false);

	/* All good */
	return new MediaFoundationState(this, input, istream, byteStream, sourceReader);
}

MediaFoundationBackend *create()
{
	try
	{
		return new MediaFoundationBackend();
	}
	catch(const std::exception& e)
	{
		return nullptr;
	}
}

}

#endif /* NAV_BACKEND_MEDIAFOUNDATION */
