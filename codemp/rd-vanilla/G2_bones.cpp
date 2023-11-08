/*
===========================================================================
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2013 - 2015, OpenJK contributors

This file is part of the OpenJK source code.

OpenJK is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.
===========================================================================
*/

#include "qcommon/matcomp.h"

#include "ghoul2/G2.h"
#include "ghoul2/g2_local.h"

//rww - RAGDOLL_BEGIN
#ifndef __linux__
#include <cfloat>
#else
#include <math.h>
#endif
#include "ghoul2/G2_gore.h"
#include "tr_local.h"

//#define RAG_TRACE_DEBUG_LINES

#include "client/client.h" //while this is all "shared" code, there are some places where we want to make cgame callbacks (for ragdoll) only if the cgvm exists
//rww - RAGDOLL_END

//=====================================================================================================================
// Bone List handling routines - so entities can override bone info on a bone by bone level, and also interrogate this info

// Given a bone name, see if that bone is already in our bone list - note the model_t pointer that gets passed in here MUST point at the
// gla file, not the glm file type.
int G2_Find_Bone(const model_t* mod, const boneInfo_v& blist, const char* bone_name)
{
	// look through entire list
	for (size_t i = 0; i < blist.size(); i++)
	{
		const mdxaSkelOffsets_t* offsets = reinterpret_cast<mdxaSkelOffsets_t*>(reinterpret_cast<byte*>(mod->mdxa) + sizeof(mdxaHeader_t));
		// if this bone entry has no info in it, bounce over it
		if (blist[i].boneNumber == -1)
		{
			continue;
		}

		// figure out what skeletal info structure this bone entry is looking at
		const mdxaSkel_t* skel = reinterpret_cast<mdxaSkel_t*>(reinterpret_cast<byte*>(mod->mdxa) + sizeof(mdxaHeader_t) + offsets->offsets[blist[i].boneNumber]);

		// if name is the same, we found it
		if (!Q_stricmp(skel->name, bone_name))
		{
			return i;
		}
	}

	// didn't find it
	return -1;
}

// we need to add a bone to the list - find a free one and see if we can find a corresponding bone in the gla file
int G2_Add_Bone(const model_t* mod, boneInfo_v& blist, const char* bone_name)
{
	int x;
	mdxaSkel_t* skel;
	boneInfo_t temp_bone;

	//rww - RAGDOLL_BEGIN
	memset(&temp_bone, 0, sizeof temp_bone);
	//rww - RAGDOLL_END

	const mdxaSkelOffsets_t* offsets = reinterpret_cast<mdxaSkelOffsets_t*>(reinterpret_cast<byte*>(mod->mdxa) + sizeof(mdxaHeader_t));

	// walk the entire list of bones in the gla file for this model and see if any match the name of the bone we want to find
	for (x = 0; x < mod->mdxa->numBones; x++)
	{
		skel = reinterpret_cast<mdxaSkel_t*>(reinterpret_cast<byte*>(mod->mdxa) + sizeof(mdxaHeader_t) + offsets->offsets[x]);
		// if name is the same, we found it
		if (!Q_stricmp(skel->name, bone_name))
		{
			break;
		}
	}

	// check to see we did actually make a match with a bone in the model
	if (x == mod->mdxa->numBones)
	{
		// didn't find it? Error
		//assert(0);
#ifdef _DEBUG
		ri->Printf(PRINT_ALL, "WARNING: Failed to add bone %s\n", bone_name);
#endif

#ifdef _RAG_PRINT_TEST
		ri->Printf(PRINT_ALL, "WARNING: Failed to add bone %s\n", boneName);
#endif
		return -1;
	}

	// look through entire list - see if it's already there first
	for (size_t i = 0; i < blist.size(); i++)
	{
		// if this bone entry has info in it, bounce over it
		if (blist[i].boneNumber != -1)
		{
			skel = reinterpret_cast<mdxaSkel_t*>(reinterpret_cast<byte*>(mod->mdxa) + sizeof(mdxaHeader_t) + offsets->offsets[blist[i].boneNumber]);
			// if name is the same, we found it
			if (!Q_stricmp(skel->name, bone_name))
			{
				return i;
			}
		}
		else
		{
			// if we found an entry that had a -1 for the bonenumber, then we hit a bone slot that was empty
			blist[i].boneNumber = x;
			blist[i].flags = 0;
			return i;
		}
	}

#ifdef _RAG_PRINT_TEST
	ri->Printf(PRINT_ALL, "New bone added for %s\n", boneName);
#endif
	// ok, we didn't find an existing bone of that name, or an empty slot. Lets add an entry
	temp_bone.boneNumber = x;
	temp_bone.flags = 0;
	blist.push_back(temp_bone);
	return blist.size() - 1;
}

// Given a model handle, and a bone name, we want to remove this bone from the bone override list
qboolean G2_Remove_Bone_Index(boneInfo_v& blist, const int index)
{
	if (index != -1)
	{
		if (blist[index].flags & BONE_ANGLES_RAGDOLL)
		{
			return qtrue; // don't accept any calls on ragdoll bones
		}
	}

	// did we find it?
	if (index != -1)
	{
		// check the flags first - if it's still being used Do NOT remove it
		if (!blist[index].flags)
		{
			// set this bone to not used
			blist[index].boneNumber = -1;

			unsigned int new_size = blist.size();
			// now look through the list from the back and see if there is a block of -1's we can resize off the end of the list
			for (int i = blist.size() - 1; i > -1; i--)
			{
				if (blist[i].boneNumber == -1)
				{
					new_size = i;
				}
				// once we hit one that isn't a -1, we are done.
				else
				{
					break;
				}
			}
			// do we need to resize?
			if (new_size != blist.size())
			{
				// yes, so lets do it
				blist.resize(new_size);
			}

			return qtrue;
		}
	}

	//	assert(0);
		// no
	return qfalse;
}

// given a bone number, see if there is an override bone in the bone list
int	G2_Find_Bone_In_List(const boneInfo_v& blist, const int bone_num)
{
	// look through entire list
	for (size_t i = 0; i < blist.size(); i++)
	{
		if (blist[i].boneNumber == bone_num)
		{
			return i;
		}
	}
	return -1;
}

// given a model, bonelist and bonename, lets stop an anim if it's playing.
qboolean G2_Stop_Bone_Index(boneInfo_v& blist, const int index, const int flags)
{
	// did we find it?
	if (index != -1)
	{
		blist[index].flags &= ~flags;
		// try and remove this bone if we can
		return G2_Remove_Bone_Index(blist, index);
	}

	assert(0);

	return qfalse;
}

// generate a matrix for a given bone given some new angles for it.
void G2_Generate_Matrix(const model_t* mod, boneInfo_v& blist, const int index, const float* angles, const int flags,
	const Eorientations up, const Eorientations left, const Eorientations forward)
{
	mdxaBone_t		temp1;
	mdxaBone_t		permutation{};
	mdxaBone_t* bone_override = &blist[index].matrix;

	if (flags & (BONE_ANGLES_PREMULT | BONE_ANGLES_POSTMULT))
	{
		// build us a matrix out of the angles we are fed - but swap y and z because of wacky Quake setup
		vec3_t	new_angles{};

		// determine what axis newAngles Yaw should revolve around
		switch (up)
		{
		case NEGATIVE_X:
			new_angles[1] = angles[2] + 180;
			break;
		case POSITIVE_X:
			new_angles[1] = angles[2];
			break;
		case NEGATIVE_Y:
			new_angles[1] = angles[0];
			break;
		case POSITIVE_Y:
			new_angles[1] = angles[0];
			break;
		case NEGATIVE_Z:
			new_angles[1] = angles[1] + 180;
			break;
		case POSITIVE_Z:
			new_angles[1] = angles[1];
			break;
		default:
			break;
		}

		// determine what axis newAngles pitch should revolve around
		switch (left)
		{
		case NEGATIVE_X:
			new_angles[0] = angles[2];
			break;
		case POSITIVE_X:
			new_angles[0] = angles[2] + 180;
			break;
		case NEGATIVE_Y:
			new_angles[0] = angles[0];
			break;
		case POSITIVE_Y:
			new_angles[0] = angles[0] + 180;
			break;
		case NEGATIVE_Z:
			new_angles[0] = angles[1];
			break;
		case POSITIVE_Z:
			new_angles[0] = angles[1];
			break;
		default:
			break;
		}

		// determine what axis newAngles Roll should revolve around
		switch (forward)
		{
		case NEGATIVE_X:
			new_angles[2] = angles[2];
			break;
		case POSITIVE_X:
			new_angles[2] = angles[2];
			break;
		case NEGATIVE_Y:
			new_angles[2] = angles[0];
			break;
		case POSITIVE_Y:
			new_angles[2] = angles[0] + 180;
			break;
		case NEGATIVE_Z:
			new_angles[2] = angles[1];
			break;
		case POSITIVE_Z:
			new_angles[2] = angles[1] + 180;
			break;
		default:
			break;
		}

		Create_Matrix(new_angles, bone_override);

		// figure out where the bone hirearchy info is
		const mdxaSkelOffsets_t* offsets = reinterpret_cast<mdxaSkelOffsets_t*>(reinterpret_cast<byte*>(mod->mdxa) + sizeof(mdxaHeader_t));
		const auto skel = reinterpret_cast<mdxaSkel_t*>(reinterpret_cast<byte*>(mod->mdxa) + sizeof(mdxaHeader_t) + offsets->offsets[blist[index].
			boneNumber]);

		Multiply_3x4Matrix(&temp1, bone_override, &skel->BasePoseMatInv);
		Multiply_3x4Matrix(bone_override, &skel->BasePoseMat, &temp1);
	}
	else
	{
		vec3_t new_angles;
		VectorCopy(angles, new_angles);

		// why I should need do this Fuck alone knows. But I do.
		if (left == POSITIVE_Y)
		{
			new_angles[0] += 180;
		}

		Create_Matrix(new_angles, &temp1);

		permutation.matrix[0][0] = permutation.matrix[0][1] = permutation.matrix[0][2] = permutation.matrix[0][3] = 0;
		permutation.matrix[1][0] = permutation.matrix[1][1] = permutation.matrix[1][2] = permutation.matrix[1][3] = 0;
		permutation.matrix[2][0] = permutation.matrix[2][1] = permutation.matrix[2][2] = permutation.matrix[2][3] = 0;

		// determine what axis newAngles Yaw should revolve around
		switch (forward)
		{
		case NEGATIVE_X:
			permutation.matrix[0][0] = -1;		// works
			break;
		case POSITIVE_X:
			permutation.matrix[0][0] = 1;		// works
			break;
		case NEGATIVE_Y:
			permutation.matrix[1][0] = -1;
			break;
		case POSITIVE_Y:
			permutation.matrix[1][0] = 1;
			break;
		case NEGATIVE_Z:
			permutation.matrix[2][0] = -1;
			break;
		case POSITIVE_Z:
			permutation.matrix[2][0] = 1;
			break;
		default:
			break;
		}

		// determine what axis newAngles pitch should revolve around
		switch (left)
		{
		case NEGATIVE_X:
			permutation.matrix[0][1] = -1;
			break;
		case POSITIVE_X:
			permutation.matrix[0][1] = 1;
			break;
		case NEGATIVE_Y:
			permutation.matrix[1][1] = -1;		// works
			break;
		case POSITIVE_Y:
			permutation.matrix[1][1] = 1;		// works
			break;
		case NEGATIVE_Z:
			permutation.matrix[2][1] = -1;
			break;
		case POSITIVE_Z:
			permutation.matrix[2][1] = 1;
			break;
		default:
			break;
		}

		// determine what axis newAngles Roll should revolve around
		switch (up)
		{
		case NEGATIVE_X:
			permutation.matrix[0][2] = -1;
			break;
		case POSITIVE_X:
			permutation.matrix[0][2] = 1;
			break;
		case NEGATIVE_Y:
			permutation.matrix[1][2] = -1;
			break;
		case POSITIVE_Y:
			permutation.matrix[1][2] = 1;
			break;
		case NEGATIVE_Z:
			permutation.matrix[2][2] = -1;		// works
			break;
		case POSITIVE_Z:
			permutation.matrix[2][2] = 1;		// works
			break;
		default:
			break;
		}

		Multiply_3x4Matrix(bone_override, &temp1, &permutation);
	}

	// keep a copy of the matrix in the newmatrix which is actually what we use
	memcpy(&blist[index].newMatrix, &blist[index].matrix, sizeof(mdxaBone_t));
}

//=========================================================================================
//// Public Bone Routines

// Given a model handle, and a bone name, we want to remove this bone from the bone override list
qboolean G2_Remove_Bone(const CGhoul2Info* ghl_info, boneInfo_v& blist, const char* bone_name)
{
	assert(ghl_info->animModel);
	const int index = G2_Find_Bone(ghl_info->animModel, blist, bone_name);

	return G2_Remove_Bone_Index(blist, index);
}

#define DEBUG_PCJ (0)

// Given a model handle, and a bone name, we want to set angles specifically for overriding
qboolean G2_Set_Bone_Angles_Index(boneInfo_v& blist, const int index,
	const float* angles, const int flags, const Eorientations yaw,
	const Eorientations pitch, const Eorientations roll,
	const int blend_time, const int current_time)
{
	if (index >= static_cast<int>(blist.size()) || blist[index].boneNumber == -1)
	{
		// we are attempting to set a bone override that doesn't exist
		assert(0);
		return qfalse;
	}

	if (index != -1)
	{
		if (blist[index].flags & BONE_ANGLES_RAGDOLL)
		{
			return qtrue; // don't accept any calls on ragdoll bones
		}
	}

	if (flags & (BONE_ANGLES_PREMULT | BONE_ANGLES_POSTMULT))
	{
		// you CANNOT call this with an index with these kinds of bone overrides - we need the model details for these kinds of bone angle overrides
		assert(0);
		return qfalse;
	}

	// yes, so set the angles and flags correctly
	blist[index].flags &= ~(BONE_ANGLES_TOTAL);
	blist[index].flags |= flags;
	blist[index].boneBlendStart = current_time;
	blist[index].boneBlendTime = blend_time;
#if DEBUG_PCJ
	Com_OPrintf("PCJ %2d %6d   (%6.2f,%6.2f,%6.2f) %d %d %d %d\n", index, current_time, angles[0], angles[1], angles[2], yaw, pitch, roll, flags);
#endif

	G2_Generate_Matrix(nullptr, blist, index, angles, flags, yaw, pitch, roll);
	return qtrue;
}

// Given a model handle, and a bone name, we want to set angles specifically for overriding
qboolean G2_Set_Bone_Angles(const CGhoul2Info* ghl_info, boneInfo_v& blist, const char* bone_name, const float* angles,
	const int flags, const Eorientations up, const Eorientations left, const Eorientations forward,
	const int blend_time, const int current_time)
{
	const model_t* mod_a = const_cast<model_t*>(ghl_info->animModel);

	int			index = G2_Find_Bone(mod_a, blist, bone_name);

	// did we find it?
	if (index != -1)
	{
		if (blist[index].flags & BONE_ANGLES_RAGDOLL)
		{
			return qtrue; // don't accept any calls on ragdoll bones
		}

		// yes, so set the angles and flags correctly
		blist[index].flags &= ~(BONE_ANGLES_TOTAL);
		blist[index].flags |= flags;
		blist[index].boneBlendStart = current_time;
		blist[index].boneBlendTime = blend_time;
#if DEBUG_PCJ
		Com_OPrintf("%2d %6d   (%6.2f,%6.2f,%6.2f) %d %d %d %d\n", index, current_time, angles[0], angles[1], angles[2], up, left, forward, flags);
#endif

		G2_Generate_Matrix(mod_a, blist, index, angles, flags, up, left, forward);
		return qtrue;
	}

	// no - lets try and add this bone in
	index = G2_Add_Bone(mod_a, blist, bone_name);

	// did we find a free one?
	if (index != -1)
	{
		// yes, so set the angles and flags correctly
		blist[index].flags &= ~(BONE_ANGLES_TOTAL);
		blist[index].flags |= flags;
		blist[index].boneBlendStart = current_time;
		blist[index].boneBlendTime = blend_time;
#if DEBUG_PCJ
		Com_OPrintf("%2d %6d   (%6.2f,%6.2f,%6.2f) %d %d %d %d\n", index, current_time, angles[0], angles[1], angles[2], up, left, forward, flags);
#endif

		G2_Generate_Matrix(mod_a, blist, index, angles, flags, up, left, forward);
		return qtrue;
	}
	//	assert(0);
		//Jeese, we don't need an assert here too. There's already a warning in G2_Add_Bone if it fails.

		// no
	return qfalse;
}

// Given a model handle, and a bone name, we want to set angles specifically for overriding - using a matrix directly
qboolean G2_Set_Bone_Angles_Matrix_Index(boneInfo_v& blist, const int index,
	const mdxaBone_t& matrix, const int flags,
	const int blend_time, const int current_time)
{
	if (index >= static_cast<int>(blist.size()) || blist[index].boneNumber == -1)
	{
		// we are attempting to set a bone override that doesn't exist
		assert(0);
		return qfalse;
	}
	if (index != -1)
	{
		if (blist[index].flags & BONE_ANGLES_RAGDOLL)
		{
			return qtrue; // don't accept any calls on ragdoll bones
		}
	}
	// yes, so set the angles and flags correctly
	blist[index].flags &= ~(BONE_ANGLES_TOTAL);
	blist[index].flags |= flags;
	blist[index].boneBlendStart = current_time;
	blist[index].boneBlendTime = blend_time;

	memcpy(&blist[index].matrix, &matrix, sizeof(mdxaBone_t));
	memcpy(&blist[index].newMatrix, &matrix, sizeof(mdxaBone_t));
	return qtrue;
}

// Given a model handle, and a bone name, we want to set angles specifically for overriding - using a matrix directly
qboolean G2_Set_Bone_Angles_Matrix(const char* file_name, boneInfo_v& blist, const char* bone_name, const mdxaBone_t& matrix,
	const int flags, const qhandle_t* model_list, const int model_index)
{
	model_t* mod_m;
	if (!file_name[0])
	{
		mod_m = R_GetModelByHandle(model_list[model_index]);
	}
	else
	{
		mod_m = R_GetModelByHandle(RE_RegisterModel(file_name));
	}
	const model_t* mod_a = R_GetModelByHandle(mod_m->mdxm->animIndex);
	int			index = G2_Find_Bone(mod_a, blist, bone_name);

	if (index != -1)
	{
		if (blist[index].flags & BONE_ANGLES_RAGDOLL)
		{
			return qtrue; // don't accept any calls on ragdoll bones
		}
	}

	// did we find it?
	if (index != -1)
	{
		// yes, so set the angles and flags correctly
		blist[index].flags &= ~(BONE_ANGLES_TOTAL);
		blist[index].flags |= flags;

		memcpy(&blist[index].matrix, &matrix, sizeof(mdxaBone_t));
		memcpy(&blist[index].newMatrix, &matrix, sizeof(mdxaBone_t));
		return qtrue;
	}

	// no - lets try and add this bone in
	index = G2_Add_Bone(mod_a, blist, bone_name);

	// did we find a free one?
	if (index != -1)
	{
		// yes, so set the angles and flags correctly
		blist[index].flags &= ~(BONE_ANGLES_TOTAL);
		blist[index].flags |= flags;

		memcpy(&blist[index].matrix, &matrix, sizeof(mdxaBone_t));
		memcpy(&blist[index].newMatrix, &matrix, sizeof(mdxaBone_t));
		return qtrue;
	}
	assert(0);

	// no
	return qfalse;
}

#define DEBUG_G2_TIMING (0)

// given a model, bone name, a bonelist, a start/end frame number, a anim speed and some anim flags, set up or modify an existing bone entry for a new set of anims
qboolean G2_Set_Bone_Anim_Index(
	boneInfo_v& blist,
	const int index,
	const int start_frame,
	const int end_frame,
	const int flags,
	const float anim_speed,
	const int current_time,
	const float set_frame,
	const int ablend_time,
	const int num_frames)
{
	int			mod_flags = flags;

	if (index >= static_cast<int>(blist.size()) || blist[index].boneNumber == -1)
	{
		// we are attempting to set a bone override that doesn't exist
		assert(0);
		return qfalse;
	}

	if (index != -1)
	{
		if (blist[index].flags & BONE_ANGLES_RAGDOLL)
		{
			return qtrue; // don't accept any calls on ragdoll bones
		}

		//mark it for needing a transform for the cached trace transform stuff
		blist[index].flags |= BONE_NEED_TRANSFORM;
	}

	if (set_frame != -1)
	{
		assert(set_frame >= start_frame && set_frame <= end_frame);
	}
	if (flags & BONE_ANIM_BLEND)
	{
		float current_frame, speed;
		int frame, end_frame1, flags1;
		// figure out where we are now
		if (G2_Get_Bone_Anim_Index(blist, index, current_time, &current_frame, &frame, &end_frame1, &flags1, &speed, num_frames))
		{
			if (blist[index].blendStart == current_time)	//we're replacing a blend in progress which hasn't started
			{
				// set the amount of time it's going to take to blend this anim with the last frame of the last one
				blist[index].blend_time = ablend_time;
			}
			else
			{
				if (speed < 0.0f)
				{
					blist[index].blendFrame = floor(current_frame);
					blist[index].blendLerpFrame = floor(current_frame);
				}
				else
				{
					blist[index].blendFrame = current_frame;
					blist[index].blendLerpFrame = current_frame + 1;

					// cope with if the lerp frame is actually off the end of the anim
					if (blist[index].blendFrame >= end_frame1)
					{
						// we only want to lerp with the first frame of the anim if we are looping
						if (blist[index].flags & BONE_ANIM_OVERRIDE_LOOP)
						{
							blist[index].blendFrame = frame;
						}
						// if we intend to end this anim or freeze after this, then just keep on the last frame
						else
						{
							//	assert(end_frame>0);
							if (end_frame1 <= 0)
							{
								blist[index].blendLerpFrame = 0;
							}
							else
							{
								blist[index].blendFrame = end_frame1 - 1;
							}
						}
					}

					// cope with if the lerp frame is actually off the end of the anim
					if (blist[index].blendLerpFrame >= end_frame1)
					{
						// we only want to lerp with the first frame of the anim if we are looping
						if (blist[index].flags & BONE_ANIM_OVERRIDE_LOOP)
						{
							blist[index].blendLerpFrame = frame;
						}
						// if we intend to end this anim or freeze after this, then just keep on the last frame
						else
						{
							//	assert(end_frame>0);
							if (end_frame1 <= 0)
							{
								blist[index].blendLerpFrame = 0;
							}
							else
							{
								blist[index].blendLerpFrame = end_frame1 - 1;
							}
						}
					}
				}
				// set the amount of time it's going to take to blend this anim with the last frame of the last one
				blist[index].blend_time = ablend_time;
				blist[index].blendStart = current_time;
			}
		}
		// hmm, we weren't animating on this bone. In which case disable the blend
		else
		{
			blist[index].blendFrame = blist[index].blendLerpFrame = 0;
			blist[index].blend_time = 0;
			mod_flags &= ~BONE_ANIM_BLEND;
		}
	}
	else
	{
		blist[index].blendFrame = blist[index].blendLerpFrame = 0;
		blist[index].blend_time = blist[index].blendStart = 0;
		// we aren't blending, so remove the option to do so
		mod_flags &= ~BONE_ANIM_BLEND;
	}
	// yes, so set the anim data and flags correctly
	blist[index].end_frame = end_frame;
	blist[index].start_frame = start_frame;
	blist[index].animSpeed = anim_speed;
	blist[index].pauseTime = 0;
	// start up the animation:)
	if (set_frame != -1)
	{
		blist[index].lastTime = blist[index].startTime = current_time - (set_frame - static_cast<float>(start_frame)) * 50.0 / anim_speed;
	}
	else
	{
		blist[index].lastTime = blist[index].startTime = current_time;
	}
	blist[index].flags &= ~(BONE_ANIM_TOTAL);
	if (blist[index].flags < 0)
	{
		blist[index].flags = 0;
	}
	blist[index].flags |= mod_flags;

#if DEBUG_G2_TIMING
	if (index == 2)
	{
		const boneInfo_t& bone = blist[index];
		char mess[1000];
		if (bone.flags & BONE_ANIM_BLEND)
		{
			sprintf(mess, "sab[%2d] %5d  %5d  (%5d-%5d) %4.2f %4x   bt(%5d-%5d) %7.2f %5d\n",
				index,
				current_time,
				bone.startTime,
				bone.start_frame,
				bone.end_frame,
				bone.animSpeed,
				bone.flags,
				bone.blendStart,
				bone.blendStart + bone.blend_time,
				bone.blendFrame,
				bone.blendLerpFrame
			);
		}
		else
		{
			sprintf(mess, "saa[%2d] %5d  %5d  (%5d-%5d) %4.2f %4x\n",
				index,
				current_time,
				bone.startTime,
				bone.start_frame,
				bone.end_frame,
				bone.animSpeed,
				bone.flags
			);
		}
		Com_OPrintf("%s", mess);
	}
#endif

	return qtrue;
}

