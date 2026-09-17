#include "camera.h"

#include "main.h"

#include <ncl_math/vec3.h>
#include <cstdint>

#include <goldsrc/client_api.h>
#include <goldsrc/cvar.h>
#include <goldsrc/engine/model.h>
#include <goldsrc/entity.h>
#include <goldsrc/format/studio.h>
#include <goldsrc/types.h>
#include <goldsrc/view.h>

/* animate camera using a viewmodel attachment */

static goldsrc::cvar_t* camera_movement_scale;
static goldsrc::cvar_t* camera_movement_interp;

void CameraInit()
{
	camera_movement_scale = gEngfuncs.pfnRegisterVariable("camera_movement_scale", "1", goldsrc::FCVAR_ARCHIVE);
	camera_movement_interp = gEngfuncs.pfnRegisterVariable("camera_movement_interp", "0", goldsrc::FCVAR_ARCHIVE);
}

static goldsrc::mstudioanim_t* GetAnim(goldsrc::studiohdr_t* hdr, goldsrc::model_t* model, goldsrc::mstudioseqdesc_t* seqdesc)
{
	uint8_t* data;
	goldsrc::cache_user_t* seqs;

	if (!seqdesc->seqgroup)
		return (goldsrc::mstudioanim_t*)((uint8_t*)hdr + seqdesc->animindex);

	seqs = (goldsrc::cache_user_t*)model->submodels;
	data = (uint8_t*)seqs[seqdesc->seqgroup].data;

	return (goldsrc::mstudioanim_t*)&data[seqdesc->animindex];
}

static float UnpackScale(int frame, goldsrc::mstudioanim_t* anim, int offset)
{
	int index;
	goldsrc::mstudioanimvalue_t* bone_frame;

	if (!offset)
		return 0;

	bone_frame = (goldsrc::mstudioanimvalue_t*)((uint8_t*)anim + offset);
	index = frame;

	while (1)
	{
		if (index < bone_frame->num.total)
			break;
		index -= bone_frame->num.total;
		bone_frame += bone_frame->num.valid;
		bone_frame++;
	}

	if (index >= bone_frame->num.valid)
		index = bone_frame->num.valid;
	else
		index += 1;

	return (float)bone_frame[index].value;
}

static void UnpackRotation(vec3_t angles, int frame, goldsrc::mstudiobone_t* bone, goldsrc::mstudioanim_t* anim)
{
	/* unpack the bone rotation */
	angles[0] = (bone->value[3] + bone->scale[3] *
		UnpackScale(frame, anim, anim->offset[3])) * ncl_math::kRad2Deg;

	angles[1] = (bone->value[4] + bone->scale[4] *
		UnpackScale(frame, anim, anim->offset[4])) * ncl_math::kRad2Deg;

	angles[2] = (bone->value[5] + bone->scale[5] *
		UnpackScale(frame, anim, anim->offset[5])) * ncl_math::kRad2Deg;
}

static int GetCameraBone(goldsrc::studiohdr_t* hdr)
{
	int i;
	goldsrc::mstudioattachment_t* a;
	goldsrc::mstudiobone_t* b;

	a = (goldsrc::mstudioattachment_t*)((uint8_t*)hdr + hdr->attachmentindex);

	for (i = 0; i < hdr->numattachments; i++, a++)
	{
		b = (goldsrc::mstudiobone_t*)((uint8_t*)hdr + hdr->boneindex) + a->bone;
		if (!strcmp(b->name, "camera"))
			return a->bone;
	}

	return -1;
}

static void CameraCalcMovement(goldsrc::cl_entity_t* vm, vec3_t angles)
{
	float clientTime = gEngfuncs.GetClientTime();

	goldsrc::model_t* model;
	goldsrc::studiohdr_t* hdr;
	int bone_index;
	int sequence;
	goldsrc::mstudioseqdesc_t* seqdesc;
	goldsrc::mstudioanim_t* anim;
	goldsrc::mstudiobone_t* bone;
	float anim_time, t;
	int frame, next_frame;
	vec3_t angles1, angles2;

	ncl_math::VectorClear(angles);

	model = vm->model;
	if (!model)
		return;

	hdr = (goldsrc::studiohdr_t*)model->cache.data;
	if (!hdr)
		return;

	bone_index = GetCameraBone(hdr);
	if (bone_index == -1)
		return;

	sequence = vm->curstate.sequence;
	if (sequence >= hdr->numseq)
		sequence = 0;

	seqdesc = (goldsrc::mstudioseqdesc_t*)((uint8_t*)hdr + hdr->seqindex) + sequence;

	anim = GetAnim(hdr, model, seqdesc) + bone_index;
	bone = (goldsrc::mstudiobone_t*)((uint8_t*)hdr + hdr->boneindex) + bone_index;

	anim_time = (clientTime - vm->curstate.animtime) * seqdesc->fps;

	frame = (int)floor(anim_time);
	next_frame = frame + 1;
	t = anim_time - frame;

	if (seqdesc->flags & goldsrc::STUDIO_LOOPING)
	{
		frame %= seqdesc->numframes;
		next_frame %= seqdesc->numframes;
	}
	else
	{
		frame = std::min(frame, seqdesc->numframes - 1);
		next_frame = std::min(next_frame, seqdesc->numframes - 1);
	}

	UnpackRotation(angles1, frame, bone, anim);
	UnpackRotation(angles2, next_frame, bone, anim);

	ncl_math::AngleLerp(angles1, angles2, t, angles);
	return;
}

void CameraApplyMovement(goldsrc::ref_params_t* pparams)
{
	static goldsrc::studiohdr_t* last_hdr;
	static vec3_t prevMove, curMove;
	static float lerpTime;

	float clientTime = gEngfuncs.GetClientTime();

	vec3_t new_movement;
	float scale;
	goldsrc::cl_entity_t* vm;
	goldsrc::model_t* model;
	goldsrc::studiohdr_t* hdr;

	vm = gEngfuncs.GetViewModel();
	CameraCalcMovement(vm, new_movement);

	/* check if weapon has changed */
	model = vm->model;
	hdr = model ? (goldsrc::studiohdr_t*)model->cache.data : NULL;

	if (hdr != last_hdr)
	{
		last_hdr = hdr;

		if (camera_movement_interp->value)
		{
			lerpTime = clientTime;
			ncl_math::VectorCopy(curMove, prevMove);
		}
	}

	if (lerpTime)
	{
		float frac = (clientTime - lerpTime) / camera_movement_interp->value;

		if (frac < 0 || frac >= 1)
		{
			/* just copy */
			ncl_math::VectorCopy(new_movement, curMove);
			lerpTime = 0;
		}
		else
		{
			ncl_math::AngleLerp(prevMove, new_movement, frac, curMove);
		}
	}
	else
	{
		/* just copy */
		ncl_math::VectorCopy(new_movement, curMove);
	}

	scale = camera_movement_scale->value;

	ncl_math::VectorMA(curMove, scale, pparams->viewangles);
}
