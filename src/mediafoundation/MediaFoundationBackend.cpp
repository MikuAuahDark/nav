#include "MediaFoundationBackend.hpp"

#ifdef NAV_BACKEND_MEDIAFOUNDATION

#include <iomanip>
#include <map>
#include <memory>
#include <new>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include <initguid.h>
#include <windows.h>
#include <mferror.h>
#include <d3d11_4.h>

#include "Common.hpp"
#include "Error.hpp"
#include "MediaFoundationInternal.hpp"

/* Notes on MF backend:
 * * Audio is always assumed 16-bit PCM.
 * * Video is always assumed YUV420 planar.
 */

namespace
{

// Don't use GUID_NULL, it needs linking to uuid.lib which is no-no.
constexpr GUID NULL_GUID = {0, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0}};
constexpr GUID IMFAttributes_GUID = {0x2cd2d921, 0xc447, 0x44a7, {0xa1, 0x3c, 0x4a, 0xda, 0xbf, 0xc2, 0x47, 0xe3}};
constexpr GUID IMFTransform_GUID = {0xbf94c121, 0x5b05, 0x4e6f, {0x80, 0x00, 0xba, 0x59, 0x89, 0x61, 0x41, 0x4d}};
constexpr GUID IMF2DBuffer_GUID = {0x7DC9D5F9, 0x9ED9, 0x44ec, {0x9B, 0xBF, 0x06, 0x00, 0xBB, 0x58, 0x9F, 0xBB}};
constexpr GUID IMF2DBuffer2_GUID = {0x33ae5ea6, 0x4316, 0x436f, {0x8d, 0xdd, 0xd7, 0x3d, 0x22, 0xf8, 0x29, 0xec}};
constexpr GUID ID3D11Multithread_GUID = {0x9B7E4E00, 0x342C, 0x4106, {0xA1, 0x9F, 0x4F, 0x27, 0x04, 0xF6, 0x89, 0xF0}};

constexpr uint64_t MF_100NS_UNIT = 10000000;

struct GUIDLessThan
{
	// https://stackoverflow.com/a/24114001
	bool operator()(const GUID &guid1, const GUID &guid2) const
	{
		if (guid1.Data1 != guid2.Data1)
			return guid1.Data1 < guid2.Data1;
		if (guid1.Data2 != guid2.Data2)
			return guid1.Data2 < guid2.Data2;
		if (guid1.Data3 != guid2.Data3)
			return guid1.Data3 < guid2.Data3;

		for (int i = 0; i < 8; i++) {
			if (guid1.Data4[i]!=guid2.Data4[i])
				return guid1.Data4[i] < guid2.Data4[i];
		}

		return false;
	}
};

static std::map<GUID, nav_pixelformat, GUIDLessThan> mediaTypeCombination[2] = {
	{
		// Not hardware accelerated
		{MFVideoFormat_IYUV, NAV_PIXELFORMAT_YUV420},
		{MFVideoFormat_I420, NAV_PIXELFORMAT_YUV420},
		{MFVideoFormat_NV12, NAV_PIXELFORMAT_NV12},
		{MFVideoFormat_RGB24, NAV_PIXELFORMAT_RGB8}
	},
	{
		// Hardware accelerated
		{MFVideoFormat_NV12, NAV_PIXELFORMAT_NV12},
		{MFVideoFormat_IYUV, NAV_PIXELFORMAT_YUV420},
		{MFVideoFormat_I420, NAV_PIXELFORMAT_YUV420},
		{MFVideoFormat_RGB24, NAV_PIXELFORMAT_RGB8}
	}
};

static std::map<GUID, bool, GUIDLessThan> audioTypeCombination = {
	{MFAudioFormat_PCM, false},
	{MFAudioFormat_Float, true}
};

inline void unixTimestampToFILETIME(FILETIME &ft, uint64_t t = 0LL)
{
	ULARGE_INTEGER tv;
	tv.QuadPart = (t * 10000000LL) + 116444736000000000LL;
	ft.dwHighDateTime = tv.HighPart;
	ft.dwLowDateTime = tv.LowPart;
}