// given a model, bone name, a bonelist, a start/end frame number, a anim speed and some anim flags, set up or modify an existing bone entry for a new set of anims
qboolean G2_Set_Bone_Anim(const CGhoul2Info* ghl_info,
	boneInfo_v& blist,
	const char* bone_name,
	const int start_frame,
	const int end_frame,
	const int flags,
	const float anim_speed,
	const int current_time,
	const float set_frame,
	const int blend_time)
{
	const model_t* mod_a = const_cast<model_t*>(ghl_info->animModel);

	int			index = G2_Find_Bone(mod_a, blist, bone_name);
	if (index == -1)
	{
		index = G2_Add_Bone(mod_a, blist, bone_name);
	}

	if (index != -1)
	{
		if (blist[index].flags & BONE_ANGLES_RAGDOLL)
		{
			return qtrue; // don't accept any calls on ragdoll bones
		}
	}

	if (index != -1)
	{
		return G2_Set_Bone_Anim_Index(blist, index, start_frame, end_frame, flags, anim_speed, current_time, set_frame, blend_time, ghl_info->aHeader->numFrames);
	}
	return qfalse;
}

qboolean G2_Get_Bone_Anim_Range(const CGhoul2Info* ghl_info, const boneInfo_v& blist, const char* bone_name, int* start_frame, int* end_frame)
{
	const model_t* mod_a = const_cast<model_t*>(ghl_info->animModel);
	const int			index = G2_Find_Bone(mod_a, blist, bone_name);

	// did we find it?
	if (index != -1)
	{
		// are we an animating bone?
		if (blist[index].flags & (BONE_ANIM_OVERRIDE_LOOP | BONE_ANIM_OVERRIDE))
		{
			*start_frame = blist[index].start_frame;
			*end_frame = blist[index].end_frame;
			return qtrue;
		}
	}
	return qfalse;
}

// given a model, bonelist and bonename, return the current frame, startframe and endframe of the current animation
// NOTE if we aren't running an animation, then qfalse is returned
void G2_TimingModel(boneInfo_t& bone, int current_time, int num_frames_in_file, int& current_frame, int& new_frame, float& lerp);

qboolean G2_Get_Bone_Anim_Index(boneInfo_v& blist, const int index, const int current_time,
	float* current_frame, int* start_frame, int* end_frame, int* flags, float* ret_anim_speed, const int num_frames)
{
	// did we find it?
	if (index >= 0 && !(index >= static_cast<int>(blist.size()) || blist[index].boneNumber == -1))
	{
		// are we an animating bone?
		if (blist[index].flags & (BONE_ANIM_OVERRIDE_LOOP | BONE_ANIM_OVERRIDE))
		{
			int lcurrentFrame, newFrame;
			float lerp;
			G2_TimingModel(blist[index], current_time, num_frames, lcurrentFrame, newFrame, lerp);

			*current_frame = static_cast<float>(lcurrentFrame) + lerp;
			*start_frame = blist[index].start_frame;
			*end_frame = blist[index].end_frame;
			*flags = blist[index].flags;
			*ret_anim_speed = blist[index].animSpeed;
			return qtrue;
		}
	}
	*start_frame = 0;
	*end_frame = 1;
	*current_frame = 0.0f;
	*flags = 0;
	*ret_anim_speed = 0.0f;
	return qfalse;
}

// given a model, bonelist and bonename, return the current frame, startframe and endframe of the current animation
// NOTE if we aren't running an animation, then qfalse is returned
qboolean G2_Get_Bone_Anim(const CGhoul2Info* ghl_info, boneInfo_v& blist, const char* bone_name, const int current_time,
	float* current_frame, int* start_frame, int* end_frame, int* flags, float* ret_anim_speed)
{
	const model_t* mod_a = const_cast<model_t*>(ghl_info->animModel);

	int			index = G2_Find_Bone(mod_a, blist, bone_name);

	if (index == -1)
	{
		index = G2_Add_Bone(mod_a, blist, bone_name);

		if (index == -1)
		{
			return qfalse;
		}
	}

	assert(ghl_info->aHeader);

	if (G2_Get_Bone_Anim_Index(blist, index, current_time, current_frame, start_frame, end_frame, flags, ret_anim_speed, ghl_info->aHeader->numFrames))
	{
		assert(*start_frame >= 0 && *start_frame < ghl_info->aHeader->numFrames);
		assert(*end_frame > 0 && *end_frame <= ghl_info->aHeader->numFrames);
		assert(*current_frame >= 0.0f && static_cast<int>(*current_frame) < ghl_info->aHeader->numFrames);
		return qtrue;
	}

	return qfalse;
}

// given a model, bonelist and bonename, lets pause an anim if it's playing.
qboolean G2_Pause_Bone_Anim(const CGhoul2Info* ghl_info, boneInfo_v& blist, const char* bone_name, const int current_time)
{
	const model_t* mod_a = const_cast<model_t*>(ghl_info->animModel);

	const int			index = G2_Find_Bone(mod_a, blist, bone_name);

	// did we find it?
	if (index != -1)
	{
		// are we pausing or un pausing?
		if (blist[index].pauseTime)
		{
			int		start_frame = 0, end_frame = 0, flags = 0;
			float	current_frame = 0.0f, animSpeed = 1.0f;

			// figure out what frame we are on now
			G2_Get_Bone_Anim(ghl_info, blist, bone_name, blist[index].pauseTime, &current_frame, &start_frame, &end_frame, &flags, &animSpeed);
			// reset start time so we are actually on this frame right now
			G2_Set_Bone_Anim(ghl_info, blist, bone_name, start_frame, end_frame, flags, animSpeed, current_time, current_frame, 0);
			// no pausing anymore
			blist[index].pauseTime = 0;
		}
		// ahh, just pausing, the easy bit
		else
		{
			blist[index].pauseTime = current_time;
		}

		return qtrue;
	}
	assert(0);

	return qfalse;
}

qboolean	G2_IsPaused(const char* file_name, const boneInfo_v& blist, const char* bone_name)
{
	const model_t* mod_m = R_GetModelByHandle(RE_RegisterModel(file_name));
	const model_t* mod_a = R_GetModelByHandle(mod_m->mdxm->animIndex);
	const int			index = G2_Find_Bone(mod_a, blist, bone_name);

	// did we find it?
	if (index != -1)
	{
		// are we paused?
		if (blist[index].pauseTime)
		{
			// yup. paused.
			return qtrue;
		}
		return qfalse;
	}

	return qfalse;
}

// given a model, bonelist and bonename, lets stop an anim if it's playing.
qboolean G2_Stop_Bone_Anim_Index(boneInfo_v& blist, const int index)
{
	if (index >= static_cast<int>(blist.size()) || blist[index].boneNumber == -1)
	{
		// we are attempting to set a bone override that doesn't exist
		assert(0);
		return qfalse;
	}

	blist[index].flags &= ~(BONE_ANIM_TOTAL);
	// try and remove this bone if we can
	return G2_Remove_Bone_Index(blist, index);
}

// given a model, bonelist and bonename, lets stop an anim if it's playing.
qboolean G2_Stop_Bone_Anim(const char* file_name, boneInfo_v& blist, const char* bone_name)
{
	const model_t* mod_m = R_GetModelByHandle(RE_RegisterModel(file_name));
	const model_t* mod_a = R_GetModelByHandle(mod_m->mdxm->animIndex);
	const int			index = G2_Find_Bone(mod_a, blist, bone_name);

	// did we find it?
	if (index != -1)
	{
		blist[index].flags &= ~(BONE_ANIM_TOTAL);
		// try and remove this bone if we can
		return G2_Remove_Bone_Index(blist, index);
	}
	assert(0);

	return qfalse;
}

// given a model, bonelist and bonename, lets stop an anim if it's playing.
qboolean G2_Stop_Bone_Angles_Index(boneInfo_v& blist, const int index)
{
	if (index >= static_cast<int>(blist.size()) || blist[index].boneNumber == -1)
	{
		// we are attempting to set a bone override that doesn't exist
		assert(0);
		return qfalse;
	}

	blist[index].flags &= ~(BONE_ANGLES_TOTAL);
	// try and remove this bone if we can
	return G2_Remove_Bone_Index(blist, index);
}

// given a model, bonelist and bonename, lets stop an anim if it's playing.
qboolean G2_Stop_Bone_Angles(const char* file_name, boneInfo_v& blist, const char* bone_name)
{
	const model_t* mod_m = R_GetModelByHandle(RE_RegisterModel(file_name));
	const model_t* mod_a = R_GetModelByHandle(mod_m->mdxm->animIndex);
	const int			index = G2_Find_Bone(mod_a, blist, bone_name);

	// did we find it?
	if (index != -1)
	{
		blist[index].flags &= ~(BONE_ANGLES_TOTAL);
		// try and remove this bone if we can
		return G2_Remove_Bone_Index(blist, index);
	}
	assert(0);

	return qfalse;
}

// actually walk the bone list and update each and every bone if we have ended an animation for them.
void G2_Animate_Bone_List(CGhoul2Info_v& ghoul2, const int current_time, const int index)
{
	boneInfo_v& blist = ghoul2[index].mBlist;

	// look through entire list
	for (size_t i = 0; i < blist.size(); i++)
	{
		// we we a valid bone override?
		if (blist[i].boneNumber != -1)
		{
			// are we animating?
			if (blist[i].flags & (BONE_ANIM_OVERRIDE_LOOP | BONE_ANIM_OVERRIDE))
			{
				// yes - add in animation speed to current frame
				const float	anim_speed = blist[i].animSpeed;
				const float	end_frame = static_cast<float>(blist[i].end_frame);
				float	time = (current_time - blist[i].startTime) / 50.0f;
				// are we a paused anim?
				if (blist[i].pauseTime)
				{
					time = (blist[i].pauseTime - blist[i].startTime) / 50.0f;
				}
				if (time < 0.0f)
				{
					time = 0.0f;
				}
				float	new_frame_g = blist[i].start_frame + time * anim_speed;

				const int		anim_size = end_frame - blist[i].start_frame;
				// we are supposed to be animating right?
				if (anim_size)
				{
					// did we run off the end?
					if (anim_speed > 0.0f && new_frame_g > end_frame - 1 ||
						anim_speed < 0.0f && new_frame_g < end_frame + 1)
					{
						// yep - decide what to do
						if (blist[i].flags & BONE_ANIM_OVERRIDE_LOOP)
						{
							// get our new animation frame back within the bounds of the animation set
							if (anim_speed < 0.0f)
							{
								if (new_frame_g <= end_frame + 1)
								{
									new_frame_g = end_frame + fmod(new_frame_g - end_frame, anim_size) - anim_size;
								}
							}
							else
							{
								if (new_frame_g >= end_frame)
								{
									new_frame_g = end_frame + fmod(new_frame_g - end_frame, anim_size) - anim_size;
								}
							}
							// figure out new start time
							const float frame_time = new_frame_g - blist[i].start_frame;
							blist[i].startTime = current_time - static_cast<int>(frame_time / anim_speed * 50.0f);
							if (blist[i].startTime > current_time)
							{
								blist[i].startTime = current_time;
							}
							assert(blist[i].startTime <= current_time);
							blist[i].lastTime = blist[i].startTime;
						}
						else
						{
							if ((blist[i].flags & BONE_ANIM_OVERRIDE_FREEZE) != BONE_ANIM_OVERRIDE_FREEZE)
							{
								// nope, just stop it. And remove the bone if possible
								G2_Stop_Bone_Index(blist, i, BONE_ANIM_TOTAL);
							}
						}
					}
				}
			}
		}
	}
}

//rww - RAGDOLL_BEGIN
/*

  rag stuff

*/
static void G2_RagDollSolve(CGhoul2Info_v& ghoul2_v, int g2_index, float decay, bool limit_angles, const CRagDollUpdateParams
	* params = nullptr);
static void G2_RagDollCurrentPosition(CGhoul2Info_v& ghoul2_v, int g2_index, int frame_num, const vec3_t angles, const vec3_t position, const vec3_t scale);
static bool G2_RagDollSettlePositionNumeroTrois(CGhoul2Info_v& ghoul2_v, CRagDollUpdateParams* params, int cur_time);
static bool G2_RagDollSetup(CGhoul2Info& ghoul2, int frame_num, bool reset_origin, const vec3_t origin, bool any_rendered);

void G2_GetBoneBasepose(const CGhoul2Info& ghoul2, int bone_num, mdxaBone_t*& ret_basepose, mdxaBone_t*& ret_basepose_inv);
int G2_GetBoneDependents(CGhoul2Info& ghoul2, int bone_num, int* temp_dependents, int max_dep);
void G2_GetBoneMatrixLow(const CGhoul2Info& ghoul2, int bone_num, const vec3_t scale, mdxaBone_t& retMatrix, mdxaBone_t*& ret_basepose, mdxaBone_t*& ret_basepose_inv);
int G2_GetParentBoneMatrixLow(const CGhoul2Info& ghoul2, int bone_num, const vec3_t scale, mdxaBone_t& retMatrix, mdxaBone_t*& ret_basepose, mdxaBone_t*& ret_basepose_inv);
bool G2_WasBoneRendered(const CGhoul2Info& ghoul2, int bone_num);

#define MAX_BONES_RAG (256)

struct SRagEffector
{
	vec3_t		currentOrigin;
	vec3_t		desiredDirection;
	vec3_t		desiredOrigin;
	float		radius;
	float		weight;
};

#define RAG_MASK (CONTENTS_SOLID|CONTENTS_TERRAIN)//|CONTENTS_SHOTCLIP|CONTENTS_TERRAIN//(/*MASK_SOLID|*/CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_SHOTCLIP|CONTENTS_TERRAIN|CONTENTS_BODY)

extern cvar_t* broadsword;
extern cvar_t* broadsword_kickbones;
extern cvar_t* broadsword_kickorigin;
extern cvar_t* broadsword_dontstopanim;
extern cvar_t* broadsword_waitforshot;
extern cvar_t* broadsword_playflop;

extern cvar_t* broadsword_effcorr;

extern cvar_t* broadsword_ragtobase;

extern cvar_t* broadsword_dircap;

extern cvar_t* broadsword_extra1;
extern cvar_t* broadsword_extra2;

#define RAG_PCJ						(0x00001)
#define RAG_PCJ_POST_MULT			(0x00002)	// has the pcj flag as well
#define RAG_PCJ_MODEL_ROOT			(0x00004)	// has the pcj flag as well
#define RAG_PCJ_PELVIS				(0x00008)	// has the pcj flag and POST_MULT as well
#define RAG_EFFECTOR				(0x00100)
#define RAG_WAS_NOT_RENDERED		(0x01000)		// not particularily reliable, more of a hint
#define RAG_WAS_EVER_RENDERED		(0x02000)		// not particularily reliable, more of a hint
#define RAG_BONE_LIGHTWEIGHT		(0x04000)		//used to indicate a bone's velocity treatment
#define RAG_PCJ_IK_CONTROLLED		(0x08000)		//controlled from IK move input
#define RAG_UNSNAPPABLE				(0x10000)		//cannot be broken out of constraints ever

// thiese flags are on the model and correspond to...
//#define		GHOUL2_RESERVED_FOR_RAGDOLL 0x0ff0  // these are not defined here for dependecies sake
#define		GHOUL2_RAG_STARTED						0x0010  // we are actually a ragdoll
#define		GHOUL2_RAG_PENDING						0x0100  // got start death anim but not end death anim
#define		GHOUL2_RAG_DONE							0x0200		// got end death anim
#define		GHOUL2_RAG_COLLISION_DURING_DEATH		0x0400		// ever have gotten a collision (da) event
#define		GHOUL2_RAG_COLLISION_SLIDE				0x0800		// ever have gotten a collision (slide) event
#define		GHOUL2_RAG_FORCESOLVE					0x1000		//api-override, determine if ragdoll should be forced to continue solving even if it thinks it is settled

//#define flrand	Q_flrand

static mdxaBone_t* ragBasepose[MAX_BONES_RAG];
static mdxaBone_t* ragBaseposeInv[MAX_BONES_RAG];
static mdxaBone_t		ragBones[MAX_BONES_RAG];
static SRagEffector		ragEffectors[MAX_BONES_RAG];
static boneInfo_t* ragBoneData[MAX_BONES_RAG];
static int				tempDependents[MAX_BONES_RAG];
static int				ragBlistIndex[MAX_BONES_RAG];
static int				numRags;
static vec3_t			ragBoneMins;
static vec3_t			ragBoneMaxs;
static vec3_t			ragBoneCM;
static bool				haveDesiredPelvisOffset = false;
static vec3_t			desiredPelvisOffset; // this is for the root
static float			ragOriginChange = 0.0f;
static vec3_t			ragOriginChangeDir;
//debug
#if 0
static vec3_t			handPos = { 0,0,0 };
static vec3_t			handPos2 = { 0,0,0 };
#endif

enum ERagState
{
	ERS_DYNAMIC,
	ERS_SETTLING,
	ERS_SETTLED
};
static int				ragState;

static std::vector<boneInfo_t*>		rag;  // once we get the dependents precomputed this can be local

static void G2_Generate_MatrixRag(
	// caution this must not be called before the whole skeleton is "remembered"
	boneInfo_v& blist,
	const int					index)
{
	boneInfo_t& bone = blist[index];//.sent;

	memcpy(&bone.matrix, &bone.ragOverrideMatrix, sizeof(mdxaBone_t));
#ifdef _DEBUG
	for (int i = 0; i < 3; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			assert(!Q_isnan(bone.matrix.matrix[i][j]));
		}
	}
#endif// _DEBUG
	memcpy(&blist[index].newMatrix, &bone.matrix, sizeof(mdxaBone_t));
}

int G2_Find_Bone_Rag(const CGhoul2Info* ghl_info, const boneInfo_v& blist, const char* bone_name)
{
	const mdxaSkelOffsets_t* offsets = reinterpret_cast<mdxaSkelOffsets_t*>((byte*)ghl_info->aHeader + sizeof(mdxaHeader_t));

	/*
	model_t			*currentModel;
	model_t			*animModel;
	mdxaHeader_t	*aHeader;

	currentModel = R_GetModelByHandle(RE_RegisterModel(ghl_info->mFileName));
	assert(currentModel);
	animModel =  R_GetModelByHandle(currentModel->mdxm->animIndex);
	assert(animModel);
	aHeader = animModel->mdxa;
	assert(aHeader);

	offsets = (mdxaSkelOffsets_t *)((byte *)aHeader + sizeof(mdxaHeader_t));
	skel = (mdxaSkel_t *)((byte *)aHeader + sizeof(mdxaHeader_t) + offsets->offsets[0]);
	*/

	// look through entire list
	for (size_t i = 0; i < blist.size(); i++)
	{
		// if this bone entry has no info in it, bounce over it
		if (blist[i].boneNumber == -1)
		{
			continue;
		}

		// figure out what skeletal info structure this bone entry is looking at
		const mdxaSkel_t* skel = reinterpret_cast<mdxaSkel_t*>((byte*)ghl_info->aHeader + sizeof(mdxaHeader_t) + offsets->offsets[blist[i].
			boneNumber]);
		//skel = (mdxaSkel_t *)((byte *)aHeader + sizeof(mdxaHeader_t) + offsets->offsets[blist[i].boneNumber]);

		// if name is the same, we found it
		if (!Q_stricmp(skel->name, bone_name))
		{
			return i;
		}
	}
#if _DEBUG
	//	G2_Bone_Not_Found(boneName,ghl_info->mFileName);
#endif
	// didn't find it
	return -1;
}

static int G2_Set_Bone_Rag(
	boneInfo_v& blist,
	const char* bone_name,
	const CGhoul2Info& ghoul2,
	const vec3_t scale,
	const vec3_t origin)
{
	// do not change the state of the skeleton here
	int	index = G2_Find_Bone_Rag(&ghoul2, blist, bone_name);

	if (index == -1)
	{
		index = G2_Add_Bone(ghoul2.animModel, blist, bone_name);
	}

	if (index != -1)
	{
		boneInfo_t& bone = blist[index];
		VectorCopy(origin, bone.extraVec1);

		G2_GetBoneMatrixLow(ghoul2, bone.boneNumber, scale, bone.originalTrueBoneMatrix, bone.basepose, bone.baseposeInv);
		//		bone.parentRawBoneIndex=G2_GetParentBoneMatrixLow(ghoul2,bone.boneNumber,scale,bone.parentTrueBoneMatrix,bone.baseposeParent,bone.baseposeInvParent);
		assert(!Q_isnan(bone.originalTrueBoneMatrix.matrix[1][1]));
		assert(!Q_isnan(bone.originalTrueBoneMatrix.matrix[1][3]));
		bone.originalOrigin[0] = bone.originalTrueBoneMatrix.matrix[0][3];
		bone.originalOrigin[1] = bone.originalTrueBoneMatrix.matrix[1][3];
		bone.originalOrigin[2] = bone.originalTrueBoneMatrix.matrix[2][3];
	}
	return index;
}

