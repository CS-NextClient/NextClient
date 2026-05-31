#pragma once
#ifdef HAS_CSSDK_LIB
#include <cssdk/engine/edict.h>
#endif

namespace ncl_entity
{
#ifdef HAS_CSSDK_LIB
    typedef cssdk::Edict edict_t;
#endif
} // namespace ncl_entity
