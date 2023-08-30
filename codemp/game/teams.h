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

#pragma once

typedef enum //# team_e
{
	NPCTEAM_FREE,
	// also TEAM_FREE - caution, some code checks a team_t via "if (!team_t_varname)" so I guess this should stay as entry 0, great or what? -slc
	NPCTEAM_ENEMY,
	// also TEAM_RED
	NPCTEAM_PLAYER,
	// also TEAM_BLUE
	NPCTEAM_NEUTRAL,
	// also TEAM_SPECTATOR - most droids are team_neutral, there are some exceptions like Probe,Seeker,Interrogator
	NPCTEAM_SOLO,

	//# #eol
	NPCTEAM_NUM_TEAMS
} npcteam_t;

// This list is made up from the model directories, this MUST be in the same order as the ClassNames array in NPC_stats.cpp
typedef enum
{
	CLASS_NONE,
	// hopefully this will never be used by an npc, just covering all bases
	CLASS_ATST,
	// technically droid...
	CLASS_BARTENDER,
	CLASS_BESPIN_COP,
	CLASS_CLAW,
	CLASS_COMMANDO,
	CLASS_DESANN,
	CLASS_FISH,
	CLASS_FLIER2,
	CLASS_GALAK,
	CLASS_GLIDER,
	CLASS_GONK,
	// droid
	CLASS_GRAN,
	CLASS_HOWLER,
	CLASS_RANCOR,
	CLASS_SAND_CREATURE,
	CLASS_WAMPA,
	CLASS_IMPERIAL,
	CLASS_IMPWORKER,
	CLASS_INTERROGATOR,
	// droid
	CLASS_JAN,
	CLASS_JEDI,
	CLASS_KYLE,
	CLASS_LANDO,
	CLASS_LIZARD,
	CLASS_LUKE,
	CLASS_MARK1,
	// droid
	CLASS_MARK2,
	// droid
	CLASS_GALAKMECH,
	// droid
	CLASS_MINEMONSTER,
	CLASS_MONMOTHA,
	CLASS_MORGANKATARN,
	CLASS_MOUSE,
	// droid
	CLASS_MURJJ,
	CLASS_PRISONER,
	CLASS_PROBE,
	// droid
	CLASS_PROTOCOL,
	// droid
	CLASS_R2D2,
	// droid
	CLASS_R5D2,
	// droid
	CLASS_REBEL,
	CLASS_REBORN,
	CLASS_REELO,
	CLASS_REMOTE,
	CLASS_RODIAN,
	CLASS_SEEKER,
	// droid
	CLASS_SENTRY,
	CLASS_SHADOWTROOPER,
	CLASS_SABOTEUR,
	CLASS_STORMTROOPER,
	CLASS_SWAMP,
	CLASS_SWAMPTROOPER,
	CLASS_NOGHRI,
	CLASS_TAVION,
	CLASS_ALORA,
	CLASS_TRANDOSHAN,
	CLASS_UGNAUGHT,
	CLASS_JAWA,
	CLASS_WEEQUAY,
	CLASS_TUSKEN,
	CLASS_BOBAFETT,
	CLASS_ROCKETTROOPER,
	CLASS_SABER_DROID,
	CLASS_ASSASSIN_DROID,
	CLASS_HAZARD_TROOPER,
	CLASS_PLAYER,
	CLASS_VEHICLE,
	CLASS_SBD,
	CLASS_BATTLEDROID,
	CLASS_DROIDEKA,
	CLASS_MANDO,
	CLASS_WOOKIE,
	CLASS_CLONETROOPER,
	CLASS_STORMCOMMANDO,
	CLASS_VADER,
	CLASS_SITHLORD,
	CLASS_YODA,
	CLASS_OBJECT,

	CLASS_NUM_CLASSES
} class_t;

