#ifndef _property_whitelist_h_
#define _property_whitelist_h_

#include "property.h"

// By default, we allow "reads" (property_handler, ML hooking) for any property.
//
// Initial support for Digic 6, 7, 8, X cams found some prop handlers triggered
// bad behaviour (crashes, hangs). These are blacklisted for both read and write
// until investigated and fixed.
//
// CONFIG_PROP_REQUEST_CHANGE is #undef for safe porting.
// Writing to props can brick cams if mistakes are made.

// deny reads / do not register property handlers for these
static const uint32_t prop_handler_deny[] =
{
    PROP_ISO,
    PROP_MVR_REC_START,
    PROP_LV_AFFRAME
};

// allow writes / allow prop_request_change() for these:
static const uint32_t prop_write_allow[] =
{
};

#endif