[[noreturn]] static void runtimeErrorWithHRESULT(const std::string &text, HRESULT hr)
{
	std::stringstream ss;
	ss << text << " (HRESULT 0x";
	ss << std::hex << (uint32_t) hr << ")";
	throw std::runtime_error(ss.str());
}

struct WrappedPropVariant: public PROPVARIANT
{
	WrappedPropVariant(const WrappedPropVariant &other) = delete;
	WrappedPropVariant(WrappedPropVariant &&other) = delete;

	WrappedPropVariant()
	{
		PropVariantInit(this);
	}

	WrappedPropVariant(uint64_t v)
	: WrappedPropVariant()
	{
		this->vt = VT_UI8;
		this->uhVal.QuadPart = v;
	}

	WrappedPropVariant(int64_t v)
	: WrappedPropVariant()
	{
		this->vt = VT_I8;
		this->uhVal.QuadPart = v;
	}

	~WrappedPropVariant()
	{
		PropVariantClear(this);
	}
};

template<typename ...Any>
static GUID getBestFormatGeneric(decltype(MFTEnumEx) *MFTEnumEx, IMFMediaType *mediaType, const std::map<GUID, Any...> &map, bool hwaccel)
{
	using namespace nav::mediafoundation;

	GUID mediaSubtype, majorType;
	if (FAILED(mediaType->GetGUID(MF_MT_SUBTYPE, &mediaSubtype)))
		return NULL_GUID;
	if (FAILED(mediaType->GetGUID(MF_MT_MAJOR_TYPE, &majorType)))
		return NULL_GUID;

	GUID category = NULL_GUID;
	if (majorType == MFMediaType_Audio)
		category = MFT_CATEGORY_AUDIO_DECODER;
	else if (majorType == MFMediaType_Video)
		category = MFT_CATEGORY_VIDEO_DECODER;
	else
		return NULL_GUID;

	MFT_REGISTER_TYPE_INFO input = {majorType, mediaSubtype};
	IMFActivate **activator = nullptr;
	UINT32 numDecoders;

	if (FAILED(MFTEnumEx(
		category,
		MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_ASYNCMFT | (MFT_ENUM_FLAG_HARDWARE * hwaccel),
		&input,
		nullptr,
		&activator,
		&numDecoders
	)))
		return NULL_GUID;

	GUID result = NULL_GUID;
	bool found = false;

	for (UINT32 i = 0; i < numDecoders && found == false; i++)
	{
		ComPtr<IMFActivate> act(activator[i], false);
		// https://www.winehq.org/pipermail/wine-devel/2020-March/162153.html
		ComPtr<IMFTransform> transform;

		if (SUCCEEDED(act->ActivateObject(IMFTransform_GUID, (void**) transform.dptr())))
		{
			if (FAILED(transform->SetInputType(0, mediaType, 0)))
			{
				act->ShutdownObject();
				continue;
			}

			for (UINT32 j = 0;; j++)
			{
				ComPtr<IMFMediaType> outputType;

				if (HRESULT hr = transform->GetOutputAvailableType(0, j, outputType.dptr()); FAILED(hr))
					break;

				GUID outputMediaType;

				if (SUCCEEDED(outputType->GetGUID(MF_MT_SUBTYPE, &outputMediaType)))
				{
					if (map.find(outputMediaType) != map.end())
					{
						result = outputMediaType;
						found = true;
						break;
					}
				}
			}

			act->ShutdownObject();
		}
	}

	CoTaskMemFree(activator);
	return result;
}