//==========================
//BotClass system
//==========================
typedef enum
{
	BCLASS_NONE,
	// hopefully this will never be used by a bot, just covering all bases
	BCLASS_ALORA,
	BCLASS_ASSASSIN_DROID,
	BCLASS_BARTENDER,
	BCLASS_BESPIN_COP,
	BCLASS_BOBAFETT,
	BCLASS_CHEWIE,
	BCLASS_CULTIST,
	BCLASS_CULTIST_COMMANDO,
	BCLASS_DESANN,
	BCLASS_ELDER,
	BCLASS_GALAK,
	BCLASS_GLIDER,
	BCLASS_GONK,
	// droid
	BCLASS_GRAN,
	BCLASS_HAZARDTROOPER,
	BCLASS_HOWLER,
	BCLASS_HUMAN_MERC,
	BCLASS_IMPCOMMANDER,
	BCLASS_IMPERIAL,
	BCLASS_IMPOFFICER,
	BCLASS_IMPWORKER,
	BCLASS_INTERROGATOR,
	// droid
	BCLASS_JAN,
	BCLASS_JAWA,
	BCLASS_JEDI,
	BCLASS_JEDIMASTER,
	BCLASS_JEDITRAINER,
	BCLASS_KYLE,
	BCLASS_LANDO,
	BCLASS_LUKE,
	BCLASS_MARK1,
	// droid
	BCLASS_MARK2,
	// droid
	BCLASS_GALAKMECH,
	// droid
	BCLASS_MINEMONSTER,
	BCLASS_MONMOTHA,
	BCLASS_MORGANKATARN,
	BCLASS_MOUSE,
	// droid
	BCLASS_NOGRHRI,
	BCLASS_PLAYER,
	BCLASS_PRISONER,
	BCLASS_PROBE,
	// droid
	BCLASS_PROTOCOL,
	// droid
	BCLASS_R2D2,
	// droid
	BCLASS_R5D2,
	// droid
	BCLASS_RAGNOS,
	BCLASS_RAX,
	BCLASS_REBEL,
	BCLASS_REBORN,
	BCLASS_REBORN_TWIN,
	BCLASS_REBORN_MASTER,
	BCLASS_REELO,
	BCLASS_REMOTE,
	BCLASS_ROCKETTROOPER,
	BCLASS_RODIAN,
	BCLASS_ROSH_PENIN,
	BCLASS_SABER_DROID,
	BCLASS_SABOTEUR,
	BCLASS_SEEKER,
	// droid
	BCLASS_SENTRY,
	BCLASS_SHADOWTROOPER,
	BCLASS_STORMTROOPER,
	BCLASS_SWAMPTROOPER,
	BCLASS_TAVION,
	BCLASS_TRANDOSHAN,
	BCLASS_TUSKEN_SNIPER,
	BCLASS_TUSKEN_RAIDER,
	BCLASS_UGNAUGHT,
	BCLASS_WAMPA,
	BCLASS_WEEQUAY,
	BCLASS_VEHICLE,
	//end of base classes .now some extra classes for better models taken from EoC
	BCLASS_SERENITY,
	BCLASS_CADENCE,
	BCLASS_YODA,
	BCLASS_PADAWAN,
	BCLASS_SITHLORD,
	BCLASS_LORDVADER,
	BCLASS_SITH,
	BCLASS_APPRENTICE,
	BCLASS_MANDOLORIAN,
	BCLASS_MANDOLORIAN1,
	BCLASS_MANDOLORIAN2,
	BCLASS_SOILDER,
	BCLASS_SBD,
	BCLASS_WOOKIE,
	BCLASS_TROOPER1,
	BCLASS_TROOPER2,
	BCLASS_TROOPER3,
	BCLASS_JEDIKNIGHT1,
	BCLASS_JEDIKNIGHT2,
	BCLASS_JEDIKNIGHT3,
	BCLASS_SMUGGLER1,
	BCLASS_SMUGGLER2,
	BCLASS_SMUGGLER3,
	BCLASS_JEDICONSULAR1,
	BCLASS_JEDICONSULAR2,
	BCLASS_JEDICONSULAR3,
	BCLASS_BOUNTYHUNTER1,
	BCLASS_BOUNTYHUNTER2,
	BCLASS_BOUNTYHUNTER3,
	BCLASS_SITHWORRIOR1,
	BCLASS_SITHWORRIOR2,
	BCLASS_SITHWORRIOR3,
	BCLASS_IPPERIALAGENT1,
	BCLASS_IPPERIALAGENT2,
	BCLASS_IPPERIALAGENT3,
	BCLASS_SITHINQUISITOR1,
	BCLASS_SITHINQUISITOR2,
	BCLASS_SITHINQUISITOR3,
	BCLASS_EOPIE,
	BCLASS_TAUNTAUN,
	BCLASS_SWOOP,
	BCLASS_DROIDEKA,
	BCLASS_RANCOR,
	BCLASS_DUELS,
	BCLASS_GRIEVOUS,
	BCLASS_STAFF,
	BCLASS_STAFFDARK,
	BCLASS_BATTLEDROID,
	BCLASS_STORMPILOT,
	BCLASS_WOOKIEMELEE,
	BCLASS_UNSTABLESABER,
	BCLASS_OBJECT,
	// new ones for MD
	BCLASS_OBIWAN,
	BCLASS_JANGO_NOJP,

	BCLASS_NUM_CLASSES
} bclass_t;