static int G2_Set_Bone_Angles_Rag(
	const CGhoul2Info& ghoul2,
	boneInfo_v& blist,
	const char* bone_name,
	const int flags,
	const float radius,
	const vec3_t angle_min = nullptr,
	const vec3_t angle_max = nullptr,
	const int blend_time = 500)
{
	int			index = G2_Find_Bone_Rag(&ghoul2, blist, bone_name);

	if (index == -1)
	{
		index = G2_Add_Bone(ghoul2.animModel, blist, bone_name);
	}
	if (index != -1)
	{
		boneInfo_t& bone = blist[index];
		bone.flags &= ~(BONE_ANGLES_TOTAL);
		bone.flags |= BONE_ANGLES_RAGDOLL;
		if (flags & RAG_PCJ)
		{
			if (flags & RAG_PCJ_POST_MULT)
			{
				bone.flags |= BONE_ANGLES_POSTMULT;
			}
			else if (flags & RAG_PCJ_MODEL_ROOT)
			{
				bone.flags |= BONE_ANGLES_PREMULT;
				//				bone.flags |= BONE_ANGLES_POSTMULT;
			}
			else
			{
				assert(!"Invalid RAG PCJ\n");
			}
		}
		bone.ragStartTime = G2API_GetTime(0);
		bone.boneBlendStart = bone.ragStartTime;
		bone.boneBlendTime = blend_time;
		bone.radius = radius;
		bone.weight = 1.0f;

		//init the others to valid values
		bone.epGravFactor = 0;
		VectorClear(bone.epVelocity);
		bone.solidCount = 0;
		bone.physicsSettled = false;
		bone.snapped = false;

		bone.parentBoneIndex = -1;

		bone.offsetRotation = 0.0f;

		bone.overGradSpeed = 0.0f;
		VectorClear(bone.overGoalSpot);
		bone.hasOverGoal = false;
		bone.hasAnimFrameMatrix = -1;

		//		bone.weight=pow(radius,1.7f); //cubed was too harsh
		//		bone.weight=radius*radius*radius;
		if (angle_min && angle_max)
		{
			VectorCopy(angle_min, bone.minAngles);
			VectorCopy(angle_max, bone.maxAngles);
		}
		else
		{
			VectorCopy(bone.currentAngles, bone.minAngles); // I guess this isn't a rag pcj then
			VectorCopy(bone.currentAngles, bone.maxAngles);
		}
		if (!bone.lastTimeUpdated)
		{
			static mdxaBone_t		id =
			{
				{
					{ 1.0f, 0.0f, 0.0f, 0.0f },
					{ 0.0f, 1.0f, 0.0f, 0.0f },
					{ 0.0f, 0.0f, 1.0f, 0.0f }
				}
			};
			memcpy(&bone.ragOverrideMatrix, &id, sizeof(mdxaBone_t));
			VectorClear(bone.anglesOffset);
			VectorClear(bone.positionOffset);
			VectorClear(bone.velocityEffector);  // this is actually a velocity now
			VectorClear(bone.velocityRoot);  // this is actually a velocity now
			VectorClear(bone.lastPosition);
			VectorClear(bone.lastShotDir);
			bone.lastContents = 0;
			// if this is non-zero, we are in a dynamic state
			bone.firstCollisionTime = bone.ragStartTime;
			// if this is non-zero, we are in a settling state
			bone.restTime = 0;
			// if they are both zero, we are in a settled state

			bone.firstTime = 0;

			bone.RagFlags = flags;
			bone.DependentRagIndexMask = 0;

			G2_Generate_MatrixRag(blist, index); // set everything to th id

#if 0
			VectorClear(bone.currentAngles);
			//			VectorAdd(bone.minAngles,bone.maxAngles,bone.currentAngles);
			//			VectorScale(bone.currentAngles,0.5f,bone.currentAngles);
#else
			{
				if (
					flags & RAG_PCJ_MODEL_ROOT ||
					flags & RAG_PCJ_PELVIS ||
					!(flags & RAG_PCJ))
				{
					VectorClear(bone.currentAngles);
				}
				else
				{
					for (int k = 0; k < 3; k++)
					{
						float scalar = flrand(-1.0f, 1.0f);
						scalar *= flrand(-1.0f, 1.0f) * flrand(-1.0f, 1.0f);
						// this is a heavily central distribution
						// center it on .5 (and make it small)
						scalar *= 0.5f;
						scalar += 0.5f;

						bone.currentAngles[k] = (bone.minAngles[k] - bone.maxAngles[k]) * scalar + bone.maxAngles[k];
					}
				}
			}
			//			VectorClear(bone.currentAngles);
#endif
			VectorCopy(bone.currentAngles, bone.lastAngles);
		}
	}
	return index;
}

class CRagDollParams;
const mdxaHeader_t* G2_GetModA(const CGhoul2Info& ghoul2);

static void G2_RagDollMatchPosition()
{
	haveDesiredPelvisOffset = false;
	for (int i = 0; i < numRags; i++)
	{
		boneInfo_t& bone = *ragBoneData[i];
		SRagEffector& e = ragEffectors[i];

		vec3_t& desiredPos = e.desiredOrigin; // we will save this

		if (!(bone.RagFlags & RAG_EFFECTOR))
		{
			continue;
		}
		VectorCopy(bone.originalOrigin, desiredPos);
		VectorSubtract(desiredPos, e.currentOrigin, e.desiredDirection);
		VectorCopy(e.currentOrigin, bone.lastPosition); // last arg is dest
	}
}

qboolean G2_Set_Bone_Anim_No_BS(const CGhoul2Info& ghoul2, boneInfo_v& blist, const char* bone_name, const int arg_start_frame,
	const int arg_end_frame, const int flags, const float anim_speed)
{
	int			index = G2_Find_Bone_Rag(&ghoul2, blist, bone_name);
	int			mod_flags = flags;

	const int start_frame = arg_start_frame;
	const int end_frame = arg_end_frame;

	if (index != -1)
	{
		blist[index].blendFrame = blist[index].blendLerpFrame = 0;
		blist[index].blend_time = blist[index].blendStart = 0;
		mod_flags &= ~BONE_ANIM_BLEND;

		blist[index].end_frame = end_frame;
		blist[index].start_frame = start_frame;
		blist[index].animSpeed = anim_speed;
		blist[index].pauseTime = 0;
		//		blist[index].boneMap = NULL;
		//		blist[index].lastTime = blist[index].startTime = (current_time - (((setFrame - (float)start_frame) * 50.0)/ animSpeed));
		blist[index].flags &= ~(BONE_ANIM_TOTAL);
		blist[index].flags |= mod_flags;

		return qtrue;
	}

	index = G2_Add_Bone(ghoul2.animModel, blist, bone_name);

	if (index != -1)
	{
		blist[index].blendFrame = blist[index].blendLerpFrame = 0;
		blist[index].blend_time = 0;
		mod_flags &= ~BONE_ANIM_BLEND;
		blist[index].end_frame = end_frame;
		blist[index].start_frame = start_frame;
		blist[index].animSpeed = anim_speed;
		blist[index].pauseTime = 0;
		//		blist[index].boneMap = NULL;
		//		blist[index].lastTime = blist[index].startTime = (current_time - (((setFrame - (float)start_frame) * 50.0f)/ animSpeed));
		blist[index].flags &= ~(BONE_ANIM_TOTAL);
		blist[index].flags |= mod_flags;

		return qtrue;
	}

	assert(0);
	return qfalse;
}

void G2_ResetRagDoll(CGhoul2Info_v& ghoul2_v)
{
	int model;

	for (model = 0; model < ghoul2_v.size(); model++)
	{
		if (ghoul2_v[model].mModelindex != -1)
		{
			break;
		}
	}

	if (model == ghoul2_v.size())
	{
		return;
	}

	CGhoul2Info& ghoul2 = ghoul2_v[model];

	if (!(ghoul2.mFlags & GHOUL2_RAG_STARTED))
	{ //no use in doing anything if we aren't ragging
		return;
	}

	boneInfo_v& blist = ghoul2.mBlist;
#if 1
	//Eh, screw it. Ragdoll does a lot of terrible things to the bones that probably aren't directly reversible, so just reset it all.
	G2_Init_Bone_List(blist, ghoul2.aHeader->numBones);
#else //The anims on every bone are messed up too, as are the angles. There's not really any way to get back to a normal state, so just clear the list
	//and let them re-set their anims/angles gameside.
	int i = 0;
	while (i < blist.size())
	{
		boneInfo_t& bone = blist[i];
		if (bone.boneNumber != -1 && (bone.flags & BONE_ANGLES_RAGDOLL))
		{
			bone.flags &= ~BONE_ANGLES_RAGDOLL;
			bone.flags &= ~BONE_ANGLES_IK;
			bone.RagFlags = 0;
			bone.lastTimeUpdated = 0;
			VectorClear(bone.currentAngles);
			bone.ragStartTime = 0;
		}
		i++;
	}
#endif
	ghoul2.mFlags &= ~(GHOUL2_RAG_PENDING | GHOUL2_RAG_DONE | GHOUL2_RAG_STARTED);
}

