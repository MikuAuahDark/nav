#ifndef _NAV_BACKEND_H_
#define _NAV_BACKEND_H_

#include "nav_internal.hpp"

#include "nav/nav.h"

namespace nav
{

class Backend
{
public:
	virtual ~Backend();
	virtual State *open(nav_input *input, const char *filename) = 0;
};

}

#endif
