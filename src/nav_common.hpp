#ifndef _NAV_COMMON_H_
#define _NAV_COMMON_H_

#include <string>
#include <vector>
#include <type_traits>

#include "nav_internal.hpp"

namespace nav
{

struct FrameVector: public nav_frame_t
{
	FrameVector(const void *data, size_t size);
	~FrameVector() override;
	size_t size() const noexcept override;
	void *data() noexcept override;

private:
	std::vector<uint8_t> buffer;
};

#ifdef _WIN32
std::wstring fromUTF8(const std::string &str);
#endif /* _WIN32 */

}

#endif /* _NAV_COMMON_H_ */