void G2_SetRagDoll(CGhoul2Info_v& ghoul2_v, CRagDollParams* parms)
{
	if (parms)
	{
		parms->CallRagDollBegin = qfalse;
	}
	if (!broadsword || !broadsword->integer || !parms)
	{
		return;
	}
	int model;
	for (model = 0; model < ghoul2_v.size(); model++)
	{
		if (ghoul2_v[model].mModelindex != -1)
		{
			break;
		}
	}
	if (model == ghoul2_v.size())
	{
		return;
	}
	CGhoul2Info& ghoul2 = ghoul2_v[model];
	const mdxaHeader_t* mod_a = G2_GetModA(ghoul2);
	if (!mod_a)
	{
		return;
	}
	const int cur_time = G2API_GetTime(0);
	boneInfo_v& blist = ghoul2.mBlist;
	int	index = G2_Find_Bone_Rag(&ghoul2, blist, "model_root");
	switch (parms->RagPhase)
	{
	case CRagDollParams::RP_START_DEATH_ANIM:
		ghoul2.mFlags |= GHOUL2_RAG_PENDING;
		return;  /// not doing anything with this yet
	case CRagDollParams::RP_END_DEATH_ANIM:
		ghoul2.mFlags |= GHOUL2_RAG_PENDING | GHOUL2_RAG_DONE;
		if (broadsword_waitforshot &&
			broadsword_waitforshot->integer)
		{
			if (broadsword_waitforshot->integer == 2)
			{
				if (!(ghoul2.mFlags & (GHOUL2_RAG_COLLISION_DURING_DEATH | GHOUL2_RAG_COLLISION_SLIDE)))
				{
					//nothing was encountered, lets just wait for the first shot
					return; // we ain't starting yet
				}
			}
			else
			{
				return; // we ain't starting yet
			}
		}
		break;
	case CRagDollParams::RP_DEATH_COLLISION:
		if (parms->collisionType)
		{
			ghoul2.mFlags |= GHOUL2_RAG_COLLISION_SLIDE;
		}
		else
		{
			ghoul2.mFlags |= GHOUL2_RAG_COLLISION_DURING_DEATH;
		}
		if (broadsword_dontstopanim && broadsword_waitforshot &&
			(broadsword_dontstopanim->integer || broadsword_waitforshot->integer)
			)
		{
			if (!(ghoul2.mFlags & GHOUL2_RAG_DONE))
			{
				return; // we ain't starting yet
			}
		}
		break;
	case CRagDollParams::RP_CORPSE_SHOT:
		if (broadsword_kickorigin &&
			broadsword_kickorigin->integer)
		{
			if (index >= 0 && index < static_cast<int>(blist.size()))
			{
				boneInfo_t& bone = blist[index];
				if (bone.boneNumber >= 0)
				{
					if (bone.flags & BONE_ANGLES_RAGDOLL)
					{
						//rww - Would need ent pointer here. But.. since this is SW, we aren't even having corpse shooting anyway I'd imagine.
						/*
						float magicFactor14=8.0f; //64.0f; // kick strength

						if (parms->fShotStrength)
						{ //if there is a shot strength, use it instead
							magicFactor14 = parms->fShotStrength;
						}

						parms->me->s.pos.trType = TR_GRAVITY;
						parms->me->s.pos.trDelta[0] += bone.lastShotDir[0]*magicFactor14;
						parms->me->s.pos.trDelta[1] += bone.lastShotDir[1]*magicFactor14;
						//parms->me->s.pos.trDelta[2] = fabsf(bone.lastShotDir[2])*magicFactor14;
						//rww - The vertical portion of this doesn't seem to work very well
						//I am just leaving it whatever it is for now, because my velocity scaling
						//only works on x and y and the gravity stuff for NPCs is a bit unpleasent
						//trying to change/work with
						assert( !Q_isnan(bone.lastShotDir[1]));
						*/
					}
				}
			}
		}
		break;
	case CRagDollParams::RP_GET_PELVIS_OFFSET:
		if (parms->RagPhase == CRagDollParams::RP_GET_PELVIS_OFFSET)
		{
			VectorClear(parms->pelvisAnglesOffset);
			VectorClear(parms->pelvisPositionOffset);
		}
		// intentional lack of a break
	case CRagDollParams::RP_SET_PELVIS_OFFSET:
		if (index >= 0 && index < static_cast<int>(blist.size()))
		{
			boneInfo_t& bone = blist[index];
			if (bone.boneNumber >= 0)
			{
				if (bone.flags & BONE_ANGLES_RAGDOLL)
				{
					if (parms->RagPhase == CRagDollParams::RP_GET_PELVIS_OFFSET)
					{
						VectorCopy(bone.anglesOffset, parms->pelvisAnglesOffset);
						VectorCopy(bone.positionOffset, parms->pelvisPositionOffset);
					}
					else
					{
						VectorCopy(parms->pelvisAnglesOffset, bone.anglesOffset);
						VectorCopy(parms->pelvisPositionOffset, bone.positionOffset);
					}
				}
			}
		}
		return;
	case CRagDollParams::RP_DISABLE_EFFECTORS:
		// not doing anything with this yet
		return;
	default:
		assert(0);
		return;
	}
	if (ghoul2.mFlags & GHOUL2_RAG_STARTED)
	{
		// only going to begin ragdoll once, everything else depends on what happens to the origin
		return;
	}
#if 0
	if (index >= 0)
	{
		Com_OPrintf("death %d %d\n", blist[index].start_frame, blist[index].end_frame);
	}
#endif

	ghoul2.mFlags |= GHOUL2_RAG_PENDING | GHOUL2_RAG_DONE | GHOUL2_RAG_STARTED;  // well anyway we are going live
	parms->CallRagDollBegin = qtrue;

	G2_GenerateWorldMatrix(parms->angles, parms->position);
	G2_ConstructGhoulSkeleton(ghoul2_v, cur_time, false, parms->scale);

	G2_Set_Bone_Rag(blist, "model_root", ghoul2, parms->scale, parms->position);
	G2_Set_Bone_Rag(blist, "pelvis", ghoul2, parms->scale, parms->position);

	G2_Set_Bone_Rag(blist, "lower_lumbar", ghoul2, parms->scale, parms->position);
	G2_Set_Bone_Rag(blist, "upper_lumbar", ghoul2, parms->scale, parms->position);
	G2_Set_Bone_Rag(blist, "thoracic", ghoul2, parms->scale, parms->position);
	G2_Set_Bone_Rag(blist, "cranium", ghoul2, parms->scale, parms->position);
	G2_Set_Bone_Rag(blist, "rhumerus", ghoul2, parms->scale, parms->position);
	G2_Set_Bone_Rag(blist, "lhumerus", ghoul2, parms->scale, parms->position);
	G2_Set_Bone_Rag(blist, "rradius", ghoul2, parms->scale, parms->position);
	G2_Set_Bone_Rag(blist, "lradius", ghoul2, parms->scale, parms->position);
	G2_Set_Bone_Rag(blist, "rfemurYZ", ghoul2, parms->scale, parms->position);
	G2_Set_Bone_Rag(blist, "lfemurYZ", ghoul2, parms->scale, parms->position);
	G2_Set_Bone_Rag(blist, "rtibia", ghoul2, parms->scale, parms->position);
	G2_Set_Bone_Rag(blist, "ltibia", ghoul2, parms->scale, parms->position);
	G2_Set_Bone_Rag(blist, "rhand", ghoul2, parms->scale, parms->position);
	G2_Set_Bone_Rag(blist, "lhand", ghoul2, parms->scale, parms->position);
	//G2_Set_Bone_Rag(mod_a,blist,"rtarsal",ghoul2,parms->scale,parms->position);
	//G2_Set_Bone_Rag(mod_a,blist,"ltarsal",ghoul2,parms->scale,parms->position);
	G2_Set_Bone_Rag(blist, "rtalus", ghoul2, parms->scale, parms->position);
	G2_Set_Bone_Rag(blist, "ltalus", ghoul2, parms->scale, parms->position);
	G2_Set_Bone_Rag(blist, "rradiusX", ghoul2, parms->scale, parms->position);
	G2_Set_Bone_Rag(blist, "lradiusX", ghoul2, parms->scale, parms->position);
	G2_Set_Bone_Rag(blist, "rfemurX", ghoul2, parms->scale, parms->position);
	G2_Set_Bone_Rag(blist, "lfemurX", ghoul2, parms->scale, parms->position);
	G2_Set_Bone_Rag(blist, "ceyebrow", ghoul2, parms->scale, parms->position);

	//int start_frame = 3665, end_frame = 3665+1;
	int start_frame = parms->start_frame, end_frame = parms->end_frame;

	G2_Set_Bone_Anim_No_BS(ghoul2, blist, "upper_lumbar", start_frame, end_frame - 1, BONE_ANIM_OVERRIDE_FREEZE | BONE_ANIM_BLEND,
		1.0f);
	G2_Set_Bone_Anim_No_BS(ghoul2, blist, "lower_lumbar", start_frame, end_frame - 1, BONE_ANIM_OVERRIDE_FREEZE | BONE_ANIM_BLEND,
		1.0f);
	G2_Set_Bone_Anim_No_BS(ghoul2, blist, "Motion", start_frame, end_frame - 1, BONE_ANIM_OVERRIDE_FREEZE | BONE_ANIM_BLEND,
		1.0f);
	//	G2_Set_Bone_Anim_No_BS(ghoul2, mod_a,blist,"model_root",start_frame,end_frame-1,
	//		BONE_ANIM_OVERRIDE_FREEZE|BONE_ANIM_BLEND,
	//		1.0f,cur_time,float(start_frame),150,0,true);
	G2_Set_Bone_Anim_No_BS(ghoul2, blist, "lfemurYZ", start_frame, end_frame - 1, BONE_ANIM_OVERRIDE_FREEZE | BONE_ANIM_BLEND,
		1.0f);
	G2_Set_Bone_Anim_No_BS(ghoul2, blist, "rfemurYZ", start_frame, end_frame - 1, BONE_ANIM_OVERRIDE_FREEZE | BONE_ANIM_BLEND,
		1.0f);

	G2_Set_Bone_Anim_No_BS(ghoul2, blist, "rhumerus", start_frame, end_frame - 1, BONE_ANIM_OVERRIDE_FREEZE | BONE_ANIM_BLEND,
		1.0f);
	G2_Set_Bone_Anim_No_BS(ghoul2, blist, "lhumerus", start_frame, end_frame - 1, BONE_ANIM_OVERRIDE_FREEZE | BONE_ANIM_BLEND,
		1.0f);

	//    should already be set					G2_GenerateWorldMatrix(parms->angles, parms->position);
	G2_ConstructGhoulSkeleton(ghoul2_v, cur_time, false, parms->scale);

	static constexpr float f_rad_scale = 0.3f;//0.5f;

	vec3_t pcj_min, pcj_max;
	VectorSet(pcj_min, -90.0f, -45.0f, -45.0f);
	VectorSet(pcj_max, 90.0f, 45.0f, 45.0f);
	G2_Set_Bone_Angles_Rag(ghoul2, blist, "model_root", RAG_PCJ_MODEL_ROOT | RAG_PCJ | RAG_UNSNAPPABLE, 10.0f * f_rad_scale, pcj_min, pcj_max, 100);
	VectorSet(pcj_min, -45.0f, -45.0f, -45.0f);
	VectorSet(pcj_max, 45.0f, 45.0f, 45.0f);
	G2_Set_Bone_Angles_Rag(ghoul2, blist, "pelvis", RAG_PCJ_PELVIS | RAG_PCJ | RAG_PCJ_POST_MULT | RAG_UNSNAPPABLE, 10.0f * f_rad_scale, pcj_min, pcj_max, 100);

#if 1
	// new base anim, unconscious flop
	int pcjflags = RAG_PCJ | RAG_PCJ_POST_MULT;//|RAG_EFFECTOR;

	VectorSet(pcj_min, -15.0f, -15.0f, -15.0f);
	VectorSet(pcj_max, 15.0f, 15.0f, 15.0f);
	G2_Set_Bone_Angles_Rag(ghoul2, blist, "lower_lumbar", pcjflags | RAG_UNSNAPPABLE, 10.0f * f_rad_scale, pcj_min, pcj_max, 500);
	G2_Set_Bone_Angles_Rag(ghoul2, blist, "upper_lumbar", pcjflags | RAG_UNSNAPPABLE, 10.0f * f_rad_scale, pcj_min, pcj_max, 500);
	VectorSet(pcj_min, -25.0f, -25.0f, -25.0f);
	VectorSet(pcj_max, 25.0f, 25.0f, 25.0f);
	G2_Set_Bone_Angles_Rag(ghoul2, blist, "thoracic", pcjflags | RAG_EFFECTOR | RAG_UNSNAPPABLE, 12.0f * f_rad_scale, pcj_min, pcj_max, 500);

	VectorSet(pcj_min, -10.0f, -10.0f, -90.0f);
	VectorSet(pcj_max, 10.0f, 10.0f, 90.0f);
	G2_Set_Bone_Angles_Rag(ghoul2, blist, "cranium", pcjflags | RAG_BONE_LIGHTWEIGHT | RAG_UNSNAPPABLE, 6.0f * f_rad_scale, pcj_min, pcj_max, 500);

	static constexpr float s_fact_leg = 1.0f;
	static constexpr float s_fact_arm = 1.0f;
	static constexpr float s_rad_arm = 1.0f;
	static constexpr float s_rad_leg = 1.0f;

	VectorSet(pcj_min, -100.0f, -40.0f, -15.0f);
	VectorSet(pcj_max, -15.0f, 80.0f, 15.0f);
	VectorScale(pcj_min, s_fact_arm, pcj_min);
	VectorScale(pcj_max, s_fact_arm, pcj_max);
	G2_Set_Bone_Angles_Rag(ghoul2, blist, "rhumerus", pcjflags | RAG_BONE_LIGHTWEIGHT | RAG_UNSNAPPABLE, 4.0f * s_rad_arm * f_rad_scale, pcj_min, pcj_max, 500);
	VectorSet(pcj_min, -50.0f, -80.0f, -15.0f);
	VectorSet(pcj_max, 15.0f, 40.0f, 15.0f);
	VectorScale(pcj_min, s_fact_arm, pcj_min);
	VectorScale(pcj_max, s_fact_arm, pcj_max);
	G2_Set_Bone_Angles_Rag(ghoul2, blist, "lhumerus", pcjflags | RAG_BONE_LIGHTWEIGHT | RAG_UNSNAPPABLE, 4.0f * s_rad_arm * f_rad_scale, pcj_min, pcj_max, 500);

	VectorSet(pcj_min, -25.0f, -20.0f, -20.0f);
	VectorSet(pcj_max, 90.0f, 20.0f, -20.0f);
	VectorScale(pcj_min, s_fact_arm, pcj_min);
	VectorScale(pcj_max, s_fact_arm, pcj_max);
	G2_Set_Bone_Angles_Rag(ghoul2, blist, "rradius", pcjflags | RAG_BONE_LIGHTWEIGHT, 3.0f * s_rad_arm * f_rad_scale, pcj_min, pcj_max, 500);
	VectorSet(pcj_min, -90.0f, -20.0f, -20.0f);
	VectorSet(pcj_max, 30.0f, 20.0f, -20.0f);
	VectorScale(pcj_min, s_fact_arm, pcj_min);
	VectorScale(pcj_max, s_fact_arm, pcj_max);
	G2_Set_Bone_Angles_Rag(ghoul2, blist, "lradius", pcjflags | RAG_BONE_LIGHTWEIGHT, 3.0f * s_rad_arm * f_rad_scale, pcj_min, pcj_max, 500);

	VectorSet(pcj_min, -80.0f, -50.0f, -20.0f);
	VectorSet(pcj_max, 30.0f, 5.0f, 20.0f);
	VectorScale(pcj_min, s_fact_leg, pcj_min);
	VectorScale(pcj_max, s_fact_leg, pcj_max);
	G2_Set_Bone_Angles_Rag(ghoul2, blist, "rfemurYZ", pcjflags | RAG_BONE_LIGHTWEIGHT, 6.0f * s_rad_leg * f_rad_scale, pcj_min, pcj_max, 500);
	VectorSet(pcj_min, -60.0f, -5.0f, -20.0f);
	VectorSet(pcj_max, 50.0f, 50.0f, 20.0f);
	VectorScale(pcj_min, s_fact_leg, pcj_min);
	VectorScale(pcj_max, s_fact_leg, pcj_max);
	G2_Set_Bone_Angles_Rag(ghoul2, blist, "lfemurYZ", pcjflags | RAG_BONE_LIGHTWEIGHT, 6.0f * s_rad_leg * f_rad_scale, pcj_min, pcj_max, 500);

	VectorSet(pcj_min, -20.0f, -15.0f, -15.0f);
	VectorSet(pcj_max, 100.0f, 15.0f, 15.0f);
	VectorScale(pcj_min, s_fact_leg, pcj_min);
	VectorScale(pcj_max, s_fact_leg, pcj_max);
	G2_Set_Bone_Angles_Rag(ghoul2, blist, "rtibia", pcjflags | RAG_EFFECTOR | RAG_BONE_LIGHTWEIGHT, 4.0f * s_rad_leg * f_rad_scale, pcj_min, pcj_max, 500);
	VectorSet(pcj_min, 20.0f, -15.0f, -15.0f);
	VectorSet(pcj_max, 100.0f, 15.0f, 15.0f);
	VectorScale(pcj_min, s_fact_leg, pcj_min);
	VectorScale(pcj_max, s_fact_leg, pcj_max);
	G2_Set_Bone_Angles_Rag(ghoul2, blist, "ltibia", pcjflags | RAG_EFFECTOR | RAG_BONE_LIGHTWEIGHT, 4.0f * s_rad_leg * f_rad_scale, pcj_min, pcj_max, 500);
#else
	// old base anim
	int pcjflags = RAG_PCJ | RAG_PCJ_POST_MULT | RAG_EFFECTOR;

	VectorSet(pcjMin, -15.0f, -15.0f, -15.0f);
	VectorSet(pcjMax, 45.0f, 15.0f, 15.0f);
	G2_Set_Bone_Angles_Rag(ghoul2, mod_a, blist, "lower_lumbar", pcjflags, 10.0f, pcjMin, pcjMax, 500);
	G2_Set_Bone_Angles_Rag(ghoul2, mod_a, blist, "upper_lumbar", pcjflags, 10.0f, pcjMin, pcjMax, 500);
	VectorSet(pcjMin, -45.0f, -45.0f, -45.0f);
	VectorSet(pcjMax, 45.0f, 45.0f, 45.0f);
	G2_Set_Bone_Angles_Rag(ghoul2, mod_a, blist, "thoracic", pcjflags, 10.0f, pcjMin, pcjMax, 500);

	VectorSet(pcjMin, -10.0f, -10.0f, -90.0f);
	VectorSet(pcjMax, 10.0f, 10.0f, 90.0f);
	G2_Set_Bone_Angles_Rag(ghoul2, mod_a, blist, "cranium", pcjflags | RAG_BONE_LIGHTWEIGHT, 6.0f, pcjMin, pcjMax, 500);

	//VectorSet(pcjMin,-45.0f,-90.0f,-100.0f);
	VectorSet(pcjMin, -180.0f, -180.0f, -100.0f);
	//VectorSet(pcjMax,60.0f,60.0f,45.0f);
	VectorSet(pcjMax, 180.0f, 180.0f, 45.0f);
	G2_Set_Bone_Angles_Rag(ghoul2, mod_a, blist, "rhumerus", pcjflags | RAG_BONE_LIGHTWEIGHT, 4.0f, pcjMin, pcjMax, 500);
	//VectorSet(pcjMin,-45.0f,-60.0f,-45.0f);
	VectorSet(pcjMin, -180.0f, -180.0f, -100.0f);
	//VectorSet(pcjMax,60.0f,90.0f,100.0f);
	VectorSet(pcjMax, 180.0f, 180.0f, 100.0f);
	G2_Set_Bone_Angles_Rag(ghoul2, mod_a, blist, "lhumerus", pcjflags | RAG_BONE_LIGHTWEIGHT, 4.0f, pcjMin, pcjMax, 500);

	//-120/120
	VectorSet(pcjMin, -120.0f, -20.0f, -20.0f);
	VectorSet(pcjMax, 50.0f, 20.0f, -20.0f);
	G2_Set_Bone_Angles_Rag(ghoul2, mod_a, blist, "rradius", pcjflags | RAG_BONE_LIGHTWEIGHT, 3.0f, pcjMin, pcjMax, 500);
	VectorSet(pcjMin, -120.0f, -20.0f, -20.0f);
	VectorSet(pcjMax, 5.0f, 20.0f, -20.0f);
	G2_Set_Bone_Angles_Rag(ghoul2, mod_a, blist, "lradius", pcjflags | RAG_BONE_LIGHTWEIGHT, 3.0f, pcjMin, pcjMax, 500);

	VectorSet(pcjMin, -90.0f, -50.0f, -20.0f);
	VectorSet(pcjMax, 50.0f, 20.0f, 20.0f);
	G2_Set_Bone_Angles_Rag(ghoul2, mod_a, blist, "rfemurYZ", pcjflags | RAG_BONE_LIGHTWEIGHT, 6.0f, pcjMin, pcjMax, 500);
	VectorSet(pcjMin, -90.0f, -20.0f, -20.0f);
	VectorSet(pcjMax, 50.0f, 50.0f, 20.0f);
	G2_Set_Bone_Angles_Rag(ghoul2, mod_a, blist, "lfemurYZ", pcjflags | RAG_BONE_LIGHTWEIGHT, 6.0f, pcjMin, pcjMax, 500);

	//120
	VectorSet(pcjMin, -20.0f, -15.0f, -15.0f);
	VectorSet(pcjMax, 120.0f, 15.0f, 15.0f);
	G2_Set_Bone_Angles_Rag(ghoul2, mod_a, blist, "rtibia", pcjflags | RAG_EFFECTOR | RAG_BONE_LIGHTWEIGHT, 4.0f, pcjMin, pcjMax, 500);
	VectorSet(pcjMin, 20.0f, -15.0f, -15.0f);
	VectorSet(pcjMax, 120.0f, 15.0f, 15.0f);
	G2_Set_Bone_Angles_Rag(ghoul2, mod_a, blist, "ltibia", pcjflags | RAG_EFFECTOR | RAG_BONE_LIGHTWEIGHT, 4.0f, pcjMin, pcjMax, 500);
#endif

	constexpr float s_rad_e_arm = 1.2f;
	constexpr float s_rad_e_leg = 1.2f;

	//	int rhand=
	G2_Set_Bone_Angles_Rag(ghoul2, blist, "rhand", RAG_EFFECTOR | RAG_BONE_LIGHTWEIGHT, 6.0f * s_rad_e_arm * f_rad_scale);
	G2_Set_Bone_Angles_Rag(ghoul2, blist, "lhand", RAG_EFFECTOR | RAG_BONE_LIGHTWEIGHT, 6.0f * s_rad_e_arm * f_rad_scale);
	G2_Set_Bone_Angles_Rag(ghoul2, blist, "rtalus", RAG_EFFECTOR | RAG_BONE_LIGHTWEIGHT, 4.0f * s_rad_e_leg * f_rad_scale);
	G2_Set_Bone_Angles_Rag(ghoul2, blist, "ltalus", RAG_EFFECTOR | RAG_BONE_LIGHTWEIGHT, 4.0f * s_rad_e_leg * f_rad_scale);
	G2_Set_Bone_Angles_Rag(ghoul2, blist, "rradiusX", RAG_EFFECTOR | RAG_BONE_LIGHTWEIGHT, 6.0f * s_rad_e_arm * f_rad_scale);
	G2_Set_Bone_Angles_Rag(ghoul2, blist, "lradiusX", RAG_EFFECTOR | RAG_BONE_LIGHTWEIGHT, 6.0f * s_rad_e_arm * f_rad_scale);
	G2_Set_Bone_Angles_Rag(ghoul2, blist, "rfemurX", RAG_EFFECTOR | RAG_BONE_LIGHTWEIGHT, 10.0f * s_rad_e_leg * f_rad_scale);
	G2_Set_Bone_Angles_Rag(ghoul2, blist, "lfemurX", RAG_EFFECTOR | RAG_BONE_LIGHTWEIGHT, 10.0f * s_rad_e_leg * f_rad_scale);
	G2_Set_Bone_Angles_Rag(ghoul2, blist, "ceyebrow", RAG_EFFECTOR | RAG_BONE_LIGHTWEIGHT, 5.0f);
	//match the currrent animation
	if (!G2_RagDollSetup(ghoul2, cur_time, true, parms->position, false))
	{
		assert(!"failed to add any rag bones");
		return;
	}
	G2_RagDollCurrentPosition(ghoul2_v, model, cur_time, parms->angles, parms->position, parms->scale);
#if 0
	if (rhand > 0)
	{
		boneInfo_t& bone = blist[rhand];
		SRagEffector& e = ragEffectors[bone.ragIndex];
		VectorCopy(bone.originalOrigin, handPos);
		VectorCopy(e.currentOrigin, handPos2);
	}
#endif

	CRagDollUpdateParams fparms;
	VectorCopy(parms->position, fparms.position);
	VectorCopy(parms->angles, fparms.angles);
	VectorCopy(parms->scale, fparms.scale);
	VectorClear(fparms.velocity);
	fparms.me = parms->me;
	fparms.settleFrame = parms->end_frame;

	//Guess I don't need to do this, do I?
	G2_ConstructGhoulSkeleton(ghoul2_v, cur_time, false, parms->scale);

	vec3_t d_pos;
	VectorCopy(parms->position, d_pos);
#ifdef _OLD_STYLE_SETTLE
	dPos[2] -= 6;
#endif

	for (int k = 0; k </*10*/20; k++)
	{
		G2_RagDollSettlePositionNumeroTrois(ghoul2_v, &fparms, cur_time);

		G2_RagDollCurrentPosition(ghoul2_v, model, cur_time, parms->angles, d_pos, parms->scale);
		G2_RagDollMatchPosition();
		G2_RagDollSolve(ghoul2_v, model, 1.0f * (1.0f - k / 40.0f), false);
	}
}

void G2_SetRagDollBullet(CGhoul2Info& ghoul2, const vec3_t ray_start, const vec3_t hit)
{
	if (!broadsword || !broadsword->integer)
	{
		return;
	}
	vec3_t shot_dir;
	VectorSubtract(hit, ray_start, shot_dir);
	float len = VectorLength(shot_dir);
	if (len < 1.0f)
	{
		return;
	}
	float lenr = 1.0f / len;
	shot_dir[0] *= lenr;
	shot_dir[1] *= lenr;
	shot_dir[2] *= lenr;

	if (broadsword_kickbones && broadsword_kickbones->integer)
	{
		bool firstOne = false;
		boneInfo_v& blist = ghoul2.mBlist;
		for (int i = static_cast<int>(blist.size() - 1); i >= 0; i--)
		{
			boneInfo_t& bone = blist[i];
			if (bone.flags & BONE_ANGLES_TOTAL)
			{
				if (bone.flags & BONE_ANGLES_RAGDOLL)
				{
					constexpr int magic_factor13 = 150.0f;
					if (!firstOne)
					{
						firstOne = true;
#if 0
						int cur_time = G2API_GetTime(0);
						const mdxaHeader_t* mod_a = G2_GetModA(ghoul2);
						int start_frame = 0, end_frame = 0;
#if 1
						TheGhoul2Wraith()->GetAnimFrames(ghoul2.mID, "unconsciousdeadflop01", start_frame, end_frame);
						if (start_frame == -1 && end_frame == -1)
						{ //A bad thing happened! Just use the hardcoded numbers even though they could be wrong.
							start_frame = 3573;
							end_frame = 3583;
							assert(0);
						}
						G2_Set_Bone_Anim_No_BS(mod_a, blist, "upper_lumbar", start_frame, end_frame - 1,
							BONE_ANIM_OVERRIDE_FREEZE | BONE_ANIM_BLEND,
							1.0f, cur_time, float(start_frame), 75, 0, true);
						G2_Set_Bone_Anim_No_BS(mod_a, blist, "lfemurYZ", start_frame, end_frame - 1,
							BONE_ANIM_OVERRIDE_FREEZE | BONE_ANIM_BLEND,
							1.0f, cur_time, float(start_frame), 75, 0, true);
						G2_Set_Bone_Anim_No_BS(mod_a, blist, "rfemurYZ", start_frame, end_frame - 1,
							BONE_ANIM_OVERRIDE_FREEZE | BONE_ANIM_BLEND,
							1.0f, cur_time, float(start_frame), 75, 0, true);
#else
						TheGhoul2Wraith()->GetAnimFrames(ghoul2.mID, "backdeadflop01", start_frame, end_frame);
						if (start_frame == -1 && end_frame == -1)
						{ //A bad thing happened! Just use the hardcoded numbers even though they could be wrong.
							start_frame = 3581;
							end_frame = 3592;
							assert(0);
						}
						G2_Set_Bone_Anim_No_BS(mod_a, blist, "upper_lumbar", end_frame, start_frame + 1,
							BONE_ANIM_OVERRIDE_FREEZE,
							-1.0f, cur_time, float(end_frame - 1), 50, 0, true);
						G2_Set_Bone_Anim_No_BS(mod_a, blist, "lfemurYZ", end_frame, start_frame + 1,
							BONE_ANIM_OVERRIDE_FREEZE,
							-1.0f, cur_time, float(end_frame - 1), 50, 0, true);
						G2_Set_Bone_Anim_No_BS(mod_a, blist, "rfemurYZ", end_frame, start_frame + 1,
							BONE_ANIM_OVERRIDE_FREEZE,
							-1.0f, cur_time, float(end_frame - 1), 50, 0, true);
#endif
#endif
					}

					VectorCopy(shot_dir, bone.lastShotDir);
					vec3_t dir;
					VectorSubtract(bone.lastPosition, hit, dir);
					len = VectorLength(dir);
					if (len < 1.0f)
					{
						len = 1.0f;
					}
					lenr = 1.0f / len;
					float effect = lenr;
					effect *= magic_factor13 * effect; // this is cubed, one of them is absorbed by the next calc
					bone.velocityEffector[0] = shot_dir[0] * (effect + flrand(0.0f, 0.05f));
					bone.velocityEffector[1] = shot_dir[1] * (effect + flrand(0.0f, 0.05f));
					bone.velocityEffector[2] = fabs(shot_dir[2]) * (effect + flrand(0.0f, 0.05f));
					assert(!Q_isnan(shot_dir[2]));

					// go dynamic
					bone.firstCollisionTime = G2API_GetTime(0);
					bone.restTime = 0;
				}
			}
		}
	}
}

static float G2_RagSetState(const CGhoul2Info& ghoul2, boneInfo_t& bone, const int frame_num, const vec3_t origin)
{
	ragOriginChange = DistanceSquared(origin, bone.extraVec1);
	VectorSubtract(origin, bone.extraVec1, ragOriginChangeDir);

	float decay = 1.0f;

	constexpr int dynamic_time = 1000;

	if (ghoul2.mFlags & GHOUL2_RAG_FORCESOLVE)
	{
		ragState = ERS_DYNAMIC;
		if (frame_num > bone.firstCollisionTime + dynamic_time)
		{
			VectorCopy(origin, bone.extraVec1);
			if (ragOriginChange > 15.0f)
			{ //if we moved, or if this bone is still in solid
				bone.firstCollisionTime = frame_num;
			}
			else
			{
				// settle out
				bone.firstCollisionTime = 0;
				bone.restTime = frame_num;
				ragState = ERS_SETTLING;
			}
		}
	}
	else if (bone.firstCollisionTime > 0)
	{
		ragState = ERS_DYNAMIC;
		if (frame_num > bone.firstCollisionTime + dynamic_time)
		{
			VectorCopy(origin, bone.extraVec1);
			if (ragOriginChange > 15.0f)
			{ //if we moved, or if this bone is still in solid
				bone.firstCollisionTime = frame_num;
			}
			else
			{
				// settle out
				bone.firstCollisionTime = 0;
				bone.restTime = frame_num;
				ragState = ERS_SETTLING;
			}
		}
		//decay=0.0f;
	}
	else if (bone.restTime > 0)
	{
		constexpr int settle_time = 1000;
		decay = 1.0f - (frame_num - bone.restTime) / static_cast<float>(dynamic_time);
		if (decay < 0.0f)
		{
			decay = 0.0f;
		}
		if (decay > 1.0f)
		{
			decay = 1.0f;
		}
		constexpr float	magic_factor8 = 1.0f; // Power for decay
		decay = pow(decay, magic_factor8);
		ragState = ERS_SETTLING;
		if (frame_num > bone.restTime + settle_time)
		{
			VectorCopy(origin, bone.extraVec1);
			if (ragOriginChange > 15.0f)
			{
				bone.restTime = frame_num;
			}
			else
			{
				// stop
				bone.restTime = 0;
				ragState = ERS_SETTLED;
			}
		}
		//decay=0.0f;
	}
	else
	{
		if (bone.RagFlags & RAG_PCJ_IK_CONTROLLED)
		{
			bone.firstCollisionTime = frame_num;
			ragState = ERS_DYNAMIC;
		}
		else if (ragOriginChange > 15.0f)
		{
			bone.firstCollisionTime = frame_num;
			ragState = ERS_DYNAMIC;
		}
		else
		{
			ragState = ERS_SETTLED;
		}
		decay = 0.0f;
	}
	//			ragState=ERS_SETTLED;
	//			decay=0.0f;
	return decay;
}

