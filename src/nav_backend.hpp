#ifndef _NAV_BACKEND_H_
#define _NAV_BACKEND_H_

#include "nav_internal.hpp"

#include "nav.h"

namespace nav
{

class Backend
{
public:
	virtual ~Backend() = 0;
	virtual State *open(nav_input *input) = 0;
};

}

#endif