static size_t bruteForceExtraHeight(uint32_t width, uint32_t height, size_t contigSize, nav_pixelformat pixelformat)
{
	constexpr size_t MAX_ADD_HEIGHT = 32;
	size_t w = width, h = height;

	switch (pixelformat)
	{
		case NAV_PIXELFORMAT_YUV420:
		case NAV_PIXELFORMAT_NV12:
		{
			// Try with this formula first
			size_t i = contigSize * 2 / (3 * w) - h;
			if ((w * (h + i) + 2 * ((w + 1) / 2) * ((h + i + 1) / 2)) == contigSize)
				return i;

			// Brute-force
			for (size_t i = 0; i <= MAX_ADD_HEIGHT; i++)
			{
				size_t size = w * (h + i) + 2 * ((w + 1) / 2) * ((h + i + 1) / 2);
				if (size == contigSize)
					return i;
			}
			break;
		}
		case NAV_PIXELFORMAT_YUV444:
		{
			for (size_t i = 0; i <= MAX_ADD_HEIGHT; i++)
			{
				size_t size = w * (h + i) * 3;
				if (size == contigSize)
					return i;
			}
			break;
		}
		default:
			break;
	}

	return 0;
}

}

#define NAV_FFCALL(name) backend->func_##name

namespace nav::mediafoundation
{

NavInputStream::NavInputStream(nav_input *input, const char *filename)
: filename(filename ? filename : "")
, input(input)
, refc(1)
{}

ULONG STDMETHODCALLTYPE NavInputStream::AddRef()
{
	return ++refc;
}

HRESULT STDMETHODCALLTYPE NavInputStream::QueryInterface(REFIID riid, void **out)
{
	if (riid == IID_IUnknown || riid == IID_ISequentialStream || riid == IID_IStream)
	{
		*out = this;
		AddRef();
		return S_OK;
	}

	return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE NavInputStream::Release()
{
	ULONG newref = --refc;
	if (refc == 0)
		delete this;
	
	return newref;
}

HRESULT STDMETHODCALLTYPE NavInputStream::Read(void *dest, ULONG size, ULONG *readed)
{
	ULONG r = (ULONG) input->readf(dest, size);
	if (readed)
		*readed = r;
	return r == size ? S_OK : S_FALSE;
}

HRESULT STDMETHODCALLTYPE NavInputStream::Write(const void *src, ULONG size, ULONG *written)
{
	if (written)
		*written = 0;
	return STG_E_INVALIDFUNCTION;
}

HRESULT STDMETHODCALLTYPE NavInputStream::Clone(IStream **out)
{
	/* Is this needed? */
	*out = nullptr;
	return STG_E_INVALIDFUNCTION;
}
HRESULT STDMETHODCALLTYPE NavInputStream::Commit(DWORD flags)
{
	return STG_E_INVALIDFUNCTION;
}

HRESULT STDMETHODCALLTYPE NavInputStream::CopyTo(IStream *pstm, ULARGE_INTEGER cb, ULARGE_INTEGER *readed, ULARGE_INTEGER *written)
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

HRESULT STDMETHODCALLTYPE NavInputStream::LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)
{
	return STG_E_INVALIDFUNCTION;
}
HRESULT STDMETHODCALLTYPE NavInputStream::Revert(void)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE NavInputStream::Seek(LARGE_INTEGER offset, DWORD origin, ULARGE_INTEGER *newpos)
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

HRESULT STDMETHODCALLTYPE NavInputStream::SetSize(ULARGE_INTEGER libNewSize)
{
	return STG_E_INVALIDFUNCTION;
}

HRESULT STDMETHODCALLTYPE NavInputStream::Stat(STATSTG *pstatstg, DWORD grfStatFlag)
{
	if (pstatstg == nullptr)
		return STG_E_INVALIDPOINTER;
	
	if (!(grfStatFlag | STATFLAG_NONAME))
	{
		std::wstring widefilename = nav::fromUTF8(filename);
		wchar_t *mem = (wchar_t*) CoTaskMemAlloc((widefilename.length() + 1) * sizeof(wchar_t));
		if (mem == nullptr)
			return E_OUTOFMEMORY;

		std::copy(widefilename.c_str(), widefilename.c_str() + widefilename.length() + 1, mem);
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
	pstatstg->clsid = NULL_GUID;
	pstatstg->grfStateBits = 0;

	return S_OK;
}

HRESULT STDMETHODCALLTYPE NavInputStream::UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)
{
	return STG_E_INVALIDFUNCTION;
}

MediaFoundationState::MediaFoundationState(
	MediaFoundationBackend *backend,
	nav_input *input,
	ComPtr<NavInputStream> &is,
	ComPtr<IMFByteStream> &mfbs,
	ComPtr<IMFSourceReader> &mfsr,
	HWAccelState *hwaccelState
)
: backend(backend)
, originalInput(input)
, inputStream(is)
, mfByteStream(mfbs)
, mfSourceReader(mfsr)
, currentPosition(0)
, hwaccel()
{
	if (hwaccelState)
		hwaccel = *hwaccelState;

	if (HRESULT hr = mfsr->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, TRUE); FAILED(hr))
		runtimeErrorWithHRESULT("IMFSourceReader::SetStreamSelection failed", hr);
	