static bool G2_RagDollSetup(CGhoul2Info& ghoul2, const int frame_num, const bool reset_origin, const vec3_t origin, const bool any_rendered)
{
	int min_surviving_bone = 10000;
	//int minSurvivingBoneAt=-1;
	int min_surviving_bone_alt = 10000;
	//int minSurvivingBoneAtAlt=-1;

	assert(ghoul2.mFileName[0]);
	boneInfo_v& blist = ghoul2.mBlist;
	rag.clear();
	int num_rendered = 0;
	int num_not_rendered = 0;
	//int pelvisAt=-1;
	for (size_t i = 0; i < blist.size(); i++)
	{
		boneInfo_t& bone = blist[i];
		if (bone.boneNumber >= 0)
		{
			assert(bone.boneNumber < MAX_BONES_RAG);
			if (bone.flags & BONE_ANGLES_RAGDOLL || bone.flags & BONE_ANGLES_IK)
			{
				//rww - this was (!anyRendered) before. Isn't that wrong? (if anyRendered, then wasRendered should be true)
				const bool was_rendered =
					!any_rendered ||   // offscreeen or something
					G2_WasBoneRendered(ghoul2, bone.boneNumber);
				if (!was_rendered)
				{
					bone.RagFlags |= RAG_WAS_NOT_RENDERED;
					num_not_rendered++;
				}
				else
				{
					bone.RagFlags &= ~RAG_WAS_NOT_RENDERED;
					bone.RagFlags |= RAG_WAS_EVER_RENDERED;
					num_rendered++;
				}
				if (bone.RagFlags & RAG_PCJ_PELVIS)
				{
					//pelvisAt=i;
				}
				else if (bone.RagFlags & RAG_PCJ_MODEL_ROOT)
				{
				}
				else if (was_rendered && bone.RagFlags & RAG_PCJ)
				{
					if (min_surviving_bone > bone.boneNumber)
					{
						min_surviving_bone = bone.boneNumber;
						//minSurvivingBoneAt=i;
					}
				}
				else if (was_rendered)
				{
					if (min_surviving_bone_alt > bone.boneNumber)
					{
						min_surviving_bone_alt = bone.boneNumber;
						//minSurvivingBoneAtAlt=i;
					}
				}
				if (
					any_rendered &&
					bone.RagFlags & RAG_WAS_EVER_RENDERED &&
					!(bone.RagFlags & RAG_PCJ_MODEL_ROOT) &&
					!(bone.RagFlags & RAG_PCJ_PELVIS) &&
					!was_rendered &&
					bone.RagFlags & RAG_EFFECTOR
					)
				{
					// this thing was rendered in the past, but wasn't now, although other bones were, lets get rid of it
//					bone.flags &= ~BONE_ANGLES_RAGDOLL;
//					bone.RagFlags = 0;
//Com_OPrintf("Deleted Effector %d\n",i);
//					continue;
				}
				if (static_cast<int>(rag.size()) < bone.boneNumber + 1)
				{
					rag.resize(bone.boneNumber + 1, nullptr);
				}
				rag[bone.boneNumber] = &bone;
				ragBlistIndex[bone.boneNumber] = i;

				bone.lastTimeUpdated = frame_num;
				if (reset_origin)
				{
					VectorCopy(origin, bone.extraVec1); // this is only done incase a limb is removed
				}
			}
		}
	}
#if 0
	if (numRendered < 5)  // I think this is a limb
	{
		//Com_OPrintf("limb %3d/%3d  (r,N).\n",numRendered,numNotRendered);
		if (minSurvivingBoneAt < 0)
		{
			// pelvis is gone, but we have no remaining pcj's
			// just find any remain rag effector
			minSurvivingBoneAt = minSurvivingBoneAtAlt;
		}
		if (
			minSurvivingBoneAt >= 0 &&
			pelvisAt >= 0)
		{
			{
				// remove the pelvis as a rag
				boneInfo_t& bone = blist[minSurvivingBoneAt];
				bone.flags &= ~BONE_ANGLES_RAGDOLL;
				bone.RagFlags = 0;
			}
			{
				// the root-est bone is now our "pelvis
				boneInfo_t& bone = blist[minSurvivingBoneAt];
				VectorSet(bone.minAngles, -14500.0f, -14500.0f, -14500.0f);
				VectorSet(bone.maxAngles, 14500.0f, 14500.0f, 14500.0f);
				bone.RagFlags |= RAG_PCJ_PELVIS | RAG_PCJ; // this guy is our new "pelvis"
				bone.flags |= BONE_ANGLES_POSTMULT;
				bone.ragStartTime = G2API_GetTime(0);
			}
		}
	}
#endif
	numRags = 0;
	//int ragStartTime=0;
	for (const auto& i : rag)
	{
		if (i)
		{
			boneInfo_t& bone = *i;
			assert(bone.boneNumber >= 0);
			assert(numRags < MAX_BONES_RAG);

			//ragStartTime=bone.ragStartTime;

			bone.ragIndex = numRags;
			ragBoneData[numRags] = &bone;
			ragEffectors[numRags].radius = bone.radius;
			ragEffectors[numRags].weight = bone.weight;
			G2_GetBoneBasepose(ghoul2, bone.boneNumber, bone.basepose, bone.baseposeInv);
			numRags++;
		}
	}
	if (!numRags)
	{
		return false;
	}
	return true;
}

static void G2_RagDoll(CGhoul2Info_v& ghoul2_v, const int g2_index, CRagDollUpdateParams* params, const int cur_time)
{
	if (!broadsword || !broadsword->integer)
	{
		return;
	}

	if (!params)
	{
		assert(0);
		return;
	}

	vec3_t d_pos;
	VectorCopy(params->position, d_pos);
#ifdef _OLD_STYLE_SETTLE
	dPos[2] -= 6;
#endif

	//	params->DebugLine(handPos,handPos2,false);
	const int frame_num = G2API_GetTime(0);
	CGhoul2Info& ghoul2 = ghoul2_v[g2_index];
	assert(ghoul2.mFileName[0]);
	boneInfo_v& blist = ghoul2.mBlist;

	// hack for freezing ragdoll (no idea if it works)
#if 0
	if (0)
	{
		// we gotta hack this to basically freeze the timers
		for (i = 0; i < blist.size(); i++)
		{
			boneInfo_t& bone = blist[i];
			if (bone.boneNumber >= 0)
			{
				assert(bone.boneNumber < MAX_BONES_RAG);
				if (bone.flags & BONE_ANGLES_RAGDOLL)
				{
					bone.ragStartTime += 50;
					if (bone.firstTime)
					{
						bone.firstTime += 50;
					}
					if (bone.firstCollisionTime)
					{
						bone.firstCollisionTime += 50;
					}
					if (bone.restTime)
					{
						bone.restTime += 50;
					}
				}
			}
		}
		return;
	}
#endif

	float decay = 1.0f;
	bool any_rendered = false;

	// this loop checks for settled
	for (boneInfo_t& bone : blist)
	{
		if (bone.boneNumber >= 0)
		{
			assert(bone.boneNumber < MAX_BONES_RAG);
			if (bone.flags & BONE_ANGLES_RAGDOLL)
			{
				// check for state transitions
				decay = G2_RagSetState(ghoul2, bone, frame_num, d_pos); // set the current state

				if (ragState == ERS_SETTLED)
				{
#if 0
					bool noneInSolid = true;

					//first iterate through and make sure no bones are still in solid a lot
					for (int j = 0; j < blist.size(); j++)
					{
						boneInfo_t& bone2 = blist[j];

						if (bone2.boneNumber >= 0 && bone2.solidCount > 8)
						{
							noneInSolid = false;
							break;
						}
					}

					if (noneInSolid)
					{ //we're settled then
						params->RagDollSettled();
						return;
					}
					else
					{
						continue;
					}
#else
					params->RagDollSettled();
					return;
#endif
				}
				if (G2_WasBoneRendered(ghoul2, bone.boneNumber))
				{
					any_rendered = true;
					break;
				}
			}
		}
	}
	//int iters=(ragState==ERS_DYNAMIC)?2:1;
	int iters = ragState == ERS_DYNAMIC ? 4 : 2;
	//bool kicked=false;
	if (ragOriginChangeDir[2] < -100.0f)
	{
		//kicked=true;
		//iters*=8;
		iters *= 2; //rww - changed to this.. it was getting up to around 600 traces at times before (which is insane)
	}
	if (iters)
	{
		constexpr bool reset_origin = false;
		if (!G2_RagDollSetup(ghoul2, frame_num, reset_origin, d_pos, any_rendered))
		{
			return;
		}
		// ok, now our data structures are compact and set up in topological order

		for (int i = 0; i < iters; i++)
		{
			G2_RagDollCurrentPosition(ghoul2_v, g2_index, frame_num, params->angles, d_pos, params->scale);

			if (G2_RagDollSettlePositionNumeroTrois(ghoul2_v, params, cur_time))
			{
#if 0
				//effectors are start solid alot, so this was pretty extreme
				if (!kicked && iters < 4)
				{
					kicked = true;
					//iters*=4;
					iters *= 2;
				}
#endif
			}
			//params->position[2] += 16;
			G2_RagDollSolve(ghoul2_v, g2_index, decay * 2.0f, true, params);
		}
	}

	if (params->me != ENTITYNUM_NONE)
	{
#if 0
		vec3_t worldMins, worldMaxs;
		worldMins[0] = params->position[0] - 17;
		worldMins[1] = params->position[1] - 17;
		worldMins[2] = params->position[2];
		worldMaxs[0] = params->position[0] + 17;
		worldMaxs[1] = params->position[1] + 17;
		worldMaxs[2] = params->position[2];
		//Com_OPrintf(va("%f \n",worldMins[2]);
		//		params->DebugLine(worldMins,worldMaxs,true);
#endif
		G2_RagDollCurrentPosition(ghoul2_v, g2_index, frame_num, params->angles, params->position, params->scale);
		//		SV_UnlinkEntity(params->me);
		//		params->me->SetMins(BB_SHOOTING_SIZE,ragBoneMins);
		//		params->me->SetMaxs(BB_SHOOTING_SIZE,ragBoneMaxs);
		//		SV_LinkEntity(params->me);
	}
}

#ifdef _DEBUG
#define _DEBUG_BONE_NAMES
#endif

static char* G2_Get_Bone_Name(const CGhoul2Info* ghl_info, const boneInfo_v& blist, const int bone_num)
{
	const mdxaSkelOffsets_t* offsets = reinterpret_cast<mdxaSkelOffsets_t*>((byte*)ghl_info->aHeader + sizeof(mdxaHeader_t));

	// look through entire list
	for (const auto& i : blist)
	{
		// if this bone entry has no info in it, bounce over it
		if (i.boneNumber != bone_num)
		{
			continue;
		}

		// figure out what skeletal info structure this bone entry is looking at
		const auto skel = reinterpret_cast<mdxaSkel_t*>((byte*)ghl_info->aHeader + sizeof(mdxaHeader_t) + offsets->offsets[i.boneNumber]);

		return skel->name;
	}

	// didn't find it
	return "BONE_NOT_FOUND";
}

char* G2_GetBoneNameFromSkel(const CGhoul2Info& ghoul2, int bone_num);

static void G2_RagDollCurrentPosition(CGhoul2Info_v& ghoul2_v, const int g2_index, const int frame_num, const vec3_t angles, const vec3_t position, const vec3_t scale)
{
	const CGhoul2Info& ghoul2 = ghoul2_v[g2_index];
	assert(ghoul2.mFileName[0]);
	//Com_OPrintf("angles %f %f %f\n",angles[0],angles[1],angles[2]);
	G2_GenerateWorldMatrix(angles, position);
	G2_ConstructGhoulSkeleton(ghoul2_v, frame_num, false, scale);

	float total_wt = 0.0f;
	for (int i = 0; i < numRags; i++)
	{
		const boneInfo_t& bone = *ragBoneData[i];
		G2_GetBoneMatrixLow(ghoul2, bone.boneNumber, scale, ragBones[i], ragBasepose[i], ragBaseposeInv[i]);

#ifdef _DEBUG_BONE_NAMES
		const char* debugBoneName = G2_Get_Bone_Name(&ghoul2, ghoul2.mBlist, bone.boneNumber);
		assert(debugBoneName);
#endif

		//float cmweight=ragEffectors[numRags].weight;
		constexpr float cmweight = 1.0f;
		for (int k = 0; k < 3; k++)
		{
			ragEffectors[i].currentOrigin[k] = ragBones[i].matrix[k][3];
			assert(!Q_isnan(ragEffectors[i].currentOrigin[k]));
			if (!i)
			{
				// set mins, maxs and cm
				ragBoneCM[k] = ragEffectors[i].currentOrigin[k] * cmweight;
				ragBoneMaxs[k] = ragEffectors[i].currentOrigin[k];
				ragBoneMins[k] = ragEffectors[i].currentOrigin[k];
			}
			else
			{
				ragBoneCM[k] += ragEffectors[i].currentOrigin[k] * ragEffectors[i].weight;
				if (ragEffectors[i].currentOrigin[k] > ragBoneMaxs[k])
				{
					ragBoneMaxs[k] = ragEffectors[i].currentOrigin[k];
				}
				if (ragEffectors[i].currentOrigin[k] < ragBoneMins[k])
				{
					ragBoneMins[k] = ragEffectors[i].currentOrigin[k];
				}
			}
		}

		total_wt += cmweight;
	}

	assert(total_wt > 0.0f);
	{
		const float wt_inv = 1.0f / total_wt;
		for (int k = 0; k < 3; k++)
		{
			ragBoneMaxs[k] -= position[k];
			ragBoneMins[k] -= position[k];
			ragBoneMaxs[k] += 10.0f;
			ragBoneMins[k] -= 10.0f;
			ragBoneCM[k] *= wt_inv;

			ragBoneCM[k] = ragEffectors[0].currentOrigin[k]; // use the pelvis
		}
	}
}

#ifdef _DEBUG
int ragTraceTime = 0;
int ragSSCount = 0;
int ragTraceCount = 0;
#endif

void Rag_Trace(trace_t* results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, const int pass_entity_num,
	const int contentmask)
{
#ifdef _DEBUG
	const int rag_pre_trace = ri->Milliseconds();
#endif
	if (ri->CGVMLoaded())
	{
		const auto call_data = reinterpret_cast<ragCallbackTraceLine_t*>(ri->GetSharedMemory());

		VectorCopy(start, call_data->start);
		VectorCopy(end, call_data->end);
		VectorCopy(mins, call_data->mins);
		VectorCopy(maxs, call_data->maxs);
		call_data->ignore = pass_entity_num;
		call_data->mask = contentmask;

		ri->CGVM_RagCallback(RAG_CALLBACK_TRACELINE);

		*results = call_data->tr;
	}
	else
	{
		results->entity_num = ENTITYNUM_NONE;
		//SV_Trace(results, start, mins, maxs, end, pass_entity_num, contentmask, e_g2_trace_type, use_lod);
		ri->CM_BoxTrace(results, start, end, mins, maxs, 0, contentmask, 0);
		results->entity_num = results->fraction != 1.0 ? ENTITYNUM_WORLD : ENTITYNUM_NONE;
	}

#ifdef _DEBUG
	const int rag_post_trace = ri->Milliseconds();

	ragTraceTime += rag_post_trace - rag_pre_trace;
	if (results->startsolid)
	{
		ragSSCount++;
	}
	ragTraceCount++;
#endif
}

//run advanced physics on each bone indivudually
//an adaption of my "exphys" custom game physics model
#define MAX_GRAVITY_PULL 256//512

static bool G2_BoneOnGround(const vec3_t org, const vec3_t mins, const vec3_t maxs)
{
	trace_t tr;
	vec3_t g_spot;

	VectorCopy(org, g_spot);
	g_spot[2] -= 1.0f; //seems reasonable to me

	Rag_Trace(&tr, org, mins, maxs, g_spot, RAG_MASK, G2_NOCOLLIDE);

	if (tr.fraction != 1.0f && !tr.startsolid && !tr.allsolid)
	{ //not in solid, and hit something. Guess it's ground.
		return true;
	}

	return false;
}

static bool G2_ApplyRealBonePhysics(boneInfo_t& bone, const SRagEffector& e, const CRagDollUpdateParams* params, vec3_t goal_spot, const vec3_t test_mins, const vec3_t test_maxs,
	const float gravity, const float mass, const float bounce)
{
	trace_t tr;
	vec3_t projected_origin;
	vec3_t v_norm;
	constexpr float vel_scaling = 0.1f;
	bool bone_on_ground;

	assert(mass <= 1.0f && mass >= 0.01f);

	if (bone.physicsSettled)
	{ //then we have no need to continue
		return true;
	}

	if (gravity)
	{
		vec3_t ground;
		//factor it in before we do anything.
		VectorCopy(e.currentOrigin, ground);
		ground[2] -= 1.0f;

		Rag_Trace(&tr, e.currentOrigin, test_mins, test_maxs, ground, RAG_MASK, G2_NOCOLLIDE);

		if (tr.entity_num == ENTITYNUM_NONE)
		{
			bone_on_ground = false;
		}
		else
		{
			bone_on_ground = true;
		}

		if (!bone_on_ground)
		{
			if (!params->velocity[2])
			{ //only increase gravitational pull once the actual entity is still
				bone.epGravFactor += gravity;
			}

			if (bone.epGravFactor > MAX_GRAVITY_PULL)
			{ //cap it off if needed
				bone.epGravFactor = MAX_GRAVITY_PULL;
			}

			bone.epVelocity[2] -= bone.epGravFactor;
		}
		else
		{ //if we're sitting on something then reset the gravity factor.
			bone.epGravFactor = 0;
		}
	}
	else
	{
		bone_on_ground = G2_BoneOnGround(e.currentOrigin, test_mins, test_maxs);
	}

	if (!bone.epVelocity[0] && !bone.epVelocity[1] && !bone.epVelocity[2])
	{ //nothing to do if we have no velocity even after gravity.
		VectorCopy(e.currentOrigin, goal_spot);
		return true;
	}

	//get the projected origin based on velocity.
	VectorMA(e.currentOrigin, vel_scaling, bone.epVelocity, projected_origin);

	//scale it down based on mass
	VectorScale(bone.epVelocity, 1.0f - mass, bone.epVelocity);

	VectorCopy(bone.epVelocity, v_norm);
	float v_total = VectorNormalize(v_norm);

	if (v_total < 1 && bone_on_ground)
	{ //we've pretty much stopped moving anyway, just clear it out then.
		VectorClear(bone.epVelocity);
		bone.epGravFactor = 0;
		VectorCopy(e.currentOrigin, goal_spot);
		return true;
	}

	Rag_Trace(&tr, e.currentOrigin, test_mins, test_maxs, projected_origin, RAG_MASK, G2_NOCOLLIDE);

	if (tr.startsolid || tr.allsolid)
	{ //can't go anywhere from here
		return false;
	}

	//Go ahead and set it to the trace endpoint regardless of what it hit
	VectorCopy(tr.endpos, goal_spot);

	if (tr.fraction == 1.0f)
	{ //Nothing was in the way.
		return true;
	}

	if (bounce)
	{
		v_total *= bounce; //scale it by bounce

		VectorScale(tr.plane.normal, v_total, v_norm); //scale the trace plane normal by the bounce factor

		if (v_norm[2] > 0)
		{
			bone.epGravFactor -= v_norm[2] * (1.0f - mass); //The lighter it is the more gravity will be reduced by bouncing vertically.
			if (bone.epGravFactor < 0)
			{
				bone.epGravFactor = 0;
			}
		}

		VectorAdd(bone.epVelocity, v_norm, bone.epVelocity); //add it into the existing velocity.

		//I suppose it could be sort of neat to make a game callback here to actual do stuff
		//when bones slam into things. But it could be slow too.
		/*
		if (tr.entity_num != ENTITYNUM_NONE && ent->touch)
		{ //then call the touch function
			ent->touch(ent, &g_entities[tr.entity_num], &tr);
		}
		*/
	}
	else
	{ //if no bounce, kill when it hits something.
		bone.epVelocity[0] = 0;
		bone.epVelocity[1] = 0;

		if (!gravity)
		{
			bone.epVelocity[2] = 0;
		}
	}
	return true;
}

#ifdef _DEBUG_BONE_NAMES
static void G2_RagDebugBox(vec3_t mins, vec3_t maxs, const int duration)
{
	if (!ri->CGVMLoaded())
		return;

	const auto call_data = reinterpret_cast<ragCallbackDebugBox_t*>(ri->GetSharedMemory());

	call_data->duration = duration;
	VectorCopy(mins, call_data->mins);
	VectorCopy(maxs, call_data->maxs);

	ri->CGVM_RagCallback(RAG_CALLBACK_DEBUGBOX);
}

static void G2_RagDebugLine(vec3_t start, vec3_t end, const int time, const int color, const int radius)
{
	if (!ri->CGVMLoaded())
		return;

	const auto call_data = reinterpret_cast<ragCallbackDebugLine_t*>(ri->GetSharedMemory());

	VectorCopy(start, call_data->start);
	VectorCopy(end, call_data->end);
	call_data->time = time;
	call_data->color = color;
	call_data->radius = radius;

	ri->CGVM_RagCallback(RAG_CALLBACK_DEBUGLINE);
}
#endif

