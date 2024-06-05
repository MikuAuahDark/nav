#include <algorithm>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#endif

#include "nav_common.hpp"

namespace nav
{

FrameVector::FrameVector(const void *data, size_t size)
: buffer()
{
	buffer.resize(size);

	if (data)
		std::copy((const uint8_t*) data, ((const uint8_t*) data) + size, buffer.data());
}

FrameVector::~FrameVector()
{}


size_t FrameVector::size() const noexcept
{
	return buffer.size();
}

void *FrameVector::data() noexcept
{
	return buffer.data();
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