	// Find how many streams
	for (DWORD i = 0; i < 0xFFFFFFFCU; i++)
	{
		ComPtr<IMFMediaType> mfMediaType, mfNativeMediaType;
		HRESULT hr = mfsr->GetCurrentMediaType(i, mfMediaType.dptr());

		if (hr == MF_E_INVALIDSTREAMNUMBER)
			break;
		else if (FAILED(hr))
			runtimeErrorWithHRESULT("IMFSourceReader::GetCurrentMediaType failed", hr);

		nav::StreamInfo streamInfo {NAV_STREAMTYPE_UNKNOWN};
		GUID majorType;

		if (SUCCEEDED(mfMediaType->GetMajorType(&majorType)))
		{
			ComPtr<IMFMediaType> partialType, decoderType;
			bool failed = false;

			if (majorType == MFMediaType_Audio)
			{
				// FIXME: Do not assume non-float and signed bps 16.
				failed = FAILED(NAV_FFCALL(MFCreateMediaType)(partialType.dptr()));

				if (!failed)
				{
					GUID bestAudioFormat = getBestFormatGeneric(NAV_FFCALL(MFTEnumEx), mfMediaType.get(), audioTypeCombination, false);
					if (bestAudioFormat == NULL_GUID)
						bestAudioFormat = MFAudioFormat_PCM;

					partialType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
					partialType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
					partialType->SetGUID(MF_MT_SUBTYPE, bestAudioFormat);
					failed = failed || FAILED(mfsr->SetCurrentMediaType(i, nullptr, partialType.get()));
				}

				if (!failed && SUCCEEDED(mfsr->GetCurrentMediaType(i, decoderType.dptr())))
				{
					UINT32 bps;
					GUID rawAudioType;

					hr = decoderType->GetGUID(MF_MT_SUBTYPE, &rawAudioType);
					failed = failed || FAILED(hr);

					// Sample rate
					hr = decoderType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &streamInfo.audio.sample_rate);
					failed = failed || FAILED(hr);

					// Number of channels
					hr = decoderType->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &streamInfo.audio.nchannels);
					failed = failed || FAILED(hr);

					// Bits per sample
					hr = decoderType->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bps);
					failed = failed || FAILED(hr);

					if (!failed)
					{
						bool isFloat = audioTypeCombination[rawAudioType];
						streamInfo.type = NAV_STREAMTYPE_AUDIO;
						streamInfo.audio.format = makeAudioFormat(bps, isFloat, isFloat || bps > 8);
					}
				}
			}
			else if (majorType == MFMediaType_Video)
			{
				failed = FAILED(NAV_FFCALL(MFCreateMediaType)(partialType.dptr()));
				nav_pixelformat pixfmt = NAV_PIXELFORMAT_UNKNOWN;

				if (!failed)
				{
					bool gotCodec = false;
					GUID codec = NULL_GUID;

					mfMediaType->GetGUID(MF_MT_SUBTYPE, &codec);
					mfMediaType->CopyAllItems(partialType.get());
					partialType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
					partialType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);

					// Query best pixel format before brute-forcing.
					GUID bestPixelFormat = getBestFormatGeneric(NAV_FFCALL(MFTEnumEx), mfMediaType.get(), mediaTypeCombination[hwaccel.active], hwaccel.active);
					if (bestPixelFormat != NULL_GUID)
					{
						partialType->SetGUID(MF_MT_SUBTYPE, bestPixelFormat);

						if (SUCCEEDED(mfsr->SetCurrentMediaType(i, nullptr, partialType.get())))
						{
							gotCodec = true;
							pixfmt = mediaTypeCombination[hwaccel.active][bestPixelFormat];
						}
					}

					if (gotCodec == false)
					{
						// Try best known combination
						for (const auto &comb: mediaTypeCombination[hwaccel.active])
						{
							partialType->SetGUID(MF_MT_SUBTYPE, comb.first);

							if (SUCCEEDED(mfsr->SetCurrentMediaType(i, nullptr, partialType.get())))
							{
								gotCodec = true;
								pixfmt = comb.second;
								break;
							}
						}
					}
				}

				failed = failed || pixfmt == NAV_PIXELFORMAT_UNKNOWN;

				if (!failed)
				{
					ULARGE_INTEGER fps, dimensions;

					// Dimensions
					hr = mfMediaType->GetUINT64(MF_MT_FRAME_SIZE, &dimensions.QuadPart);
					failed = failed || FAILED(hr);

					// FPS
					hr = mfMediaType->GetUINT64(MF_MT_FRAME_RATE, &fps.QuadPart);
					failed = failed || FAILED(hr);

					if (!failed)
					{
						streamInfo.type = NAV_STREAMTYPE_VIDEO;
						streamInfo.video.fps = derationalize(fps.HighPart, fps.LowPart);
						streamInfo.video.width = dimensions.HighPart;
						streamInfo.video.height = dimensions.LowPart;
						streamInfo.video.format = pixfmt;
					}
				}
			}
			else
				mfsr->SetStreamSelection(i, FALSE);
		}

		streamInfoList.push_back(streamInfo);
	}
}