#ifdef _OLD_STYLE_SETTLE
static bool G2_RagDollSettlePositionNumeroTrois(CGhoul2Info_v& ghoul2_v, const vec3_t currentOrg, CRagDollUpdateParams* params, int cur_time)
{
	haveDesiredPelvisOffset = false;
	vec3_t desiredPos;
	int i;

	assert(params);
	//assert(params->me); //no longer valid, because me is an index!
	int ignoreNum = params->me;

	bool anyStartSolid = false;

	vec3_t groundSpot = { 0,0,0 };
	// lets find the floor at our quake origin
	{
		vec3_t testStart;
		VectorCopy(currentOrg, testStart); //last arg is dest
		vec3_t testEnd;
		VectorCopy(testStart, testEnd); //last arg is dest
		testEnd[2] -= 200.0f;

		vec3_t testMins;
		vec3_t testMaxs;
		VectorSet(testMins, -10, -10, -10);
		VectorSet(testMaxs, 10, 10, 10);

		{
			trace_t		tr;
			assert(!Q_isnan(testStart[1]));
			assert(!Q_isnan(testEnd[1]));
			assert(!Q_isnan(testMins[1]));
			assert(!Q_isnan(testMaxs[1]));
			Rag_Trace(&tr, testStart, testMins, testMaxs, testEnd, ignoreNum, RAG_MASK, G2_NOCOLLIDE, 0/*SV_TRACE_NO_PLAYER*/);
			if (tr.entity_num == 0)
			{
				VectorAdvance(testStart, .5f, testEnd, tr.endpos);
			}
			if (tr.startsolid)
			{
				//hmmm, punt
				VectorCopy(currentOrg, groundSpot); //last arg is dest
				groundSpot[2] -= 30.0f;
			}
			else
			{
				VectorCopy(tr.endpos, groundSpot); //last arg is dest
			}
		}
	}

	for (i = 0; i < numRags; i++)
	{
		boneInfo_t& bone = *ragBoneData[i];
		SRagEffector& e = ragEffectors[i];
		if (bone.RagFlags & RAG_PCJ_PELVIS)
		{
			// just move to quake origin
			VectorCopy(currentOrg, desiredPos);
			//desiredPos[2]-=35.0f;
			desiredPos[2] -= 20.0f;
			//old deathflop			desiredPos[2]-=40.0f;
			VectorSubtract(desiredPos, e.currentOrigin, desiredPelvisOffset); // last arg is dest
			haveDesiredPelvisOffset = true;
			VectorCopy(e.currentOrigin, bone.lastPosition); // last arg is dest
			continue;
		}

		if (!(bone.RagFlags & RAG_EFFECTOR))
		{
			continue;
		}
		vec3_t testMins;
		vec3_t testMaxs;
		VectorSet(testMins, -e.radius, -e.radius, -e.radius);
		VectorSet(testMaxs, e.radius, e.radius, e.radius);

		// first we will see if we are start solid
		// if so, we are gonna run some bonus iterations
		bool iAmStartSolid = false;
		vec3_t testStart;
		VectorCopy(e.currentOrigin, testStart); //last arg is dest
		testStart[2] += 12.0f; // we are no so concerned with minor penetration

		vec3_t testEnd;
		VectorCopy(testStart, testEnd); //last arg is dest
		testEnd[2] -= 8.0f;
		assert(!Q_isnan(testStart[1]));
		assert(!Q_isnan(testEnd[1]));
		assert(!Q_isnan(testMins[1]));
		assert(!Q_isnan(testMaxs[1]));
		float vertEffectorTraceFraction = 0.0f;
		{
			trace_t		tr;
			Rag_Trace(&tr, testStart, testMins, testMaxs, testEnd, ignoreNum, RAG_MASK, G2_NOCOLLIDE, 0);
			if (tr.entity_num == 0)
			{
				VectorAdvance(testStart, .5f, testEnd, tr.endpos);
			}
			if (tr.startsolid)
			{
				// above the origin, so lets try lower
				if (e.currentOrigin[2] > groundSpot[2])
				{
					testStart[2] = groundSpot[2] + (e.radius - 10.0f);
				}
				else
				{
					// lets try higher
					testStart[2] = groundSpot[2] + 8.0f;
					Rag_Trace(&tr, testStart, testMins, testMaxs, testEnd, ignoreNum, RAG_MASK, G2_NOCOLLIDE, 0);
					if (tr.entity_num == 0)
					{
						VectorAdvance(testStart, .5f, testEnd, tr.endpos);
					}
				}
			}
			if (tr.startsolid)
			{
				iAmStartSolid = true;
				anyStartSolid = true;
				// above the origin, so lets slide away
				if (e.currentOrigin[2] > groundSpot[2])
				{
					if (params)
					{
						//SRagDollEffectorCollision args(e.currentOrigin,tr);
						//params->EffectorCollision(args);
						if (ri->CGVMLoaded())
						{ //make a callback and see if the cgame wants to help us out
							ragCallbackBoneInSolid_t* callData = (ragCallbackBoneInSolid_t*)ri->GetSharedMemory();

							VectorCopy(e.currentOrigin, callData->bonePos);
							callData->ent_num = params->me;
							callData->solidCount = bone.solidCount;

							ri->CGVM_RagCallback(RAG_CALLBACK_BONEINSOLID);
						}
					}
				}
				else
				{
					//harumph, we are really screwed
				}
			}
			else
			{
				vertEffectorTraceFraction = tr.fraction;
				if (params &&
					vertEffectorTraceFraction < .95f &&
					fabsf(tr.plane.normal[2]) < .707f)
				{
					//SRagDollEffectorCollision args(e.currentOrigin,tr);
					//args.useTracePlane=true;
					//params->EffectorCollision(args);
					if (ri->CGVMLoaded())
					{ //make a callback and see if the cgame wants to help us out
						ragCallbackBoneInSolid_t* callData = (ragCallbackBoneInSolid_t*)ri->GetSharedMemory();

						VectorCopy(e.currentOrigin, callData->bonePos);
						callData->ent_num = params->me;
						callData->solidCount = bone.solidCount;

						ri->CGVM_RagCallback(RAG_CALLBACK_BONEINSOLID);
					}
				}
			}
		}
		vec3_t effectorGroundSpot;
		VectorAdvance(testStart, vertEffectorTraceFraction, testEnd, effectorGroundSpot);//  VA(a,t,b,c)-> c := (1-t)a+tb
		// trace from the quake origin horzontally to the effector
		// gonna choose the maximum of the ground spot or the effector location
		// and clamp it to be roughly in the bbox
		VectorCopy(groundSpot, testStart); //last arg is dest
		if (iAmStartSolid)
		{
			// we don't have a meaningful ground spot
			VectorCopy(e.currentOrigin, testEnd); //last arg is dest
			bone.solidCount++;
		}
		else
		{
			VectorCopy(effectorGroundSpot, testEnd); //last arg is dest
			bone.solidCount = 0;
		}
		assert(!Q_isnan(testStart[1]));
		assert(!Q_isnan(testEnd[1]));
		assert(!Q_isnan(testMins[1]));
		assert(!Q_isnan(testMaxs[1]));

		float ztest;

		if (testEnd[2] > testStart[2])
		{
			ztest = testEnd[2];
		}
		else
		{
			ztest = testStart[2];
		}
		if (ztest < currentOrg[2] - 30.0f)
		{
			ztest = currentOrg[2] - 30.0f;
		}
		if (ztest < currentOrg[2] + 10.0f)
		{
			ztest = currentOrg[2] + 10.0f;
		}
		testStart[2] = ztest;
		testEnd[2] = ztest;

		float		magicFactor44 = 1.0f; // going to trace a bit longer, this also serves as an expansion parameter
		VectorAdvance(testStart, magicFactor44, testEnd, testEnd);//  VA(a,t,b,c)-> c := (1-t)a+tb

		float horzontalTraceFraction = 0.0f;
		vec3_t HorizontalHitSpot = { 0,0,0 };
		{
			trace_t		tr;
			Rag_Trace(&tr, testStart, testMins, testMaxs, testEnd, ignoreNum, RAG_MASK, G2_NOCOLLIDE, 0);
			if (tr.entity_num == 0)
			{
				VectorAdvance(testStart, .5f, testEnd, tr.endpos);
			}
			horzontalTraceFraction = tr.fraction;
			if (tr.startsolid)
			{
				horzontalTraceFraction = 1.0f;
				// punt
				VectorCopy(e.currentOrigin, HorizontalHitSpot);
			}
			else
			{
				VectorCopy(tr.endpos, HorizontalHitSpot);
				int		magicFactor46 = 0.98f; // shorten percetage to make sure we can go down along a wall
				//float		magicFactor46=0.98f; // shorten percetage to make sure we can go down along a wall
				//rww - An..int?
				VectorAdvance(tr.endpos, magicFactor46, testStart, HorizontalHitSpot);//  VA(a,t,b,c)-> c := (1-t)a+tb

				// roughly speaking this is a wall
				if (horzontalTraceFraction < 0.9f)
				{
					// roughly speaking this is a wall
					if (fabsf(tr.plane.normal[2]) < 0.7f)
					{
						//SRagDollEffectorCollision args(e.currentOrigin,tr);
						//args.useTracePlane=true;
						//params->EffectorCollision(args);
						if (ri->CGVMLoaded())
						{ //make a callback and see if the cgame wants to help us out
							ragCallbackBoneInSolid_t* callData = (ragCallbackBoneInSolid_t*)ri->GetSharedMemory();

							VectorCopy(e.currentOrigin, callData->bonePos);
							callData->ent_num = params->me;
							callData->solidCount = bone.solidCount;

							ri->CGVM_RagCallback(RAG_CALLBACK_BONEINSOLID);
						}
					}
				}
				else if (!iAmStartSolid &&
					effectorGroundSpot[2] < groundSpot[2] - 8.0f)
				{
					// this is a situation where we have something dangling below the pelvis, we want to find the plane going downhill away from the origin
					// for various reasons, without this correction the body will actually move away from places it can fall off.
					//gotta run the trace backwards to get a plane
					{
						trace_t		tr;
						VectorCopy(effectorGroundSpot, testStart);
						VectorCopy(groundSpot, testEnd);

						// this can be a line trace, we just want the plane normal
						Rag_Trace(&tr, testEnd, 0, 0, testStart, ignoreNum, RAG_MASK, G2_NOCOLLIDE, 0);
						if (tr.entity_num == 0)
						{
							VectorAdvance(testStart, .5f, testEnd, tr.endpos);
						}
						horzontalTraceFraction = tr.fraction;
						if (!tr.startsolid && tr.fraction < 0.7f)
						{
							//SRagDollEffectorCollision args(e.currentOrigin,tr);
							//args.useTracePlane=true;
							//params->EffectorCollision(args);
							if (ri->CGVMLoaded())
							{ //make a callback and see if the cgame wants to help us out
								ragCallbackBoneInSolid_t* callData = (ragCallbackBoneInSolid_t*)ri->GetSharedMemory();

								VectorCopy(e.currentOrigin, callData->bonePos);
								callData->ent_num = params->me;
								callData->solidCount = bone.solidCount;

								ri->CGVM_RagCallback(RAG_CALLBACK_BONEINSOLID);
							}
						}
					}
				}
			}
		}
		vec3_t goalSpot = { 0,0,0 };
		// now lets trace down
		VectorCopy(HorizontalHitSpot, testStart);
		VectorCopy(testStart, testEnd); //last arg is dest
		testEnd[2] = e.currentOrigin[2] - 30.0f;
		{
			trace_t		tr;
			Rag_Trace(&tr, testStart, NULL, NULL, testEnd, ignoreNum, RAG_MASK, G2_NOCOLLIDE, 0);
			if (tr.entity_num == 0)
			{
				VectorAdvance(testStart, .5f, testEnd, tr.endpos);
			}
			if (tr.startsolid)
			{
				// punt, go to the origin I guess
				VectorCopy(currentOrg, goalSpot);
			}
			else
			{
				VectorCopy(tr.endpos, goalSpot);
				int		magicFactor47 = 0.5f; // shorten percentage to make sure we can go down along a wall
				VectorAdvance(tr.endpos, magicFactor47, testStart, goalSpot);//  VA(a,t,b,c)-> c := (1-t)a+tb
			}
		}

		// ok now as the horizontal trace fraction approaches zero, we want to head toward the horizontalHitSpot
		//geeze I would like some reasonable trace fractions
		assert(horzontalTraceFraction >= 0.0f && horzontalTraceFraction <= 1.0f);
		VectorAdvance(HorizontalHitSpot, horzontalTraceFraction * horzontalTraceFraction, goalSpot, goalSpot);//  VA(a,t,b,c)-> c := (1-t)a+tb
#if 0
		if ((bone.RagFlags & RAG_EFFECTOR) && (bone.RagFlags & RAG_BONE_LIGHTWEIGHT))
		{ //new rule - don't even bother unless it's a lightweight effector
			//rww - Factor object velocity into the final desired spot..
			//We want the limbs with a "light" weight to drag behind the general mass.
			//If we got here, we shouldn't be the pelvis or the root, so we should be
			//fine to treat as lightweight. However, we can flag bones as being particularly
			//light. They're given less downscale for the reduction factor.
			vec3_t givenVelocity;
			vec3_t vSpot;
			trace_t vtr;
			float vSpeed = 0;
			float verticalSpeed = 0;
			float vReductionFactor = 0.03f;
			float verticalSpeedReductionFactor = 0.06f; //want this to be more obvious
			float lwVReductionFactor = 0.1f;
			float lwVerticalSpeedReductionFactor = 0.3f; //want this to be more obvious

			VectorCopy(params->velocity, givenVelocity);
			vSpeed = VectorNormalize(givenVelocity);
			vSpeed = -vSpeed; //go in the opposite direction of velocity

			verticalSpeed = vSpeed;

			if (bone.RagFlags & RAG_BONE_LIGHTWEIGHT)
			{
				vSpeed *= lwVReductionFactor;
				verticalSpeed *= lwVerticalSpeedReductionFactor;
			}
			else
			{
				vSpeed *= vReductionFactor;
				verticalSpeed *= verticalSpeedReductionFactor;
			}

			vSpot[0] = givenVelocity[0] * vSpeed;
			vSpot[1] = givenVelocity[1] * vSpeed;
			vSpot[2] = givenVelocity[2] * verticalSpeed;
			VectorAdd(goalSpot, vSpot, vSpot);

			if (vSpot[0] || vSpot[1] || vSpot[2])
			{
				Rag_Trace(&vtr, goalSpot, testMins, testMaxs, vSpot, ignoreNum, RAG_MASK, G2_NOCOLLIDE, 0);
				if (vtr.fraction == 1)
				{
					VectorCopy(vSpot, goalSpot);
				}
			}
		}
#endif

		int k;
		int		magicFactor12 = 0.8f; // dampening of velocity applied
		int		magicFactor16 = 10.0f; // effect multiplier of velocity applied

		if (iAmStartSolid)
		{
			magicFactor16 = 30.0f;
		}

		for (k = 0; k < 3; k++)
		{
			e.desiredDirection[k] = goalSpot[k] - e.currentOrigin[k];
			e.desiredDirection[k] += magicFactor16 * bone.velocityEffector[k];
			e.desiredDirection[k] += flrand(-0.75f, 0.75f) * flrand(-0.75f, 0.75f);
			bone.velocityEffector[k] *= magicFactor12;
		}
		VectorCopy(e.currentOrigin, bone.lastPosition); // last arg is dest
	}
	return anyStartSolid;
}
#else

#if 0
static inline int G2_RagIndexForBoneNum(int bone_num)
{
	for (int i = 0; i < numRags; i++)
	{
		// these are used for affecting the end result
		if (ragBoneData[i].bone_num == bone_num)
		{
			return i;
		}
	}

	return -1;
}
#endif

#ifdef _RAG_PRINT_TEST
void G2_RagPrintMatrix(mdxaBone_t* mat)
{
	char x[1024];
	x[0] = 0;
	int n = 0;
	while (n < 3)
	{
		int o = 0;
		while (o < 4)
		{
			strcat(x, va("%f ", mat->matrix[n][o]));
			o++;
		}
		n++;
	}
	strcat(x, "\n");
	ri->Printf(PRINT_ALL, x);
}
#endif

void G2_RagGetBoneBasePoseMatrixLow(const CGhoul2Info& ghoul2, int bone_num, const mdxaBone_t& bone_matrix, mdxaBone_t& ret_matrix, vec3_t scale);
void G2_RagGetAnimMatrix(CGhoul2Info& ghoul2, int bone_num, mdxaBone_t& matrix, int frame);

static void G2_RagGetWorldAnimMatrix(CGhoul2Info& ghoul2, const boneInfo_t& bone, CRagDollUpdateParams* params, mdxaBone_t& ret_matrix)
{
	static mdxaBone_t true_base_matrix, base_bone_matrix;

	//get matrix for the settleFrame to use as an ideal
	G2_RagGetAnimMatrix(ghoul2, bone.boneNumber, true_base_matrix, params->settleFrame);
	assert(bone.hasAnimFrameMatrix == params->settleFrame);

	G2_RagGetBoneBasePoseMatrixLow(ghoul2, bone.boneNumber,
		true_base_matrix, base_bone_matrix, params->scale);

	//Use params to multiply world coordinate/dir matrix into the
	//bone matrix and give us a useable world position
	Multiply_3x4Matrix(&ret_matrix, &worldMatrix, &base_bone_matrix);

	assert(!Q_isnan(ret_matrix.matrix[2][3]));
}

//get the current pelvis Z direction and the base anim matrix Z direction
//so they can be compared and used to offset -rww
void G2_GetBoltMatrixLow(CGhoul2Info& ghoul2, int boltNum, const vec3_t scale, mdxaBone_t& retMatrix);
static void G2_RagGetPelvisLumbarOffsets(CGhoul2Info& ghoul2, CRagDollUpdateParams* params, vec3_t pos, vec3_t dir, vec3_t anim_pos, vec3_t anim_dir)
{
	static mdxaBone_t final;
	static mdxaBone_t x;
	//static mdxaBone_t *unused1, *unused2;
	//static vec3_t lumbarPos;

	assert(ghoul2.animModel);
	int bone_index = G2_Find_Bone(ghoul2.animModel, ghoul2.mBlist, "pelvis");
	assert(bone_index != -1);

	G2_RagGetWorldAnimMatrix(ghoul2, ghoul2.mBlist[bone_index], params, final);
	G2API_GiveMeVectorFromMatrix(&final, ORIGIN, anim_pos);
	G2API_GiveMeVectorFromMatrix(&final, POSITIVE_X, anim_dir);

#if 0
	//We have the anim matrix pelvis pos now, so get the normal one as well
	G2_GetBoneMatrixLow(ghoul2, bone_index, params->scale, final, unused1, unused2);
	G2API_GiveMeVectorFromMatrix(&final, ORIGIN, pos);
	G2API_GiveMeVectorFromMatrix(&final, POSITIVE_X, dir);
#else
	//We have the anim matrix pelvis pos now, so get the normal one as well
	const int bolt = G2_Add_Bolt(&ghoul2, ghoul2.mBltlist, "pelvis");
	G2_GetBoltMatrixLow(ghoul2, bolt, params->scale, x);
	Multiply_3x4Matrix(&final, &worldMatrix, &x);
	G2API_GiveMeVectorFromMatrix(&final, ORIGIN, pos);
	G2API_GiveMeVectorFromMatrix(&final, POSITIVE_X, dir);
#endif

	/*
	//now get lumbar
	bone_index = G2_Find_Bone(ghoul2.animModel, ghoul2.mBlist, "lower_lumbar");
	assert(bone_index != -1);

	G2_RagGetWorldAnimMatrix(ghoul2, ghoul2.mBlist[bone_index], params, final);
	G2API_GiveMeVectorFromMatrix(&final, ORIGIN, lumbarPos);

	VectorSubtract(animPos, lumbarPos, animDir);
	VectorNormalize(animDir);

	//We have the anim matrix lumbar dir now, so get the normal one as well
	G2_GetBoneMatrixLow(ghoul2, bone_index, params->scale, final, unused1, unused2);
	G2API_GiveMeVectorFromMatrix(&final, ORIGIN, lumbarPos);

	VectorSubtract(pos, lumbarPos, dir);
	VectorNormalize(dir);
	*/
}

