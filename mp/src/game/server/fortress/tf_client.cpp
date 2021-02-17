/*
Copyright (C) Valve Corporation
Copyright (C) 2014-2019 TalonBrave.info
*/

#include "cbase.h"
#include "player.h"
#include "gamerules.h"
#include "teamplay_gamerules.h"
#include "tf_gamerules.h"
#include "entitylist.h"
#include "physics.h"
#include "game.h"
#include "ai_network.h"
#include "ai_node.h"
#include "ai_hull.h"
#include "tf_player.h"
#include "shake.h"
#include "player_resource.h"
#include "engine/IEngineSound.h"

#include "tier0/vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


void Host_Say( edict_t *pEdict, bool teamonly );

extern CBaseEntity *FindPickerEntity( CBasePlayer *pPlayer );

void Bot_RunAll( void );

extern bool			g_fGameOver;

/*	Called each time a player is spawned into the game
*/
void ClientPutInServer( edict_t *pEdict, const char *playername )
{
	// Allocate a CBaseTFPlayer for pev, and call spawn
	CBaseTFPlayer *pPlayer = CBaseTFPlayer::CreatePlayer( "player", pEdict );
	pPlayer->SetPlayerName( playername );
}

/*	Returns the descriptive name of this .dll.  E.g., Half-Life, or Team Fortress 2
*/
const char *GetGameDescription()
{
	if ( g_pGameRules ) // this function may be called before the world has spawned, and the game rules initialized
		return g_pGameRules->GetGameDescription();
	else
		return "Fortress";
}

//-----------------------------------------------------------------------------
// Purpose: Precache game-specific models & sounds
//-----------------------------------------------------------------------------
void ClientGamePrecache( void )
{
	// Materials used by the client effects
	CBaseEntity::PrecacheModel( "sprites/white.vmt" );
	CBaseEntity::PrecacheModel( "sprites/physbeam.vmt" );
	CBaseEntity::PrecacheModel( "effects/human_object_glow.vmt" );

	// Precache player models
	CBaseEntity::PrecacheModel("models/player/alien_commando.mdl");
	CBaseEntity::PrecacheModel("models/player/human_commando.mdl");
	CBaseEntity::PrecacheModel("models/player/alien_defender.mdl");
	CBaseEntity::PrecacheModel("models/player/human_defender.mdl");
	CBaseEntity::PrecacheModel("models/player/alien_medic.mdl");
	CBaseEntity::PrecacheModel("models/player/human_medic.mdl");
	CBaseEntity::PrecacheModel("models/player/alien_recon.mdl");
	CBaseEntity::PrecacheModel("models/player/human_recon.mdl");
	CBaseEntity::PrecacheModel("models/player/alien_escort.mdl");
	CBaseEntity::PrecacheModel("models/player/human_escort.mdl");
	CBaseEntity::PrecacheModel("models/player/alien_defender.mdl");
	CBaseEntity::PrecacheModel("models/player/human_defender.mdl");
	CBaseEntity::PrecacheModel("models/player/alien_technician.mdl");
	CBaseEntity::PrecacheModel("models/player/human_technician.mdl");
	CBaseEntity::PrecacheModel("models/player/alien_infiltrator.mdl");
	CBaseEntity::PrecacheModel("models/player/human_infiltrator.mdl");
	CBaseEntity::PrecacheModel("models/player/alien_sapper.mdl");
	CBaseEntity::PrecacheModel("models/player/human_sapper.mdl");
	CBaseEntity::PrecacheModel("models/player/alien_support.mdl");
	CBaseEntity::PrecacheModel("models/player/human_support.mdl");
	CBaseEntity::PrecacheModel("models/player/alien_sniper.mdl");
	CBaseEntity::PrecacheModel("models/player/human_sniper.mdl");
	CBaseEntity::PrecacheModel("models/player/alien_pyro.mdl");
	CBaseEntity::PrecacheModel("models/player/human_pyro.mdl");

	// VGUI assets
	CBaseEntity::PrecacheModel("models/interface/red_team.mdl");
	CBaseEntity::PrecacheModel("models/interface/blue_team.mdl");
	CBaseEntity::PrecacheModel("models/interface/random.mdl");

	// Precache team message sounds
	enginesound->PrecacheSound( "vox/reinforcement.wav" );
	enginesound->PrecacheSound( "vox/harvester-attack.wav" );
	enginesound->PrecacheSound( "vox/harvester-destroyed.wav" );
	enginesound->PrecacheSound( "vox/new-tech-level.wav" );
	enginesound->PrecacheSound( "vox/resource-zone-emptied.wav" );

	// HOGSY ADDITIONS START
	CBaseEntity::PrecacheScriptSound("ResourceChunk.Pickup");
	CBaseEntity::PrecacheScriptSound("WeaponObjectSapper.Attach");
	CBaseEntity::PrecacheScriptSound("WeaponObjectSapper.AttachFail");
	CBaseEntity::PrecacheScriptSound("GrenadeObjectSapper.Arming");
	CBaseEntity::PrecacheScriptSound("GrenadeObjectSapper.RemoveSapper");
	CBaseEntity::PrecacheScriptSound("GasolineBlob.FlameSound");
	CBaseEntity::PrecacheScriptSound("Humans.Death");
	CBaseEntity::PrecacheScriptSound("Humans.Pain");
	
	PrecacheMaterial("cable/human_powercable.vmt");
	// HOGSY ADDITIONS END
}


// called by ClientKill and DeadThink
void respawn( CBaseEntity *pEdict, bool fCopyCorpse )
{
	CBaseTFPlayer *pPlayer = dynamic_cast<CBaseTFPlayer*>(pEdict);
	if (pPlayer == nullptr)
		return;

	// If it's not a multiplayer game, reload
	if ( !gpGlobals->deathmatch ) {
		engine->ServerCommand( "reload\n" );
		return;
	}

	if ( fCopyCorpse ) {
		// make a copy of the dead body for appearances sake
		pPlayer->CreateCorpse();
	}

	pPlayer->Spawn();
}

void GameStartFrame( void )
{
	VPROF("GameStartFrame()");

	if ( g_pGameRules )
		g_pGameRules->Think();

	if ( g_fGameOver )
		return;

	gpGlobals->teamplay = teamplay.GetInt() ? true : false;

	Bot_RunAll();
}

//=========================================================
// instantiate the proper game rules object
//=========================================================
void InstallGameRules()
{
	// Create the player resource
	g_pPlayerResource = (CPlayerResource*)CBaseEntity::Create( "tf_player_manager", vec3_origin, vec3_angle );

	// teamplay
	CreateGameRulesObject( "CTeamFortress" );
}

void ClientActive( edict_t *pEdict, bool bLoadGame )
{
	// Can't load games in CS!
	Assert( !bLoadGame );

	CBaseTFPlayer *pPlayer = static_cast<CBaseTFPlayer*>( CBaseEntity::Instance( pEdict ) );
	if(!pPlayer)
		return;

	pPlayer->InitialSpawn();

	char sName[128];
	V_strncpy(sName, pPlayer->GetPlayerName(), sizeof(sName));

	// First parse the name and remove any %'s
	for (char *pApersand = sName; pApersand != NULL && *pApersand != 0; pApersand++)
	{
		// Replace it with a space
		if (*pApersand == '%')
			*pApersand = ' ';
	}

	// notify other clients of player joining the game
	UTIL_ClientPrintAll(HUD_PRINTNOTIFY, "#Game_connected", sName[0] != 0 ? sName : "<unconnected>");

	pPlayer->Spawn();
}