MediaFoundationState::~MediaFoundationState()
{}

size_t MediaFoundationState::getStreamCount() noexcept
{
	return streamInfoList.size();
}

nav_streaminfo_t *MediaFoundationState::getStreamInfo(size_t index) noexcept
{
	if (index >= streamInfoList.size())
	{
		nav::error::set("Stream index out of range");
		return nullptr;
	}

	return &streamInfoList[index];
}

bool MediaFoundationState::isStreamEnabled(size_t index) noexcept
{
	BOOL enabled = FALSE;

	if (index >= streamInfoList.size())
	{
		nav::error::set("Index out of range");
		return false;
	}

	if (FAILED(mfSourceReader->GetStreamSelection((DWORD) index, &enabled)))
	{
		nav::error::set("IMFSourceReader::GetStreamSelection failed");
		return false;
	}

	return (bool) enabled;
}

bool MediaFoundationState::setStreamEnabled(size_t index, bool enabled)
{
	if (index >= streamInfoList.size())
	{
		nav::error::set("Index out of range");
		return false;
	}

	if (FAILED(mfSourceReader->SetStreamSelection((DWORD) index, (BOOL) enabled)))
	{
		nav::error::set("IMFSourceReader::SetStreamSelection failed");
		return false;
	}

	return true;
}

double MediaFoundationState::getDuration() noexcept
{
	WrappedPropVariant pvar;

	if (FAILED(mfSourceReader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &pvar)))
		return -1.0;
	
	return derationalize(pvar.uhVal.QuadPart, MF_100NS_UNIT);
}

double MediaFoundationState::getPosition() noexcept
{
	return derationalize(currentPosition, MF_100NS_UNIT);
}

double MediaFoundationState::setPosition(double position)
{
	INT64 newPos = (INT64) (position * MF_100NS_UNIT);
	WrappedPropVariant pvar = newPos;

	if (FAILED(mfSourceReader->SetCurrentPosition(NULL_GUID, pvar)))
	{
		nav::error::set("IMFSourceReader::SetCurrentPosition failed");
		return -1.0;
	}

	currentPosition = (UINT64) newPos;
	return position;
}