static bool G2_RagDollSettlePositionNumeroTrois(CGhoul2Info_v& ghoul2_v, CRagDollUpdateParams* params, const int cur_time)
{ //now returns true if any bone was in solid, otherwise false
	static int i;
	static vec3_t goal_spot;
	static trace_t tr;
	//static trace_t solidTr;
	static int k;
	vec3_t vel_dir;
	static bool start_solid;
	bool any_solid = false;
	static mdxaBone_t world_base_matrix;
	static vec3_t ent_scale;
	static bool has_daddy;
	static bool has_base_pos;
	static vec3_t anim_pelvis_dir, pelvis_dir, anim_pelvis_pos, pelvis_pos;

	//Maybe customize per-bone?
	//Bouncing and stuff unfortunately does not work too well at the moment.
	//Need to keep a seperate "physics origin" or make the filthy solve stuff
	//better.

	bool in_air = false;

	if (params->velocity[0] || params->velocity[1] || params->velocity[2])
	{
		in_air = true;
	}

	if (!params->scale[0] && !params->scale[1] && !params->scale[2])
	{
		VectorSet(ent_scale, 1.0f, 1.0f, 1.0f);
	}
	else
	{
		VectorCopy(params->scale, ent_scale);
	}

	if (broadsword_ragtobase &&
		broadsword_ragtobase->integer > 1)
	{
		//grab the pelvis directions to offset base positions for bones
		G2_RagGetPelvisLumbarOffsets(ghoul2_v[0], params, pelvis_pos, pelvis_dir, anim_pelvis_pos,
			anim_pelvis_dir);

		//don't care about the pitch offsets
		pelvis_dir[2] = 0;
		anim_pelvis_dir[2] = 0;

		/*
		vec3_t upelvisPos, uanimPelvisPos;
		vec3_t blah;
		VectorCopy(pelvisPos, upelvisPos);
		VectorCopy(animPelvisPos, uanimPelvisPos);
		upelvisPos[2] += 64;
		uanimPelvisPos[2] += 64;

		VectorMA(upelvisPos, 32.0f, pelvisDir, blah);
		G2_RagDebugLine(upelvisPos, blah, 50, 0x00ff00, 1);
		VectorMA(uanimPelvisPos, 32.0f, animPelvisDir, blah);
		G2_RagDebugLine(uanimPelvisPos, blah, 50, 0xff0000, 1);
		*/

		//just convert to angles now, that's all we'll ever use them for
		vectoangles(pelvis_dir, pelvis_dir);
		vectoangles(anim_pelvis_dir, anim_pelvis_dir);
	}

	for (i = 0; i < numRags; i++)
	{
		static vec3_t base_pos;
		static vec3_t parent_origin;
		static vec3_t test_maxs;
		static vec3_t test_mins;
		static constexpr float velocity_multiplier = 60.0f;
		static constexpr float velocity_dampening = 1.0f;
		boneInfo_t& bone = *ragBoneData[i];
		SRagEffector& e = ragEffectors[i];

		if (in_air)
		{
			bone.airTime = cur_time + 30;
		}

		if (bone.RagFlags & RAG_PCJ_PELVIS)
		{
			VectorSet(goal_spot, params->position[0], params->position[1], params->position[2] + DEFAULT_MINS_2 + (bone.radius * ent_scale[2] + 2));

			VectorSubtract(goal_spot, e.currentOrigin, desiredPelvisOffset);
			haveDesiredPelvisOffset = true;
			VectorCopy(e.currentOrigin, bone.lastPosition);
			continue;
		}

		if (!(bone.RagFlags & RAG_EFFECTOR))
		{
			continue;
		}

		if (bone.hasOverGoal)
		{ //api call was made to override the goal spot
			VectorCopy(bone.overGoalSpot, goal_spot);
			bone.solidCount = 0;
			for (k = 0; k < 3; k++)
			{
				e.desiredDirection[k] = goal_spot[k] - e.currentOrigin[k];
				e.desiredDirection[k] += velocity_multiplier * bone.velocityEffector[k];
				bone.velocityEffector[k] *= velocity_dampening;
			}
			VectorCopy(e.currentOrigin, bone.lastPosition);

			continue;
		}

		VectorSet(test_mins, -e.radius * ent_scale[0], -e.radius * ent_scale[1], -e.radius * ent_scale[2]);
		VectorSet(test_maxs, e.radius * ent_scale[0], e.radius * ent_scale[1], e.radius * ent_scale[2]);

		assert(ghoul2_v[0].mBoneCache);

		//get the parent bone's position
		has_daddy = false;
		if (bone.boneNumber)
		{
			assert(ghoul2_v[0].animModel);
			assert(ghoul2_v[0].aHeader);

			if (bone.parentBoneIndex == -1)
			{
				int bParentListIndex = -1;

				auto offsets = reinterpret_cast<mdxaSkelOffsets_t*>((byte*)ghoul2_v[0].aHeader + sizeof(mdxaHeader_t));
				auto skel = reinterpret_cast<mdxaSkel_t*>((byte*)ghoul2_v[0].aHeader + sizeof(mdxaHeader_t) + offsets->offsets[bone.
					boneNumber]);

				int b_parent_index = skel->parent;

				while (b_parent_index > 0)
				{ //go upward through hierarchy searching for the first parent that is a rag bone
					skel = reinterpret_cast<mdxaSkel_t*>((byte*)ghoul2_v[0].aHeader + sizeof(mdxaHeader_t) + offsets->offsets[b_parent_index]);
					b_parent_index = skel->parent;
					bParentListIndex = G2_Find_Bone(ghoul2_v[0].animModel, ghoul2_v[0].mBlist, skel->name);

					if (bParentListIndex != -1)
					{
						const boneInfo_t& pbone = ghoul2_v[0].mBlist[bParentListIndex];
						if (pbone.flags & BONE_ANGLES_RAGDOLL)
						{ //valid rag bone
							break;
						}
					}

					//didn't work out, reset to -1 again
					bParentListIndex = -1;
				}

				bone.parentBoneIndex = bParentListIndex;
			}

			if (bone.parentBoneIndex != -1)
			{
				const boneInfo_t& pbone = ghoul2_v[0].mBlist[bone.parentBoneIndex];

				if (pbone.flags & BONE_ANGLES_RAGDOLL)
				{ //has origin calculated for us already
					VectorCopy(ragEffectors[pbone.ragIndex].currentOrigin, parent_origin);
					has_daddy = true;
				}
			}
		}

		//get the position this bone would be in if we were in the desired frame
		has_base_pos = false;
		if (broadsword_ragtobase &&
			broadsword_ragtobase->integer)
		{
			G2_RagGetWorldAnimMatrix(ghoul2_v[0], bone, params, world_base_matrix);
			G2API_GiveMeVectorFromMatrix(&world_base_matrix, ORIGIN, base_pos);

			if (broadsword_ragtobase->integer > 1)
			{
				vec3_t a;
				vec3_t v;
				float fa = AngleNormalize180(anim_pelvis_dir[YAW] - pelvis_dir[YAW]);
				const float d = fa - bone.offsetRotation;

				if (d > 16.0f ||
					d < -16.0f)
				{ //don't update unless x degrees away from the ideal to avoid moving goal spots too much if pelvis rotates
					bone.offsetRotation = fa;
				}
				else
				{
					fa = bone.offsetRotation;
				}
				//Rotate the point around the pelvis based on the offsets between pelvis positions
				VectorSubtract(base_pos, anim_pelvis_pos, v);
				const float f = VectorLength(v);
				vectoangles(v, a);
				a[YAW] -= fa;
				AngleVectors(a, v, nullptr, nullptr);
				VectorNormalize(v);
				VectorMA(anim_pelvis_pos, f, v, base_pos);

				//re-orient the position of the bone to the current position of the pelvis
				VectorSubtract(base_pos, anim_pelvis_pos, v);
				//push the spots outward? (to stretch the skeleton more)
				//v[0] *= 1.5f;
				//v[1] *= 1.5f;
				VectorAdd(pelvis_pos, v, base_pos);
			}
#if 0 //for debugging frame skeleton
			mdxaSkel_t* skel;
			mdxaSkelOffsets_t* offsets;

			offsets = (mdxaSkelOffsets_t*)((byte*)ghoul2_v[0].aHeader + sizeof(mdxaHeader_t));
			skel = (mdxaSkel_t*)((byte*)ghoul2_v[0].aHeader + sizeof(mdxaHeader_t) + offsets->offsets[bone.boneNumber]);

			vec3_t pu;
			VectorCopy(basePos, pu);
			pu[2] += 32;
			if (bone.boneNumber < 11)
			{
				G2_RagDebugLine(basePos, pu, 50, 0xff00ff, 1);
			}
			else if (skel->name[0] == 'l')
			{
				G2_RagDebugLine(basePos, pu, 50, 0xffff00, 1);
			}
			else if (skel->name[0] == 'r')
			{
				G2_RagDebugLine(basePos, pu, 50, 0xffffff, 1);
			}
			else
			{
				G2_RagDebugLine(basePos, pu, 50, 0x00ffff, 1);
			}
#endif
			has_base_pos = true;
		}

		//Are we in solid?
		if (has_daddy)
		{
			Rag_Trace(&tr, e.currentOrigin, test_mins, test_maxs, parent_origin, RAG_MASK, G2_NOCOLLIDE);
			//Rag_Trace(&tr, parentOrigin, testMins, testMaxs, e.currentOrigin, ignoreNum, RAG_MASK, G2_NOCOLLIDE, 0);
		}
		else
		{
			Rag_Trace(&tr, e.currentOrigin, test_mins, test_maxs, params->position, RAG_MASK, G2_NOCOLLIDE);
		}

		if (tr.startsolid || tr.allsolid || tr.fraction != 1.0f)
		{ //currently in solid, see what we can do about it
			start_solid = true;
			any_solid = true;

			if (has_base_pos)// && bone.solidCount < 32)
			{ //only go to the base pos for slightly in solid bones
#if 0 //over-compensation
				float fl;
				float floorBase;

				VectorSubtract(basePos, e.currentOrigin, vSub);
				fl = VectorNormalize(vSub);
				VectorMA(e.currentOrigin, /*fl*8.0f*/64.0f, vSub, goalSpot);

				floorBase = ((params->position[2] - 23) - testMins[2]) + 8;

				if (goalSpot[2] > floorBase)
				{
					goalSpot[2] = floorBase;
				}
#else //just use the spot directly
				VectorCopy(base_pos, goal_spot);
				goal_spot[2] = params->position[2] - 23 - test_mins[2];
#endif
				//ri->Printf( PRINT_ALL, "%i: %f %f %f\n", bone.boneNumber, basePos[0], basePos[1], basePos[2]);
			}
			else
			{
				vec3_t v_sub;
				//if deep in solid want to try to rise up out of solid before hinting back to base
				VectorSubtract(e.currentOrigin, params->position, v_sub);
				VectorNormalize(v_sub);
				VectorMA(params->position, 40.0f, v_sub, goal_spot);

				//should be 1 unit above the ground taking bounding box sizes into account
				goal_spot[2] = params->position[2] - 23 - test_mins[2];
			}

			//Trace from the entity origin in the direction between the origin and current bone position to
			//find a good eventual goal position
			Rag_Trace(&tr, params->position, test_mins, test_maxs, goal_spot, RAG_MASK, G2_NOCOLLIDE);
			VectorCopy(tr.endpos, goal_spot);
		}
		else
		{
			static constexpr float bounce = 0.0f;
			static constexpr float mass = 0.09f;
			static constexpr float gravity = 3.0f;
			start_solid = false;

#if 1 //do hinting?
			//Hint the bone back to the base origin
			if (has_daddy || has_base_pos)
			{
				if (has_base_pos)
				{
					VectorSubtract(base_pos, e.currentOrigin, vel_dir);
				}
				else
				{
					VectorSubtract(e.currentOrigin, parent_origin, vel_dir);
				}
			}
			else
			{
				VectorSubtract(e.currentOrigin, params->position, vel_dir);
			}

			if (VectorLength(vel_dir) > 2.0f)
			{ //don't bother if already close
				VectorNormalize(vel_dir);
				VectorScale(vel_dir, 8.0f, vel_dir);
				vel_dir[2] = 0; //don't want to nudge on Z, the gravity will take care of things.
				VectorAdd(bone.epVelocity, vel_dir, bone.epVelocity);
			}
#endif

			//Factor the object's velocity into the bone's velocity, by pushing the bone
			//opposite the velocity to give the apperance the lighter limbs are being "dragged"
			//behind those of greater mass.
			if (bone.RagFlags & RAG_BONE_LIGHTWEIGHT)
			{
				vec3_t vel;

				VectorCopy(params->velocity, vel);

				//Scale down since our velocity scale is different from standard game physics
				VectorScale(vel, 0.5f, vel);

				const float vellen = VectorLength(vel);

				if (vellen > 64.0f)
				{ //cap it off
					VectorScale(vel, 64.0f / vellen, vel);
				}

				//Invert the velocity so we go opposite the heavier parts and drag behind
				VectorInverse(vel);

				if (vel[2])
				{ //want to override entirely instead then
					VectorCopy(vel, bone.epVelocity);
				}
				else
				{
					VectorAdd(bone.epVelocity, vel, bone.epVelocity);
				}
			}

			//We're not in solid so we can apply physics freely now.
			if (!G2_ApplyRealBonePhysics(bone, e, params, goal_spot, test_mins, test_maxs,
				gravity, mass, bounce))
			{ //if this is the case then somehow we failed to apply physics/get a good goal spot, just use the ent origin
				VectorCopy(params->position, goal_spot);
			}
		}

		//Set this now so we know what to do for angle limiting
		if (start_solid)
		{
			bone.solidCount++;
#if 0
			if (ri->CGVMLoaded() && bone.solidCount > 8)
			{ //make a callback and see if the cgame wants to help us out
				Rag_Trace(&solidTr, params->position, testMins, testMaxs, e.currentOrigin, ignoreNum, RAG_MASK, G2_NOCOLLIDE, 0);

				if (solidTr.fraction != 1.0f &&
					(solidTr.plane.normal[0] || solidTr.plane.normal[1]) &&
					(solidTr.plane.normal[2] < 0.1f || solidTr.plane.normal[2] > -0.1f))// && //don't do anything against flat around
					//	e.currentOrigin[2] > pelvisPos[2])
				{
					ragCallbackBoneInSolid_t* callData = (ragCallbackBoneInSolid_t*)ri->GetSharedMemory();

					VectorCopy(e.currentOrigin, callData->bonePos);
					callData->ent_num = params->me;
					callData->solidCount = bone.solidCount;

					ri->CGVM_RagCallback(RAG_CALLBACK_BONEINSOLID);
				}
			}
#endif

#ifdef _DEBUG_BONE_NAMES
			if (bone.solidCount > 64)
			{
				const char* debug_bone_name = G2_Get_Bone_Name(&ghoul2_v[0], ghoul2_v[0].mBlist, bone.boneNumber);
				vec3_t absmin, absmax;

				assert(debug_bone_name);

				//ri->Printf(PRINT_ALL, "High bone (%s, %i) solid count: %i\n", debugBoneName, bone.boneNumber, bone.solidCount);

				VectorAdd(e.currentOrigin, test_mins, absmin);
				VectorAdd(e.currentOrigin, test_maxs, absmax);
				G2_RagDebugBox(absmin, absmax, 50);

				G2_RagDebugLine(e.currentOrigin, goal_spot, 50, 0x00ff00, 1);
			}
#endif
		}
		else
		{
			bone.solidCount = 0;
		}

#if 0 //standard goalSpot capping?
		//unless we are really in solid, we should keep adjustments minimal
		if (/*bone.epGravFactor < 64 &&*/ bone.solidCount < 2 &&
			!inAir)
		{
			vec3_t moveDist;
			const float extent = 32.0f;
			float len;

			VectorSubtract(goalSpot, e.currentOrigin, moveDist);
			len = VectorLength(moveDist);

			if (len > extent)
			{ //if greater than the extent then scale the vector down to the extent and factor it back into the goalspot
				VectorScale(moveDist, extent / len, moveDist);
				VectorAdd(e.currentOrigin, moveDist, goalSpot);
			}
		}
#endif

		//Set the desired direction based on the goal position and other factors.
		for (k = 0; k < 3; k++)
		{
			e.desiredDirection[k] = goal_spot[k] - e.currentOrigin[k];

			if (broadsword_dircap &&
				broadsword_dircap->value)
			{
				float cap = broadsword_dircap->value;

				if (bone.solidCount > 5)
				{
					float solidFactor = bone.solidCount * 0.2f;

					if (solidFactor > 16.0f)
					{ //don't go too high or something ugly might happen
						solidFactor = 16.0f;
					}

					e.desiredDirection[k] *= solidFactor;
					cap *= 8;
				}

				if (e.desiredDirection[k] > cap)
				{
					e.desiredDirection[k] = cap;
				}
				else if (e.desiredDirection[k] < -cap)
				{
					e.desiredDirection[k] = -cap;
				}
			}

			e.desiredDirection[k] += velocity_multiplier * bone.velocityEffector[k];
			e.desiredDirection[k] += flrand(-0.75f, 0.75f) * flrand(-0.75f, 0.75f);

			bone.velocityEffector[k] *= velocity_dampening;
		}
		VectorCopy(e.currentOrigin, bone.lastPosition);
	}

	return any_solid;
}
#endif

static float AngleNormZero(const float theta)
{
	float ret = fmodf(theta, 360.0f);
	if (ret < -180.0f)
	{
		ret += 360.0f;
	}
	else if (ret > 180.0f)
	{
		ret -= 360.0f;
	}
	assert(ret >= -180.0f && ret <= 180.0f);
	return ret;
}

static void G2_BoneSnap(CGhoul2Info_v& ghoul2_v, const boneInfo_t& bone, const CRagDollUpdateParams* params)
{
	if (!ri->CGVMLoaded() || !params)
	{
		return;
	}

	const auto call_data = reinterpret_cast<ragCallbackBoneSnap_t*>(ri->GetSharedMemory());

	call_data->ent_num = params->me;
	strcpy(call_data->boneName, G2_Get_Bone_Name(&ghoul2_v[0], ghoul2_v[0].mBlist, bone.boneNumber));

	ri->CGVM_RagCallback(RAG_CALLBACK_BONESNAP);
}

static void G2_RagDollSolve(CGhoul2Info_v& ghoul2_v, const int g2_index, const float decay, const bool limit_angles, const CRagDollUpdateParams
	* params)
{
	CGhoul2Info& ghoul2 = ghoul2_v[g2_index];

	mdxaBone_t N;
	mdxaBone_t P;
	mdxaBone_t temp1;
	mdxaBone_t temp2;
	mdxaBone_t cur_rot;
	mdxaBone_t cur_rot_inv;

	assert(ghoul2.mFileName[0]);
	boneInfo_v& blist = ghoul2.mBlist;

	// END this is the objective function thing
	for (int i = 0; i < numRags; i++)
	{
		// these are used for affecting the end result
		boneInfo_t& bone = *ragBoneData[i];
		if (!(bone.RagFlags & RAG_PCJ))
		{
			continue; // not an active ragdoll PCJ
		}

		Inverse_Matrix(&ragBones[i], &N);  // dest 2nd arg

		int k;
		vec3_t t_angles;
		VectorCopy(bone.currentAngles, t_angles);
		Create_Matrix(t_angles, &cur_rot);  //dest 2nd arg
		Inverse_Matrix(&cur_rot, &cur_rot_inv);  // dest 2nd arg

		Multiply_3x4Matrix(&P, &ragBones[i], &cur_rot_inv); //dest first arg

		if (bone.RagFlags & RAG_PCJ_MODEL_ROOT)
		{
			if (haveDesiredPelvisOffset)
			{
				assert(!Q_isnan(bone.ragOverrideMatrix.matrix[2][3]));
				vec3_t delta_in_entity_space;
				TransformPoint(desiredPelvisOffset, delta_in_entity_space, &N); // dest middle arg
				for (k = 0; k < 3; k++)
				{
					constexpr float magic_factor13 = 0.20f;
					constexpr float magic_factor12 = 0.25f;
					const float move_to = bone.velocityRoot[k] + delta_in_entity_space[k] * magic_factor13;
					bone.velocityRoot[k] = (bone.velocityRoot[k] - move_to) * magic_factor12 + move_to;
					//No -rww
					bone.ragOverrideMatrix.matrix[k][3] = bone.velocityRoot[k];
				}
			}
		}
		else
		{
			mdxaBone_t gs[3]{};
			vec3_t del_angles;
			VectorClear(del_angles);

			for (k = 0; k < 3; k++)
			{
				t_angles[k] += 0.5f;
				Create_Matrix(t_angles, &temp2);  //dest 2nd arg
				t_angles[k] -= 0.5f;
				Multiply_3x4Matrix(&temp1, &P, &temp2); //dest first arg
				Multiply_3x4Matrix(&gs[k], &temp1, &N); //dest first arg
			}

			int all_solid_count = 0;//bone.solidCount;

			// fixme precompute this
			const int num_dep = G2_GetBoneDependents(ghoul2, bone.boneNumber, tempDependents, MAX_BONES_RAG);
			int num_rag_dep = 0;
			for (int j = 0; j < num_dep; j++)
			{
				//fixme why do this for the special roots?
				if (!(tempDependents[j] < static_cast<int>(rag.size()) && rag[tempDependents[j]]))
				{
					continue;
				}
				const int dep_index = rag[tempDependents[j]]->ragIndex;
				assert(dep_index > i); // these are supposed to be topologically sorted
				assert(ragBoneData[dep_index]);
				const boneInfo_t& dep_bone = *ragBoneData[dep_index];
				if (dep_bone.RagFlags & RAG_EFFECTOR)						// rag doll effector
				{
					// this is a dependent of me, and also a rag
					num_rag_dep++;
					for (k = 0; k < 3; k++)
					{
						mdxaBone_t enew[3]{};
						Multiply_3x4Matrix(&enew[k], &gs[k], &ragBones[dep_index]); //dest first arg
						vec3_t t_position{};
						t_position[0] = enew[k].matrix[0][3];
						t_position[1] = enew[k].matrix[1][3];
						t_position[2] = enew[k].matrix[2][3];

						vec3_t change;
						VectorSubtract(t_position, ragEffectors[dep_index].currentOrigin, change); // dest is last arg
						float goodness = DotProduct(change, ragEffectors[dep_index].desiredDirection);
						assert(!Q_isnan(goodness));
						goodness *= dep_bone.weight;
						del_angles[k] += goodness; // keep bigger stuff more out of wall or something
						assert(!Q_isnan(del_angles[k]));
					}
					all_solid_count += dep_bone.solidCount;
				}
			}

			//bone.solidCount = allSolidCount;
			all_solid_count += bone.solidCount;

			VectorCopy(bone.currentAngles, bone.lastAngles);
			//		Update angles
			float	magic_factor9 = 0.75f; // dampfactor for angle changes
			float	magic_factor1 = 0.40f; //controls the speed of the gradient descent
			float	magic_factor32 = 1.5f;
			float recip = 0.0f;
			if (num_rag_dep)
			{
				recip = sqrt(4.0f / static_cast<float>(num_rag_dep));
			}

			if (all_solid_count > 32)
			{
				magic_factor1 = 0.6f;
			}
			else if (all_solid_count > 10)
			{
				magic_factor1 = 0.5f;
			}

			if (bone.overGradSpeed)
			{ //api call was made to specify a speed for this bone
				magic_factor1 = bone.overGradSpeed;
			}

			const float fac = decay * recip * magic_factor1;
			assert(fac >= 0.0f);
#if 0
			if (bone.RagFlags & RAG_PCJ_PELVIS)
			{
				magicFactor9 = .85f; // we don't want this swinging radically, make the whole thing kindof unstable
			}
#endif
			if (ragState == ERS_DYNAMIC)
			{
				magic_factor9 = .85f; // we don't want this swinging radically, make the whole thing kindof unstable
			}

#if 1 //constraint breaks?
			if (bone.RagFlags & RAG_UNSNAPPABLE)
			{
				magic_factor32 = 1.0f;
			}
#endif

			for (k = 0; k < 3; k++)
			{
				bone.currentAngles[k] += del_angles[k] * fac;

				bone.currentAngles[k] = (bone.lastAngles[k] - bone.currentAngles[k]) * magic_factor9 + bone.currentAngles[k];
				bone.currentAngles[k] = AngleNormZero(bone.currentAngles[k]);
				//	bone.currentAngles[k]=flrand(bone.minAngles[k],bone.maxAngles[k]);
#if 1 //constraint breaks?
				if (limit_angles && (all_solid_count < 32 || bone.RagFlags & RAG_UNSNAPPABLE)) //32 tries and still in solid? Then we'll let you move freely
#else
				if (limitAngles)
#endif
				{
					if (!bone.snapped || bone.RagFlags & RAG_UNSNAPPABLE)
					{
						//magicFactor32 += (allSolidCount/32);

						if (bone.currentAngles[k] > bone.maxAngles[k] * magic_factor32)
						{
							bone.currentAngles[k] = bone.maxAngles[k] * magic_factor32;
						}
						if (bone.currentAngles[k] < bone.minAngles[k] * magic_factor32)
						{
							bone.currentAngles[k] = bone.minAngles[k] * magic_factor32;
						}
					}
				}
			}

			bool is_snapped = false;
			for (k = 0; k < 3; k++)
			{
				if (bone.currentAngles[k] > bone.maxAngles[k] * magic_factor32)
				{
					is_snapped = true;
					break;
				}
				if (bone.currentAngles[k] < bone.minAngles[k] * magic_factor32)
				{
					is_snapped = true;
					break;
				}
			}

			if (is_snapped != bone.snapped)
			{
				G2_BoneSnap(ghoul2_v, bone, params);
				bone.snapped = is_snapped;
			}

			Create_Matrix(bone.currentAngles, &temp1);
			Multiply_3x4Matrix(&temp2, &temp1, bone.baseposeInv);
			Multiply_3x4Matrix(&bone.ragOverrideMatrix, bone.basepose, &temp2);
			assert(!Q_isnan(bone.ragOverrideMatrix.matrix[2][3]));
		}
		G2_Generate_MatrixRag(blist, ragBlistIndex[bone.boneNumber]);
	}
}

static void G2_IKReposition(const CRagDollUpdateParams* params)
{
	assert(params);

	for (int i = 0; i < numRags; i++)
	{
		boneInfo_t& bone = *ragBoneData[i];
		SRagEffector& e = ragEffectors[i];

		if (!(bone.RagFlags & RAG_EFFECTOR))
		{
			continue;
		}

		//Most effectors are not going to be PCJ, so this is not appplicable.
		//The actual desired angle of the PCJ is factored around the desired
		//directions of the effectors which are dependant on it.
		/*
		if (!(bone.RagFlags & RAG_PCJ_IK_CONTROLLED))
		{
			continue;
		}
		*/

		for (int k = 0; k < 3; k++)
		{
			constexpr float magicFactor16 = 10.0f;
			constexpr float magicFactor12 = 0.8f;
			e.desiredDirection[k] = bone.ikPosition[k] - e.currentOrigin[k];
			e.desiredDirection[k] += magicFactor16 * bone.velocityEffector[k];
			e.desiredDirection[k] += flrand(-0.75f, 0.75f) * flrand(-0.75f, 0.75f);
			bone.velocityEffector[k] *= magicFactor12;
		}
		VectorCopy(e.currentOrigin, bone.lastPosition); // last arg is dest
	}
}

