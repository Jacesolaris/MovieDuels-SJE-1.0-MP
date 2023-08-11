/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
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

/*****************************************************************************
 * name:		be_aas_entity.c
 *
 * desc:		AAS entities
 *
 * $Archive: /MissionPack/code/botlib/be_aas_entity.c $
 * $Author: Zaphod $
 * $Revision: 11 $
 * $Modtime: 11/22/00 8:50a $
 * $Date: 11/22/00 8:55a $
 *
 *****************************************************************************/

#include "qcommon/q_shared.h"
#include "l_memory.h"
#include "l_script.h"
#include "l_precomp.h"
#include "l_struct.h"
#include "l_utils.h"
#include "l_log.h"
#include "aasfile.h"
#include "botlib.h"
#include "be_aas.h"
#include "be_aas_funcs.h"
#include "be_interface.h"
#include "be_aas_def.h"

#define MASK_SOLID		CONTENTS_PLAYERCLIP

 //FIXME: these might change
enum {
	ET_GENERAL,
	ET_PLAYER,
	ET_ITEM,
	ET_MISSILE,
	ET_MOVER
};

//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
int AAS_UpdateEntity(const int entnum, const bot_entitystate_t* state)
{
	int relink;

	if (!aasworld.loaded)
	{
		botimport.Print(PRT_MESSAGE, "AAS_UpdateEntity: not loaded\n");
		return BLERR_NOAASFILE;
	} //end if

	aas_entity_t* ent = &aasworld.entities[entnum];

	if (!state) {
		//unlink the entity
		AAS_UnlinkFromAreas(ent->areas);
		//unlink the entity from the BSP leaves
		AAS_UnlinkFromBSPLeaves(ent->leaves);
		//
		ent->areas = nullptr;
		//
		ent->leaves = nullptr;
		return BLERR_NOERROR;
	}

	ent->i.update_time = AAS_Time() - ent->i.ltime;
	ent->i.type = state->type;
	ent->i.flags = state->flags;
	ent->i.ltime = AAS_Time();
	VectorCopy(ent->i.origin, ent->i.lastvisorigin);
	VectorCopy(state->old_origin, ent->i.old_origin);
	ent->i.solid = state->solid;
	ent->i.groundent = state->groundent;
	ent->i.modelindex = state->modelindex;
	ent->i.modelindex2 = state->modelindex2;
	ent->i.frame = state->frame;
	ent->i.event = state->event;
	ent->i.eventParm = state->eventParm;
	ent->i.powerups = state->powerups;
	ent->i.weapon = state->weapon;
	ent->i.legsAnim = state->legsAnim;
	ent->i.torsoAnim = state->torsoAnim;
	//number of the entity
	ent->i.number = entnum;
	//updated so set valid flag
	ent->i.valid = qtrue;
	//link everything the first frame
	if (aasworld.numframes == 1) relink = qtrue;
	else relink = qfalse;
	//
	if (ent->i.solid == SOLID_BSP)
	{
		//if the angles of the model changed
		if (!VectorCompare(state->angles, ent->i.angles))
		{
			VectorCopy(state->angles, ent->i.angles);
			relink = qtrue;
		} //end if
		//get the mins and maxs of the model
		//FIXME: rotate mins and maxs
		AAS_BSPModelMinsMaxsOrigin(ent->i.modelindex, ent->i.angles, ent->i.mins, ent->i.maxs, nullptr);
	} //end if
	else if (ent->i.solid == SOLID_BBOX)
	{
		//if the bounding box size changed
		if (!VectorCompare(state->mins, ent->i.mins) ||
			!VectorCompare(state->maxs, ent->i.maxs))
		{
			VectorCopy(state->mins, ent->i.mins);
			VectorCopy(state->maxs, ent->i.maxs);
			relink = qtrue;
		} //end if
		VectorCopy(state->angles, ent->i.angles);
	} //end if
	//if the origin changed
	if (!VectorCompare(state->origin, ent->i.origin))
	{
		VectorCopy(state->origin, ent->i.origin);
		relink = qtrue;
	} //end if
	//if the entity should be relinked
	if (relink)
	{
		//don't link the world model
		if (entnum != ENTITYNUM_WORLD)
		{
			vec3_t absmaxs;
			vec3_t absmins;
			//absolute mins and maxs
			VectorAdd(ent->i.mins, ent->i.origin, absmins);
			VectorAdd(ent->i.maxs, ent->i.origin, absmaxs);
			//unlink the entity
			AAS_UnlinkFromAreas(ent->areas);
			//relink the entity to the AAS areas (use the larges bbox)
			ent->areas = AAS_LinkEntityClientBBox(absmins, absmaxs, entnum, PRESENCE_NORMAL);
			//unlink the entity from the BSP leaves
			AAS_UnlinkFromBSPLeaves(ent->leaves);
			//link the entity to the world BSP tree
			ent->leaves = AAS_BSPLinkEntity(absmins, absmaxs, entnum, 0);
		} //end if
	} //end if
	return BLERR_NOERROR;
} //end of the function AAS_UpdateEntity
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void AAS_EntityInfo(const int entnum, aas_entityinfo_t* info)
{
	if (!aasworld.initialized)
	{
		botimport.Print(PRT_FATAL, "AAS_EntityInfo: aasworld not initialized\n");
		Com_Memset(info, 0, sizeof(aas_entityinfo_t));
		return;
	} //end if

	if (entnum < 0 || entnum >= aasworld.maxentities)
	{
		botimport.Print(PRT_FATAL, "AAS_EntityInfo: entnum %d out of range\n", entnum);
		Com_Memset(info, 0, sizeof(aas_entityinfo_t));
		return;
	} //end if

	Com_Memcpy(info, &aasworld.entities[entnum].i, sizeof(aas_entityinfo_t));
} //end of the function AAS_EntityInfo
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void AAS_EntityOrigin(const int entnum, vec3_t origin)
{
	if (entnum < 0 || entnum >= aasworld.maxentities)
	{
		botimport.Print(PRT_FATAL, "AAS_EntityOrigin: entnum %d out of range\n", entnum);
		VectorClear(origin);
		return;
	} //end if

	VectorCopy(aasworld.entities[entnum].i.origin, origin);
} //end of the function AAS_EntityOrigin
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
int AAS_EntityModelindex(const int entnum)
{
	if (entnum < 0 || entnum >= aasworld.maxentities)
	{
		botimport.Print(PRT_FATAL, "AAS_EntityModelindex: entnum %d out of range\n", entnum);
		return 0;
	} //end if
	return aasworld.entities[entnum].i.modelindex;
} //end of the function AAS_EntityModelindex
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
int AAS_EntityType(const int entnum)
{
	if (!aasworld.initialized) return 0;

	if (entnum < 0 || entnum >= aasworld.maxentities)
	{
		botimport.Print(PRT_FATAL, "AAS_EntityType: entnum %d out of range\n", entnum);
		return 0;
	} //end if
	return aasworld.entities[entnum].i.type;
} //end of the AAS_EntityType
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
int AAS_EntityModelNum(const int entnum)
{
	if (!aasworld.initialized) return 0;

	if (entnum < 0 || entnum >= aasworld.maxentities)
	{
		botimport.Print(PRT_FATAL, "AAS_EntityModelNum: entnum %d out of range\n", entnum);
		return 0;
	} //end if
	return aasworld.entities[entnum].i.modelindex;
} //end of the function AAS_EntityModelNum
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
int AAS_OriginOfMoverWithModelNum(const int modelnum, vec3_t origin)
{
	for (int i = 0; i < aasworld.maxentities; i++)
	{
		const aas_entity_t* ent = &aasworld.entities[i];
		if (ent->i.type == ET_MOVER)
		{
			if (ent->i.modelindex == modelnum)
			{
				VectorCopy(ent->i.origin, origin);
				return qtrue;
			} //end if
		} //end if
	} //end for
	return qfalse;
} //end of the function AAS_OriginOfMoverWithModelNum
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void AAS_EntitySize(const int entnum, vec3_t mins, vec3_t maxs)
{
	if (!aasworld.initialized) return;

	if (entnum < 0 || entnum >= aasworld.maxentities)
	{
		botimport.Print(PRT_FATAL, "AAS_EntitySize: entnum %d out of range\n", entnum);
		return;
	} //end if

	const aas_entity_t* ent = &aasworld.entities[entnum];
	VectorCopy(ent->i.mins, mins);
	VectorCopy(ent->i.maxs, maxs);
} //end of the function AAS_EntitySize
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void AAS_EntityBSPData(const int entnum, bsp_entdata_t* entdata)
{
	const aas_entity_t* ent = &aasworld.entities[entnum];
	VectorCopy(ent->i.origin, entdata->origin);
	VectorCopy(ent->i.angles, entdata->angles);
	VectorAdd(ent->i.origin, ent->i.mins, entdata->absmins);
	VectorAdd(ent->i.origin, ent->i.maxs, entdata->absmaxs);
	entdata->solid = ent->i.solid;
	entdata->modelnum = ent->i.modelindex - 1;
} //end of the function AAS_EntityBSPData
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void AAS_ResetEntityLinks()
{
	for (int i = 0; i < aasworld.maxentities; i++)
	{
		aasworld.entities[i].areas = nullptr;
		aasworld.entities[i].leaves = nullptr;
	} //end for
} //end of the function AAS_ResetEntityLinks
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void AAS_InvalidateEntities()
{
	for (int i = 0; i < aasworld.maxentities; i++)
	{
		aasworld.entities[i].i.valid = qfalse;
		aasworld.entities[i].i.number = i;
	} //end for
} //end of the function AAS_InvalidateEntities
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void AAS_UnlinkInvalidEntities()
{
	for (int i = 0; i < aasworld.maxentities; i++)
	{
		aas_entity_t* ent = &aasworld.entities[i];
		if (!ent->i.valid)
		{
			AAS_UnlinkFromAreas(ent->areas);
			ent->areas = nullptr;
			AAS_UnlinkFromBSPLeaves(ent->leaves);
			ent->leaves = nullptr;
		} //end for
	} //end for
} //end of the function AAS_UnlinkInvalidEntities
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
int AAS_NearestEntity(vec3_t origin, const int modelindex)
{
	int bestentnum = 0;
	float bestdist = 99999;
	for (int i = 0; i < aasworld.maxentities; i++)
	{
		vec3_t dir;
		const aas_entity_t* ent = &aasworld.entities[i];
		if (ent->i.modelindex != modelindex) continue;
		VectorSubtract(ent->i.origin, origin, dir);
		if (fabs(dir[0]) < 40)
		{
			if (fabs(dir[1]) < 40)
			{
				const float dist = VectorLength(dir);
				if (dist < bestdist)
				{
					bestdist = dist;
					bestentnum = i;
				} //end if
			} //end if
		} //end if
	} //end for
	return bestentnum;
} //end of the function AAS_NearestEntity
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
int AAS_BestReachableEntityArea(const int entnum)
{
	const aas_entity_t* ent = &aasworld.entities[entnum];
	return AAS_BestReachableLinkArea(ent->areas);
} //end of the function AAS_BestReachableEntityArea
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
int AAS_NextEntity(int entnum)
{
	if (!aasworld.loaded) return 0;

	if (entnum < 0) entnum = -1;
	while (++entnum < aasworld.maxentities)
	{
		if (aasworld.entities[entnum].i.valid) return entnum;
	} //end while
	return 0;
} //end of the function AAS_NextEntity