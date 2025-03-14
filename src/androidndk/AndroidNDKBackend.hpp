#ifndef _NAV_BACKEND_ANDROIDNDK_HPP_
#define _NAV_BACKEND_ANDROIDNDK_HPP_

#include "NAVConfig.hpp"

#ifdef NAV_BACKEND_ANDROIDNDK

#include "Internal.hpp"
#include "Backend.hpp"

namespace nav::androidndk
{

Backend *create();

}

#endif /* NAV_BACKEND_ANDROIDNDK */

#endif /* _NAV_BACKEND_ANDROIDNDK_HPP_ */