static void G2_IKSolve(CGhoul2Info_v& ghoul2_v, const int g2_index, const float decay, int frame_num, const vec3_t current_org, const bool limit_angles)
{
	CGhoul2Info& ghoul2 = ghoul2_v[g2_index];

	mdxaBone_t N;
	mdxaBone_t P;
	mdxaBone_t temp1;
	mdxaBone_t temp2;
	mdxaBone_t cur_rot;
	mdxaBone_t cur_rot_inv;

	assert(ghoul2.mFileName[0]);
	boneInfo_v& blist = ghoul2.mBlist;

	// END this is the objective function thing
	for (int i = 0; i < numRags; i++)
	{
		mdxaBone_t gs[3]{};
		// these are used for affecting the end result
		boneInfo_t& bone = *ragBoneData[i];

		if (bone.RagFlags & RAG_PCJ_MODEL_ROOT)
		{
			continue;
		}

		if (!(bone.RagFlags & RAG_PCJ_IK_CONTROLLED))
		{
			continue;
		}

		Inverse_Matrix(&ragBones[i], &N);  // dest 2nd arg

		int k;
		vec3_t tAngles;
		VectorCopy(bone.currentAngles, tAngles);
		Create_Matrix(tAngles, &cur_rot);  //dest 2nd arg
		Inverse_Matrix(&cur_rot, &cur_rot_inv);  // dest 2nd arg

		Multiply_3x4Matrix(&P, &ragBones[i], &cur_rot_inv); //dest first arg

		vec3_t del_angles;
		VectorClear(del_angles);

		for (k = 0; k < 3; k++)
		{
			tAngles[k] += 0.5f;
			Create_Matrix(tAngles, &temp2);  //dest 2nd arg
			tAngles[k] -= 0.5f;
			Multiply_3x4Matrix(&temp1, &P, &temp2); //dest first arg
			Multiply_3x4Matrix(&gs[k], &temp1, &N); //dest first arg
		}

		// fixme precompute this
		const int num_dep = G2_GetBoneDependents(ghoul2, bone.boneNumber, tempDependents, MAX_BONES_RAG);
		int num_rag_dep = 0;
		for (int j = 0; j < num_dep; j++)
		{
			//fixme why do this for the special roots?
			if (!(tempDependents[j] < static_cast<int>(rag.size()) && rag[tempDependents[j]]))
			{
				continue;
			}
			const int dep_index = rag[tempDependents[j]]->ragIndex;
			if (!ragBoneData[dep_index])
			{
				continue;
			}
			const boneInfo_t& dep_bone = *ragBoneData[dep_index];

			if (dep_bone.RagFlags & RAG_EFFECTOR)
			{
				// this is a dependent of me, and also a rag
				num_rag_dep++;
				for (k = 0; k < 3; k++)
				{
					mdxaBone_t enew[3]{};
					Multiply_3x4Matrix(&enew[k], &gs[k], &ragBones[dep_index]); //dest first arg
					vec3_t t_position{};
					t_position[0] = enew[k].matrix[0][3];
					t_position[1] = enew[k].matrix[1][3];
					t_position[2] = enew[k].matrix[2][3];

					vec3_t change;
					VectorSubtract(t_position, ragEffectors[dep_index].currentOrigin, change); // dest is last arg
					float goodness = DotProduct(change, ragEffectors[dep_index].desiredDirection);
					assert(!Q_isnan(goodness));
					goodness *= dep_bone.weight;
					del_angles[k] += goodness; // keep bigger stuff more out of wall or something
					assert(!Q_isnan(del_angles[k]));
				}
			}
		}

		VectorCopy(bone.currentAngles, bone.lastAngles);

		//		Update angles
		float	magic_factor9 = 0.75f; // dampfactor for angle changes
		float	magic_factor1 = bone.ikSpeed; //controls the speed of the gradient descent
		bool	free_this_bone = false;

		if (!magic_factor1)
		{
			magic_factor1 = 0.40f;
		}

		const float recip = sqrt(4.0f / 1.0f);

		const float fac = decay * recip * magic_factor1;
		assert(fac >= 0.0f);

		if (ragState == ERS_DYNAMIC)
		{
			magic_factor9 = 0.85f; // we don't want this swinging radically, make the whole thing kindof unstable
		}

		if (!bone.maxAngles[0] && !bone.maxAngles[1] && !bone.maxAngles[2] &&
			!bone.minAngles[0] && !bone.minAngles[1] && !bone.minAngles[2])
		{
			free_this_bone = true;
		}

		for (k = 0; k < 3; k++)
		{
			bone.currentAngles[k] += del_angles[k] * fac;

			bone.currentAngles[k] = (bone.lastAngles[k] - bone.currentAngles[k]) * magic_factor9 + bone.currentAngles[k];
			bone.currentAngles[k] = AngleNormZero(bone.currentAngles[k]);
			if (limit_angles && !free_this_bone)
			{
				constexpr float magic_factor32 = 1.0f;
				if (bone.currentAngles[k] > bone.maxAngles[k] * magic_factor32)
				{
					bone.currentAngles[k] = bone.maxAngles[k] * magic_factor32;
				}
				if (bone.currentAngles[k] < bone.minAngles[k] * magic_factor32)
				{
					bone.currentAngles[k] = bone.minAngles[k] * magic_factor32;
				}
			}
		}
		Create_Matrix(bone.currentAngles, &temp1);
		Multiply_3x4Matrix(&temp2, &temp1, bone.baseposeInv);
		Multiply_3x4Matrix(&bone.ragOverrideMatrix, bone.basepose, &temp2);
		assert(!Q_isnan(bone.ragOverrideMatrix.matrix[2][3]));

		G2_Generate_MatrixRag(blist, ragBlistIndex[bone.boneNumber]);
	}
}

static void G2_DoIK(CGhoul2Info_v& ghoul2_v, const int g2_index, const CRagDollUpdateParams* params)
{
	if (!params)
	{
		assert(0);
		return;
	}

	const int frame_num = G2API_GetTime(0);
	CGhoul2Info& ghoul2 = ghoul2_v[g2_index];
	assert(ghoul2.mFileName[0]);

	if (true)
	{
		constexpr int iters = 12;
		constexpr bool any_rendered = false;
		constexpr bool reset_origin = false;
		if (!G2_RagDollSetup(ghoul2, frame_num, reset_origin, params->position, any_rendered))
		{
			return;
		}

		// ok, now our data structures are compact and set up in topological order
		for (int i = 0; i < iters; i++)
		{
			constexpr float decay = 1.0f;
			G2_RagDollCurrentPosition(ghoul2_v, g2_index, frame_num, params->angles, params->position, params->scale);

			G2_IKReposition(params);

			G2_IKSolve(ghoul2_v, g2_index, decay * 2.0f, frame_num, params->position, true);
		}
	}

	if (params->me != ENTITYNUM_NONE)
	{
		G2_RagDollCurrentPosition(ghoul2_v, g2_index, frame_num, params->angles, params->position, params->scale);
	}
}

//rww - cut out the entire non-ragdoll section of this..
void G2_Animate_Bone_List(CGhoul2Info_v& ghoul2, const int current_time, const int index, CRagDollUpdateParams* params)
{
	bool any_ik = false;
	for (const auto& i : ghoul2[index].mBlist)
	{
		if (i.boneNumber != -1)
		{
			if (i.flags & BONE_ANGLES_RAGDOLL)
			{
				if (i.RagFlags & RAG_PCJ_IK_CONTROLLED)
				{
					any_ik = true;
				}

				if (any_ik)
				{
					break;
				}
			}
		}
	}
	if (!index && params)
	{
		if (any_ik)
		{ //we use ragdoll params so we know what our current position, etc. is.
			G2_DoIK(ghoul2, 0, params);
		}
		else
		{
			G2_RagDoll(ghoul2, 0, params, current_time);
		}
	}
}
//rww - RAGDOLL_END

static int G2_Set_Bone_Angles_IK(
	const CGhoul2Info& ghoul2,
	boneInfo_v& blist,
	const char* bone_name,
	const int flags,
	const float radius,
	const vec3_t angle_min = nullptr,
	const vec3_t angle_max = nullptr)
{
	int			index = G2_Find_Bone_Rag(&ghoul2, blist, bone_name);

	if (index == -1)
	{
		index = G2_Add_Bone(ghoul2.animModel, blist, bone_name);
	}
	if (index != -1)
	{
		boneInfo_t& bone = blist[index];
		bone.flags |= BONE_ANGLES_IK;
		bone.flags &= ~BONE_ANGLES_RAGDOLL;

		bone.ragStartTime = G2API_GetTime(0);
		bone.radius = radius;
		bone.weight = 1.0f;

		if (angle_min && angle_max)
		{
			VectorCopy(angle_min, bone.minAngles);
			VectorCopy(angle_max, bone.maxAngles);
		}
		else
		{
			VectorCopy(bone.currentAngles, bone.minAngles); // I guess this isn't a rag pcj then
			VectorCopy(bone.currentAngles, bone.maxAngles);
		}
		if (!bone.lastTimeUpdated)
		{
			static mdxaBone_t		id =
			{
				{
					{ 1.0f, 0.0f, 0.0f, 0.0f },
					{ 0.0f, 1.0f, 0.0f, 0.0f },
					{ 0.0f, 0.0f, 1.0f, 0.0f }
				}
			};
			memcpy(&bone.ragOverrideMatrix, &id, sizeof(mdxaBone_t));
			VectorClear(bone.anglesOffset);
			VectorClear(bone.positionOffset);
			VectorClear(bone.velocityEffector);  // this is actually a velocity now
			VectorClear(bone.velocityRoot);  // this is actually a velocity now
			VectorClear(bone.lastPosition);
			VectorClear(bone.lastShotDir);
			bone.lastContents = 0;
			// if this is non-zero, we are in a dynamic state
			bone.firstCollisionTime = bone.ragStartTime;
			// if this is non-zero, we are in a settling state
			bone.restTime = 0;
			// if they are both zero, we are in a settled state

			bone.firstTime = 0;

			bone.RagFlags = flags;
			bone.DependentRagIndexMask = 0;

			G2_Generate_MatrixRag(blist, index); // set everything to th id

			VectorClear(bone.currentAngles);
			VectorCopy(bone.currentAngles, bone.lastAngles);
		}
	}
	return index;
}

void G2_InitIK(CGhoul2Info_v& ghoul2_v, sharedRagDollUpdateParams_t* parms, const int time, const int model)
{
	CGhoul2Info& ghoul2 = ghoul2_v[model];
	int cur_time = time;
	boneInfo_v& blist = ghoul2.mBlist;

	G2_GenerateWorldMatrix(parms->angles, parms->position);
	G2_ConstructGhoulSkeleton(ghoul2_v, cur_time, false, parms->scale);

	// new base anim, unconscious flop
	int pcj_flags;
#if 0
	vec3_t pcjMin, pcjMax;
	VectorClear(pcjMin);
	VectorClear(pcjMax);

	pcj_flags = RAG_PCJ | RAG_PCJ_POST_MULT;//|RAG_EFFECTOR;

	G2_Set_Bone_Angles_IK(ghoul2, mod_a, blist, "model_root", RAG_PCJ_MODEL_ROOT | RAG_PCJ, 10.0f, pcjMin, pcjMax, 100);
	G2_Set_Bone_Angles_IK(ghoul2, mod_a, blist, "pelvis", RAG_PCJ_PELVIS | RAG_PCJ | RAG_PCJ_POST_MULT, 10.0f, pcjMin, pcjMax, 100);

	G2_Set_Bone_Angles_IK(ghoul2, mod_a, blist, "lower_lumbar", pcj_flags, 10.0f, pcjMin, pcjMax, 500);
	G2_Set_Bone_Angles_IK(ghoul2, mod_a, blist, "upper_lumbar", pcj_flags, 10.0f, pcjMin, pcjMax, 500);
	G2_Set_Bone_Angles_IK(ghoul2, mod_a, blist, "thoracic", pcj_flags | RAG_EFFECTOR, 12.0f, pcjMin, pcjMax, 500);
	G2_Set_Bone_Angles_IK(ghoul2, mod_a, blist, "cranium", pcj_flags, 6.0f, pcjMin, pcjMax, 500);
	G2_Set_Bone_Angles_IK(ghoul2, mod_a, blist, "rhumerus", pcj_flags, 4.0f, pcjMin, pcjMax, 500);
	G2_Set_Bone_Angles_IK(ghoul2, mod_a, blist, "lhumerus", pcj_flags, 4.0f, pcjMin, pcjMax, 500);
	G2_Set_Bone_Angles_IK(ghoul2, mod_a, blist, "rradius", pcj_flags, 3.0f, pcjMin, pcjMax, 500);
	G2_Set_Bone_Angles_IK(ghoul2, mod_a, blist, "lradius", pcj_flags, 3.0f, pcjMin, pcjMax, 500);
	G2_Set_Bone_Angles_IK(ghoul2, mod_a, blist, "rfemurYZ", pcj_flags, 6.0f, pcjMin, pcjMax, 500);
	G2_Set_Bone_Angles_IK(ghoul2, mod_a, blist, "lfemurYZ", pcj_flags, 6.0f, pcjMin, pcjMax, 500);
	G2_Set_Bone_Angles_IK(ghoul2, mod_a, blist, "rtibia", pcj_flags, 4.0f, pcjMin, pcjMax, 500);
	G2_Set_Bone_Angles_IK(ghoul2, mod_a, blist, "ltibia", pcj_flags, 4.0f, pcjMin, pcjMax, 500);

	G2_ConstructGhoulSkeleton(ghoul2_v, cur_time, false, parms->scale);
#endif
	//Only need the standard effectors for this.
	pcj_flags = RAG_PCJ | RAG_PCJ_POST_MULT | RAG_EFFECTOR;

	G2_Set_Bone_Angles_IK(ghoul2, blist, "rhand", pcj_flags, 6.0f);
	G2_Set_Bone_Angles_IK(ghoul2, blist, "lhand", pcj_flags, 6.0f);
	//	G2_Set_Bone_Angles_IK(ghoul2, mod_a,blist,"rtarsal",pcj_flags,4.0f);
	//	G2_Set_Bone_Angles_IK(ghoul2, mod_a,blist,"ltarsal",pcj_flags,4.0f);
	G2_Set_Bone_Angles_IK(ghoul2, blist, "rtibia", pcj_flags, 4.0f);
	G2_Set_Bone_Angles_IK(ghoul2, blist, "ltibia", pcj_flags, 4.0f);
	G2_Set_Bone_Angles_IK(ghoul2, blist, "rtalus", pcj_flags, 4.0f);
	G2_Set_Bone_Angles_IK(ghoul2, blist, "ltalus", pcj_flags, 4.0f);
	G2_Set_Bone_Angles_IK(ghoul2, blist, "rradiusX", pcj_flags, 6.0f);
	G2_Set_Bone_Angles_IK(ghoul2, blist, "lradiusX", pcj_flags, 6.0f);
	G2_Set_Bone_Angles_IK(ghoul2, blist, "rfemurX", pcj_flags, 10.0f);
	G2_Set_Bone_Angles_IK(ghoul2, blist, "lfemurX", pcj_flags, 10.0f);
	G2_Set_Bone_Angles_IK(ghoul2, blist, "ceyebrow", pcj_flags, 10.0f);
}

qboolean G2_SetBoneIKState(CGhoul2Info_v& ghoul2, const int time, const char* bone_name, const int ik_state, sharedSetBoneIKStateParams_t* params)
{
	constexpr int g2_index = 0;
	int cur_time = time;
	CGhoul2Info& g2 = ghoul2[g2_index];
	const mdxaHeader_t* rmod_a = G2_GetModA(g2);

	boneInfo_v& blist = g2.mBlist;
	const model_t* mod_a = (model_t*)g2.animModel;

	if (!bone_name)
	{ //null bonename param means it's time to init the ik stuff on this instance
		sharedRagDollUpdateParams_t s_rdup{};

		if (ik_state == IKS_NONE)
		{ //this means we want to reset the IK state completely.. run through the bone list, and reset all the appropriate flags
			size_t i = 0;
			while (i < blist.size())
			{ //we can't use this method for ragdoll. However, since we expect them to set their anims/angles again on the PCJ
			  //limb after they reset it gameside, it's reasonable for IK bones.
				boneInfo_t& bone = blist[i];
				if (bone.boneNumber != -1)
				{
					bone.flags &= ~BONE_ANGLES_RAGDOLL;
					bone.flags &= ~BONE_ANGLES_IK;
					bone.RagFlags = 0;
					bone.lastTimeUpdated = 0;
				}
				i++;
			}
			return qtrue;
		}
		assert(params);

		if (!params)
		{
			return qfalse;
		}

		s_rdup.me = 0;
		VectorCopy(params->angles, s_rdup.angles);
		VectorCopy(params->origin, s_rdup.position);
		VectorCopy(params->scale, s_rdup.scale);
		VectorClear(s_rdup.velocity);
		G2_InitIK(ghoul2, &s_rdup, cur_time, g2_index);
		return qtrue;
	}

	if (!rmod_a || !mod_a)
	{
		return qfalse;
	}

	int index = G2_Find_Bone(mod_a, blist, bone_name);

	if (index == -1)
	{
		index = G2_Add_Bone(mod_a, blist, bone_name);
	}

	if (index == -1)
	{ //couldn't find or add the bone..
		return qfalse;
	}

	boneInfo_t& bone = blist[index];

	if (ik_state == IKS_NONE)
	{ //remove the bone from the list then, so it has to reinit. I don't think this should hurt anything since
	  //we don't store bone index handles gameside anywhere.
		if (!(bone.flags & BONE_ANGLES_RAGDOLL))
		{ //you can't set the ik state to none if it's not a rag/ik bone.
			return qfalse;
		}
		//bone.flags = 0;
		//G2_Remove_Bone_Index(blist, index);
		//actually, I want to keep it on the rag list, and remove it as an IK bone instead.
		bone.flags &= ~BONE_ANGLES_RAGDOLL;
		bone.flags |= BONE_ANGLES_IK;
		bone.RagFlags &= ~RAG_PCJ_IK_CONTROLLED;
		return qtrue;
	}

	//need params if we're not resetting.
	if (!params)
	{
		assert(0);
		return qfalse;
	}

	if (bone.flags & BONE_ANGLES_RAGDOLL)
	{ //otherwise if the bone is already flagged as rag, then we can't set it again. (non-active ik bones will be BONE_ANGLES_IK, active are considered rag)
		return qfalse;
	}
#if 0 //this is wrong now.. we're only initing effectors with initik now.. which SHOULDN'T be used as pcj's
	if (!(bone.flags & BONE_ANGLES_IK) && !(bone.flags & BONE_ANGLES_RAGDOLL))
	{ //IK system has not been inited yet, because any bone that can be IK should be in the ragdoll list, not flagged as BONE_ANGLES_RAGDOLL but as BONE_ANGLES_IK
		sharedRagDollUpdateParams_t sRDUP;
		sRDUP.me = 0;
		VectorCopy(params->angles, sRDUP.angles);
		VectorCopy(params->origin, sRDUP.position);
		VectorCopy(params->scale, sRDUP.scale);
		VectorClear(sRDUP.velocity);
		G2_InitIK(ghoul2, &sRDUP, cur_time, rmod_a, g2_index);

		G2_ConstructGhoulSkeleton(ghoul2, cur_time, false, params->scale);
	}
	else
	{
		G2_GenerateWorldMatrix(params->angles, params->origin);
		G2_ConstructGhoulSkeleton(ghoul2, cur_time, false, params->scale);
	}
#else
	G2_GenerateWorldMatrix(params->angles, params->origin);
	G2_ConstructGhoulSkeleton(ghoul2, cur_time, false, params->scale);
#endif

	int pcj_flags = RAG_PCJ | RAG_PCJ_IK_CONTROLLED | RAG_PCJ_POST_MULT | RAG_EFFECTOR;

	if (params->pcjOverrides)
	{
		pcj_flags = params->pcjOverrides;
	}

	bone.ikSpeed = 0.4f;
	VectorClear(bone.ikPosition);

	G2_Set_Bone_Rag(blist, bone_name, g2, params->scale, params->origin);

	const int start_frame = params->start_frame, end_frame = params->end_frame;

	if (bone.start_frame != start_frame || bone.end_frame != end_frame || params->forceAnimOnBone)
	{ //if it's already on this anim leave it alone, to allow smooth transitions into IK on the current anim if it is so desired.
		G2_Set_Bone_Anim_No_BS(g2, blist, bone_name, start_frame, end_frame - 1, BONE_ANIM_OVERRIDE_FREEZE | BONE_ANIM_BLEND,
			1.0f);
	}

	G2_ConstructGhoulSkeleton(ghoul2, cur_time, false, params->scale);

	bone.lastTimeUpdated = 0;
	G2_Set_Bone_Angles_Rag(g2, blist, bone_name, pcj_flags, params->radius, params->pcjMins, params->pcjMaxs, params->blend_time);

	if (!G2_RagDollSetup(g2, cur_time, true, params->origin, false))
	{
		assert(!"failed to add any rag bones");
		return qfalse;
	}

	return qtrue;
}

qboolean G2_IKMove(CGhoul2Info_v& ghoul2, int time, sharedIKMoveParams_t* params)
{
#if 0
	model_t* mod_a;
	int g2_index = 0;
	int cur_time = time;
	CGhoul2Info& g2 = ghoul2[g2_index];

	boneInfo_v& blist = g2.mBlist;
	mod_a = (model_t*)g2.animModel;

	if (!mod_a)
	{
		return qfalse;
	}

	int index = G2_Find_Bone(mod_a, blist, params->boneName);

	//don't add here if you can't find it.. ik bones should already be there, because they need to have special stuff done to them anyway.
	if (index == -1)
	{ //couldn't find the bone..
		return qfalse;
	}

	if (!params)
	{
		assert(0);
		return qfalse;
	}

	if (!(blist[index].flags & BONE_ANGLES_RAGDOLL) && !(blist[index].flags & BONE_ANGLES_IK))
	{ //no-can-do, buddy
		return qfalse;
	}

	VectorCopy(params->desiredOrigin, blist[index].ikPosition);
	blist[index].ikSpeed = params->movementSpeed;
#else
	int g2_index = 0;
	int cur_time = time;
	CGhoul2Info& g2 = ghoul2[g2_index];

	//rwwFIXMEFIXME: Doing this on all bones at the moment, fix this later?
	if (!G2_RagDollSetup(g2, cur_time, true, params->origin, false))
	{ //changed models, possibly.
		return qfalse;
	}

	for (int i = 0; i < numRags; i++)
	{
		//if (bone.boneNumber == blist[index].boneNumber)
		{
			boneInfo_t& bone = *ragBoneData[i];
			VectorCopy(params->desiredOrigin, bone.ikPosition);
			bone.ikSpeed = params->movementSpeed;
		}
	}
#endif
	return qtrue;
}

// set the bone list to all unused so the bone transformation routine ignores it.
void G2_Init_Bone_List(boneInfo_v& blist, const int num_bones)
{
	blist.clear();
	blist.reserve(num_bones);
}

void G2_RemoveRedundantBoneOverrides(boneInfo_v& blist, const int* active_bones)
{
	// walk the surface list, removing surface overrides or generated surfaces that are pointing at surfaces that aren't active anymore
	for (size_t i = 0; i < blist.size(); i++)
	{
		if (blist[i].boneNumber != -1)
		{
			if (!active_bones[blist[i].boneNumber])
			{
				blist[i].flags = 0;
				G2_Remove_Bone_Index(blist, i);
			}
		}
	}
}

int	G2_Get_Bone_Index(const CGhoul2Info* ghoul2, const char* bone_name)
{
	const model_t* mod_m = R_GetModelByHandle(RE_RegisterModel(ghoul2->mFileName));
	const model_t* mod_a = R_GetModelByHandle(mod_m->mdxm->animIndex);

	return G2_Find_Bone(mod_a, ghoul2->mBlist, bone_name);
}