#include "analytics.h"
#include "console/console.h"

void AN_AddBreadcrumb(const char* description)
{
    if (g_Analytics)
        g_Analytics->AddBreadcrumb(BREADCRUMBS_TAG, description);

    Con_DPrintf(ConLogType::Info, "%s\n", description);
}
