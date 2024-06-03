#include "nav_backend_mediafoundation.hpp"

#ifdef NAV_BACKEND_MEDIAFOUNDATION

#include <stdexcept>

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

namespace nav::mediafoundation
{

MediaFoundationBackend::MediaFoundationBackend()
: mfplat()
, mfreadwrite()
, MFStartup(nullptr)
, MFShutdown(nullptr)
{
	mfplat = std::move(DynLib("mfplat.dll"));
	mfreadwrite = std::move(DynLib("mfplat.dll"));

	if (
		!mfplat.get("MFStartup", &MFStartup) ||
		!mfplat.get("MFShutdown", &MFShutdown)
	)
		throw std::runtime_error("cannot load MediaFoundation function pointer");
	
	if (FAILED(MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET)))
		throw std::runtime_error("MFStartup failed");
}

MediaFoundationBackend::~MediaFoundationBackend()
{
	if (MFShutdown)
		MFShutdown();
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
