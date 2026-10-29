#include <cstddef>

#include <goldsrc/efx_api.h>
#include <goldsrc/entity.h>
#include <goldsrc/format/studio.h>

static_assert(sizeof(void*) == 4);
static_assert(goldsrc::ET_NORMAL == 0);
static_assert(goldsrc::MAXSTUDIOBONES == 128);

static_assert(sizeof(goldsrc::mstudiobonecontroller_t) == 24);
static_assert(alignof(goldsrc::mstudiobonecontroller_t) == 4);
static_assert(offsetof(goldsrc::mstudiobonecontroller_t, bone) == 0);
static_assert(offsetof(goldsrc::mstudiobonecontroller_t, type) == 4);
static_assert(offsetof(goldsrc::mstudiobonecontroller_t, start) == 8);
static_assert(offsetof(goldsrc::mstudiobonecontroller_t, end) == 12);
static_assert(offsetof(goldsrc::mstudiobonecontroller_t, rest) == 16);
static_assert(offsetof(goldsrc::mstudiobonecontroller_t, index) == 20);

static_assert(sizeof(goldsrc::mstudiomesh_t) == 20);
static_assert(alignof(goldsrc::mstudiomesh_t) == 4);
static_assert(offsetof(goldsrc::mstudiomesh_t, numtris) == 0);
static_assert(offsetof(goldsrc::mstudiomesh_t, triindex) == 4);
static_assert(offsetof(goldsrc::mstudiomesh_t, skinref) == 8);
static_assert(offsetof(goldsrc::mstudiomesh_t, numnorms) == 12);
static_assert(offsetof(goldsrc::mstudiomesh_t, normindex) == 16);

static_assert(sizeof(goldsrc::mstudiomodel_t) == 112);
static_assert(alignof(goldsrc::mstudiomodel_t) == 4);
static_assert(offsetof(goldsrc::mstudiomodel_t, name) == 0);
static_assert(offsetof(goldsrc::mstudiomodel_t, type) == 64);
static_assert(offsetof(goldsrc::mstudiomodel_t, boundingradius) == 68);
static_assert(offsetof(goldsrc::mstudiomodel_t, nummesh) == 72);
static_assert(offsetof(goldsrc::mstudiomodel_t, meshindex) == 76);
static_assert(offsetof(goldsrc::mstudiomodel_t, numverts) == 80);
static_assert(offsetof(goldsrc::mstudiomodel_t, vertinfoindex) == 84);
static_assert(offsetof(goldsrc::mstudiomodel_t, vertindex) == 88);
static_assert(offsetof(goldsrc::mstudiomodel_t, numnorms) == 92);
static_assert(offsetof(goldsrc::mstudiomodel_t, norminfoindex) == 96);
static_assert(offsetof(goldsrc::mstudiomodel_t, normindex) == 100);
static_assert(offsetof(goldsrc::mstudiomodel_t, numgroups) == 104);
static_assert(offsetof(goldsrc::mstudiomodel_t, groupindex) == 108);

static_assert(sizeof(goldsrc::dlight_t) == 40);
static_assert(alignof(goldsrc::dlight_t) == 4);
static_assert(offsetof(goldsrc::dlight_t, origin) == 0);
static_assert(offsetof(goldsrc::dlight_t, radius) == 12);
static_assert(offsetof(goldsrc::dlight_t, color) == 16);
static_assert(offsetof(goldsrc::dlight_t, die) == 20);
static_assert(offsetof(goldsrc::dlight_t, decay) == 24);
static_assert(offsetof(goldsrc::dlight_t, minlight) == 28);
static_assert(offsetof(goldsrc::dlight_t, key) == 32);
static_assert(offsetof(goldsrc::dlight_t, dark) == 36);