nav_frame_t *MediaFoundationState::read()
{
	while (true)
	{
		ComPtr<IMFSample> mfSample = nullptr;
		DWORD streamIndex, streamFlags;
		LONGLONG timestamp;

		if (
			HRESULT hr = mfSourceReader->ReadSample(MF_SOURCE_READER_ANY_STREAM, 0, &streamIndex, &streamFlags, &timestamp, mfSample.dptr());
			FAILED(hr)
		)
			runtimeErrorWithHRESULT("MediaFoundation assertion failed (ReadSample failed)", hr);

		currentPosition = (UINT64) timestamp;

		if (streamFlags & MF_SOURCE_READERF_ENDOFSTREAM)
			return nullptr;
		if (mfSample)
		{
			ComPtr<IMFMediaBuffer> testbuf;
			DWORD bufcount;

			if (HRESULT hr = mfSample->GetBufferCount(&bufcount); FAILED(hr))
				runtimeErrorWithHRESULT("MediaFoundation assertion failed (GetBufferCount failed)", hr);
			if (bufcount > 0)
				return decode(mfSample, streamIndex, timestamp);
		}
	}
}

nav_frame_t *MediaFoundationState::decode(ComPtr<IMFSample> &mfSample, size_t streamIndex, LONGLONG timestamp)
{
	DWORD maxbufsize, bufsize;

	ComPtr<IMFMediaBuffer> mfMediaBuffer;
	if (HRESULT hr = mfSample->GetBufferByIndex(0, mfMediaBuffer.dptr()); FAILED(hr))
		runtimeErrorWithHRESULT("IMFSample::GetBufferByIndex failed", hr);
	
	if (ComPtr<IMF2DBuffer> buf2d = mfMediaBuffer.dcast<IMF2DBuffer>(IMF2DBuffer_GUID))
		return decode2D(buf2d, streamIndex, timestamp);

	BYTE *source;
	if (HRESULT hr = mfMediaBuffer->Lock(&source, &maxbufsize, &bufsize); FAILED(hr))
		runtimeErrorWithHRESULT("IMFMediaBuffer::Lock failed", hr);
	
	if (bufsize == 0)
		bufsize = maxbufsize;

	nav::FrameVector *frame = new (std::nothrow) nav::FrameVector(
		&streamInfoList[streamIndex],
		streamIndex,
		derationalize(timestamp, (int64_t) MF_100NS_UNIT),
		source,
		bufsize
	);
	mfMediaBuffer->Unlock();
	return frame;
}

