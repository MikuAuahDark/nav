#include <algorithm>
#include <cstring>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#endif

#include "Common.hpp"
#include "Error.hpp"

namespace nav
{

FrameVector::FrameVector(nav_streaminfo_t *streaminfo, size_t streamindex, double position, const void *data, size_t size)
: buffer(size)
, data(planeCount(streaminfo->video.format), nullptr)
, planeWidths(planeCount(streaminfo->video.format), 0)
, streaminfo(streaminfo)
, streamindex(streamindex)
, position(position)
{
	if (data)
		std::copy((const uint8_t*) data, ((const uint8_t*) data) + size, buffer.data());
	
	// Partition data, assume no padding
	uint8_t *start = buffer.data();

	if (streaminfo->type == NAV_STREAMTYPE_VIDEO)
	{
		for (size_t i = 0; i < planeCount(streaminfo->video.format); i++)
		{
			this->data[i] = start;
			planeWidths[i] = streaminfo->plane_width(i);
			start += streaminfo->plane_width(i) * streaminfo->plane_height(i);
		}
	}
	else
	{
		planeWidths[0] = buffer.size();
		this->data[0] = start;
	}
}

FrameVector::~FrameVector()
{}

size_t FrameVector::getStreamIndex() const noexcept
{
	return streamindex;
}

nav_streaminfo_t *FrameVector::getStreamInfo() const noexcept
{
	return streaminfo;
}

double FrameVector::tell() const noexcept
{
	return position;
}

const uint8_t *const *FrameVector::acquire(ptrdiff_t **strides, size_t *nplanes)
{
	if (strides == nullptr)
	{
		error::set("strides is null");
		return nullptr;
	}

	if (nplanes)
		*nplanes = planeCount(streaminfo->video.format);

	*strides = planeWidths.data();

	return data.data();
}

void FrameVector::release() noexcept
{
}

uint8_t *FrameVector::pointer() noexcept
{
	return buffer.data();
}

bool getEnvvarBool(const std::string& name)
{
	const char *value = getenv(name.c_str());
	return value != nullptr && (
		strcmp(value, "1") == 0 ||
		strcmp(value, "ON") == 0 ||
		strcmp(value, "on") == 0 ||
		strcmp(value, "On") == 0 ||
		strcmp(value, "YES") == 0 ||
		strcmp(value, "yes") == 0 ||
		strcmp(value, "Yes") == 0
	);
}

std::optional<int> getEnvvarInt(const std::string &name)
{
	const char *value = getenv(name.c_str());
	if (value == nullptr)
		return std::nullopt;
	
	try
	{
		return std::stoi(value);
	}
	catch (const std::exception &e)
	{
		return std::nullopt;
	}
}

bool checkBackendDisabled(const std::string &backendNameUppercase)
{
	return getEnvvarBool("NAV_DISABLE_" + backendNameUppercase);
}

size_t planeCount(nav_pixelformat fmt) noexcept
{
	switch (fmt)
	{
		case NAV_PIXELFORMAT_UNKNOWN:
		default:
			return 0;
		case NAV_PIXELFORMAT_RGB8:
			return 1;
		case NAV_PIXELFORMAT_NV12:
			return 2;
		case NAV_PIXELFORMAT_YUV420:
		case NAV_PIXELFORMAT_YUV444:
			return 3;
	}
}

#ifdef _WIN32
std::wstring fromUTF8(const std::string &str)
{
	int wsize = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str.c_str(), str.length(), nullptr, 0);
	if (wsize == 0)
		throw std::runtime_error("Invalid filename");

	std::vector<wchar_t> wide(wsize + 1, 0);
	int result = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str.c_str(), str.length(), wide.data(), wsize);
	if (!result)
		throw std::runtime_error("Invalid filename");
	
	return std::wstring(wide.data());
}
#endif /* _WIN32 */

}
