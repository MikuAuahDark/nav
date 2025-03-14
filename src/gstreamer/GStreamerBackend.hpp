#ifndef _NAV_BACKEND_GSTREAMER_H_
#define _NAV_BACKEND_GSTREAMER_H_

#include "NAVConfig.hpp"

#ifdef NAV_BACKEND_GSTREAMER

#include "Internal.hpp"
#include "Backend.hpp"

namespace nav::gstreamer
{

Backend *create();

}

#endif /* NAV_BACKEND_GSTREAMER */
#endif /* _NAV_BACKEND_GSTREAMER_H_ */