nav_frame_t *MediaFoundationState::decode2D(ComPtr<IMF2DBuffer> &buf2d, size_t streamIndex, LONGLONG timestamp)
{
	BYTE *source = nullptr;
	nav_streaminfo_t *streamInfo = &streamInfoList[streamIndex];
	LONG stride = 0;
	DWORD contSize = 0;

	if (HRESULT hr = buf2d->GetContiguousLength(&contSize); FAILED(hr))
		runtimeErrorWithHRESULT("IMF2DBuffer::GetContiguousLength failed", hr);

	bool lockWithBuffer1 = true;
	if (ComPtr<IMF2DBuffer2> buf2d2 = buf2d.dcast<IMF2DBuffer2>(IMF2DBuffer2_GUID))
	{
		BYTE *dummy = nullptr;
		DWORD actualContSize = 0;
		if (HRESULT hr = buf2d2->Lock2DSize(MF2DBuffer_LockFlags_Read, &source, &stride, &dummy, &actualContSize); SUCCEEDED(hr))
		{
			contSize = actualContSize;
			lockWithBuffer1 = false;
		}
	}

	if (lockWithBuffer1)
	{
		if (HRESULT hr = buf2d->Lock2D(&source, &stride); FAILED(hr))
			runtimeErrorWithHRESULT("IMF2DBuffer::Lock2D failed", hr);
	}

	nav::FrameVector *frame = nullptr;
	try
	{
		frame = new nav::FrameVector(
			streamInfo,
			streamIndex,
			derationalize(timestamp, (int64_t) MF_100NS_UNIT),
			nullptr,
			streamInfo->video.size()
		);
	}
	catch (...)
	{
		buf2d->Unlock2D();
		throw;
	}

	uint8_t *frameData = (uint8_t*) frame->data();
	
	// HACK: Some pixel format doesn't provide a way to know additional padding at height.
	size_t extraHeight = bruteForceExtraHeight((uint32_t) stride, streamInfo->video.height, contSize, streamInfo->video.format);

	int nplanes = 1;
	size_t planeWidth[3] = {streamInfo->video.width, 0, 0};
	size_t planeHeight[3] = {streamInfo->video.height, 0, 0};
	size_t sourceExtraHeight[3] = {extraHeight, 0, 0};
	ptrdiff_t sourceStrides[3] = {stride, 0, 0};

	switch (streamInfo->video.format)
	{
		case NAV_PIXELFORMAT_RGB8:
			planeWidth[0] *= 3;
			[[fallthrough]];
		case NAV_PIXELFORMAT_UNKNOWN:
		default:
			break;
		case NAV_PIXELFORMAT_NV12:
			planeWidth[1] = ((planeWidth[0] + 1) / 2) * 2;
			planeHeight[1] = (planeHeight[0] + 1) / 2;
			sourceStrides[1] = ((sourceStrides[0] + 1) / 2) * 2;
			sourceExtraHeight[1] = (sourceExtraHeight[0] + 1) / 2;
			nplanes = 2;
			break;
		case NAV_PIXELFORMAT_YUV420:
			planeWidth[1] = planeWidth[2] = (planeWidth[0] + 1) / 2;
			planeHeight[1] = planeHeight[2] = (planeHeight[0] + 1) / 2;
			sourceStrides[1] = sourceStrides[2] = (sourceStrides[0] + 1) / 2;
			sourceExtraHeight[1] = sourceExtraHeight[2] = (sourceExtraHeight[0] + 1) / 2;
			nplanes = 3;
			break;
		case NAV_PIXELFORMAT_YUV444:
			planeWidth[1] = planeWidth[2] = planeWidth[0];
			planeHeight[1] = planeHeight[2] = planeHeight[0];
			sourceStrides[1] = sourceStrides[2] = sourceStrides[0];
			sourceExtraHeight[1] = sourceExtraHeight[2] = sourceExtraHeight[0];
			nplanes = 3;
			break;
	}

	for (int i = 0; i < nplanes; i++)
	{
		for (int y = 0; y < planeHeight[i]; y++)
		{
			std::copy(source, source + planeWidth[i], frameData);
			frameData += planeWidth[i];
			source += sourceStrides[i];
		}

		source += sourceStrides[i] * ((ptrdiff_t) sourceExtraHeight[i]);
	}

	buf2d->Unlock2D();
	return frame;
}

#undef NAV_FFCALL
#define NAV_FFCALL(n) func_##n

MediaFoundationBackend::MediaFoundationBackend()
: d3d11("d3d11.dll")
, mfplat("mfplat.dll")
, mfreadwrite("mfreadwrite.dll")
, callCoUninitialize(true)
#define _NAV_PROXY_FUNCTION_POINTER(lib, n) , func_##n(nullptr)
#include "MediaFoundationPointers.h"
#undef _NAV_PROXY_FUNCTION_POINTER
{
	if (
#define _NAV_PROXY_FUNCTION_POINTER(lib, n) !lib.get(#n, &func_##n) ||
#include "MediaFoundationPointers.h"
#undef _NAV_PROXY_FUNCTION_POINTER
		!true // needed to fix the preprocessor stuff
	)
		throw std::runtime_error("cannot load MediaFoundation function pointer");

	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE);
	if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
		runtimeErrorWithHRESULT("CoInitializeEx failed", hr);

	callCoUninitialize = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;

	if (FAILED(NAV_FFCALL(MFStartup)(MF_VERSION, MFSTARTUP_NOSOCKET)))
	{
		if (callCoUninitialize)
			CoUninitialize();
		runtimeErrorWithHRESULT("MFStartup failed", hr);
	}
}

MediaFoundationBackend::~MediaFoundationBackend()	
{
	NAV_FFCALL(MFShutdown)();
	if (callCoUninitialize)
		CoUninitialize();
}

State *MediaFoundationBackend::open(nav_input *input, const char *filename, const nav_settings *settings)
{
	input->seekf(0);

	ComPtr<NavInputStream> istream(new NavInputStream(input, filename), false);		
	ComPtr<IMFByteStream> byteStream;

	if (HRESULT hr = NAV_FFCALL(MFCreateMFByteStreamOnStream)(istream.get(), byteStream.dptr()); FAILED(hr))
		runtimeErrorWithHRESULT("MFCreateMFByteStreamOnStream failed", hr);

	if (filename)
	{
		// Try set the filename
		if (ComPtr<IMFAttributes> attrs = byteStream.dcast<IMFAttributes>(IMFAttributes_GUID))
		{
			std::wstring wfilename = nav::fromUTF8(filename);
			attrs->SetString(MF_BYTESTREAM_ORIGIN_NAME, wfilename.c_str());
		}
	}
	
	// Setup hardware acceleration
	HWAccelState hwaccelState {};
	hwaccelState.active = false;

	if (!settings->disable_hwaccel)
	{
		if (SUCCEEDED(NAV_FFCALL(D3D11CreateDevice)(
			nullptr,
			D3D_DRIVER_TYPE_HARDWARE,
			nullptr,
			D3D11_CREATE_DEVICE_VIDEO_SUPPORT,
			nullptr,
			0,
			D3D11_SDK_VERSION,
			hwaccelState.d3d11dev.dptr(),
			nullptr,
			nullptr
		)))
		{
			if (SUCCEEDED(NAV_FFCALL(MFCreateDXGIDeviceManager)(&hwaccelState.resetToken, hwaccelState.dxgidm.dptr())))
			{
				hwaccelState.active = SUCCEEDED(hwaccelState.dxgidm->ResetDevice(
					hwaccelState.d3d11dev.get(),
					hwaccelState.resetToken
				));

				// Multithread protected
				if (ComPtr<ID3D11Multithread> d3d11mt = hwaccelState.d3d11dev.dcast<ID3D11Multithread>(ID3D11Multithread_GUID))
					d3d11mt->SetMultithreadProtected(TRUE);
			}
		}
	}

	ComPtr<IMFAttributes> attrs;
	if (HRESULT hr = NAV_FFCALL(MFCreateAttributes)(attrs.dptr(), 2); FAILED(hr))
		runtimeErrorWithHRESULT("MFCreateAttributes failed", hr);

	bool useHWAccel = hwaccelState.active;
	if (useHWAccel)
	{
		attrs->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
		attrs->SetUnknown(MF_SOURCE_READER_D3D_MANAGER, hwaccelState.dxgidm.get());
	}

	ComPtr<IMFSourceReader> sourceReader;
	if (HRESULT hr = NAV_FFCALL(MFCreateSourceReaderFromByteStream)(
		byteStream.get(),
		useHWAccel ? attrs.get() : nullptr,
		sourceReader.dptr()
	); FAILED(hr))
		runtimeErrorWithHRESULT("MFCreateSourceReaderFromByteStream failed", hr);

	// All good
	return new MediaFoundationState(this, input, istream, byteStream, sourceReader, useHWAccel ? &hwaccelState : nullptr);
}

const char *MediaFoundationBackend::getName() const noexcept
{
	return "mediafoundation";
}

nav_backendtype MediaFoundationBackend::getType() const noexcept
{
	return NAV_BACKENDTYPE_OS_API;
}

const char *MediaFoundationBackend::getInfo()
{
	return nullptr;
}

Backend *create()
{
	if (checkBackendDisabled("MEDIAFOUNDATION"))
		return nullptr;

	try
	{
		return new MediaFoundationBackend();
	}
	catch(const std::exception &e)
	{
		nav::error::set(e);
		return nullptr;
	}
}

}

#endif /* NAV_BACKEND_MEDIAFOUNDATION */
