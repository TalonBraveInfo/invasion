#include "cbase.h"
#include "tf_gamerules.h"
#include "tf_shareddefs.h"
#include "ammodef.h"
#include "basetfcombatweapon_shared.h"

#ifdef CLIENT_DLL
	#include "c_shield.h"
	#include "c_te_effect_dispatch.h"
	#define CShield C_Shield
#else
	#include "tf_shield.h"
	#include "te_effect_dispatch.h"
	#include "player.h"
	#include "tf_player.h"
	#include "game.h"
	#include "gamerules.h"
	#include "teamplay_gamerules.h"
	#include "ammodef.h"
	#include "techtree.h"
	#include "tf_team.h"
	#include "tf_shield.h"
	#include "mathlib/mathlib.h"
	#include "entitylist.h"
	#include "basecombatweapon.h"
	#include "voice_gamemgr.h"
	#include "tf_class_infiltrator.h"
	#include "team_messages.h"
	#include "ndebugoverlay.h"
	#include "bot_base.h"
	#include "vstdlib/random.h"
	#include "info_act.h"
	#include "igamesystem.h"
	#include "filesystem.h"
	#include "info_vehicle_bay.h"
	#include "iservervehicle.h"
	#include "weapon_builder.h"
	#include "weapon_objectselection.h"
	#include "ndebugoverlay.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

REGISTER_GAMERULES_CLASS(CTeamFortress);

//IMPLEMENT_NETWORKCLASS_ALIASED( TeamFortress, DT_TeamFortress )

BEGIN_NETWORK_TABLE_NOBASE( CTeamFortress, DT_TeamFortress )
END_NETWORK_TABLE()

#ifndef CLIENT_DLL
   
	#define MAX_OBJECT_COMMAND_DISTANCE 120.0f

	class CVoiceGameMgrHelper : public IVoiceGameMgrHelper
	{
	public:
		virtual bool CanPlayerHearPlayer( CBasePlayer *pListener, CBasePlayer *pTalker, bool &bProximity )
		{
			// Gagged players can't talk at all
			if ( ((CBaseTFPlayer*)pTalker)->CanSpeak() == false )
				return false;

			// Dead players can only be heard by other dead team mates
			if ( pTalker->IsAlive() == false )
			{
				if ( pListener->IsAlive() == false )
					return ( pListener->InSameTeam( pTalker ) );

				return false;
			}

			return ( pListener->InSameTeam( pTalker ) );
		}
	};
	CVoiceGameMgrHelper g_VoiceGameMgrHelper;
	IVoiceGameMgrHelper *g_pVoiceGameMgrHelper = &g_VoiceGameMgrHelper;


	// Load the objects.txt file.
	class CObjectsFileLoad : public CAutoGameSystem
	{
	public:
		virtual bool Init()
		{
			LoadObjectInfos( filesystem );
			return true;
		}
	} g_ObjectsFileLoad;



	extern bool		g_fGameOver;
	float			g_flNextReinforcementTime = 0.0f;
	extern ConVar	tf_knockdowntime;

	// Time between reinforcements
	#define REINFORCEMENT_TIME		15.0

	ConVar    sk_plr_dmg_grenade		( "sk_plr_dmg_grenade","0");

	char *sTeamNames[] =
	{
		"Unassigned",
		"Spectator",
		"Human",
		"Alien",
	};

	// Handle the "PossessBot" command.
	CON_COMMAND_F(PossessBot, "Toggle. Possess a bot.\n\tArguments: <bot client number>", FCVAR_CHEAT)
	{
		CBaseTFPlayer *pPlayer = CBaseTFPlayer::Instance( UTIL_GetCommandClientIndex() );
		if ( !pPlayer )
			return;

		// Put the local player in control of this bot.
		if ( args.ArgC() != 2 )
		{
			Warning( "PossessBot <client index>\n" );
			return;
		}

		int iBotClient = atoi( args[1] );
		int iBotEnt = iBotClient + 1;
		
		if ( iBotClient < 0 || 
			iBotClient >= gpGlobals->maxClients || 
			pPlayer->entindex() == iBotEnt )
			Warning( "PossessBot <client index>\n" );
		else
		{
			edict_t *pPlayerData = pPlayer->edict();
			edict_t *pBotData = engine->PEntityOfEntIndex( iBotEnt );
			if ( pBotData && pBotData->GetUnknown() )
			{
				// SWAP EDICTS

				// Backup things we don't want to swap.
				edict_t oldPlayerData = *pPlayerData;
				edict_t oldBotData = *pBotData;

				// Swap edicts.
				edict_t tmp = *pPlayerData;
				*pPlayerData = *pBotData;
				*pBotData = tmp;

				// Restore things we didn't want to swap.
				//pPlayerData->m_EntitiesTouched = oldPlayerData.m_EntitiesTouched;
				//pBotData->m_EntitiesTouched = oldBotData.m_EntitiesTouched;
			
				CBaseEntity *pPlayerBaseEnt = CBaseEntity::Instance( pPlayerData );
				CBaseEntity *pBotBaseEnt = CBaseEntity::Instance( pBotData );

				// Make the other a bot and make the player not a bot.
				pPlayerBaseEnt->RemoveFlag( FL_FAKECLIENT );
				pBotBaseEnt->AddFlag( FL_FAKECLIENT );
							
				// Point the CBaseEntities at the right players.
				pPlayerBaseEnt->NetworkProp()->SetEdict( pPlayerData );
				pBotBaseEnt->NetworkProp()->SetEdict( pBotData );

				// Freeze the bot.
				pBotBaseEnt->AddEFlags( EFL_BOT_FROZEN );

				// Remove orders to both of them..
				CTFTeam *pTeam = pPlayer->GetTFTeam();
				if ( pTeam )
				{
					pTeam->RemoveOrdersToPlayer( (CBaseTFPlayer*)pPlayerBaseEnt );
					pTeam->RemoveOrdersToPlayer( (CBaseTFPlayer*)pBotBaseEnt );
				}
			}
		}
	}

	// Handler for the "bot" command.
	CON_COMMAND_F(bot, "Add a bot.", FCVAR_CHEAT)
	{
		CBaseTFPlayer *pPlayer = CBaseTFPlayer::Instance( UTIL_GetCommandClientIndex() );

		// The bot command uses switches like command-line switches.
		// -count <count> tells how many bots to spawn.
		// -team <index> selects the bot's team. Default is -1 which chooses randomly.
		//	Note: if you do -team !, then it 
		// -class <index> selects the bot's class. Default is -1 which chooses randomly.
		// -frozen prevents the bots from running around when they spawn in.

		// Look at -count.
		int count = args.FindArgInt( "-count", 1 );
		count = clamp( count, 1, 16 );

		int iTeam = -1;
		const char *pVal = args.FindArg( "-team" );
		if ( pVal )
		{
			if ( pVal[0] == '!' )
				iTeam = pPlayer->GetTFTeam()->GetEnemyTeam()->GetTeamNumber();
			else
			{
				iTeam = atoi( pVal );
				iTeam = clamp( iTeam, 0, GetNumberOfTeams() );
			}
		}

		int iClass = args.FindArgInt( "-class", -1 );
		iClass = clamp( iClass, -1, TFCLASS_CLASS_COUNT );
		if ( iClass == TFCLASS_UNDECIDED )
			iClass = TFCLASS_RECON;
		
		// Look at -frozen.
		bool bFrozen = !!args.FindArg( "-frozen" );
			
		// Ok, spawn all the bots.
		while ( --count >= 0 )
			BotPutInServer( bFrozen, iTeam, iClass );
	}

	bool IsSpaceEmpty( CBaseEntity *pMainEnt, const Vector &vMin, const Vector &vMax )
	{
		Vector vHalfDims = ( vMax - vMin ) * 0.5f;
		Vector vCenter = vMin + vHalfDims;

		trace_t trace;
		UTIL_TraceHull( vCenter, vCenter, -vHalfDims, vHalfDims, MASK_SOLID, pMainEnt, COLLISION_GROUP_NONE, &trace );

		bool bClear = ( trace.fraction == 1 && trace.allsolid != 1 && (trace.startsolid != 1) );
		return bClear;
	}


	Vector MaybeDropToGround( 
		CBaseEntity *pMainEnt, 
		bool bDropToGround, 
		const Vector &vPos, 
		const Vector &vMins, 
		const Vector &vMaxs )
	{
		if ( bDropToGround )
		{
			trace_t trace;
			UTIL_TraceHull( vPos, vPos + Vector( 0, 0, -500 ), vMins, vMaxs, MASK_SOLID, pMainEnt, COLLISION_GROUP_NONE, &trace );
			return trace.endpos;
		}
		else
		{
			return vPos;
		}
	}


	//-----------------------------------------------------------------------------
	// Purpose: This function can be used to find a valid placement location for an entity.
	//			Given an origin to start looking from and a minimum radius to place the entity at,
	//			it will sweep out a circle around vOrigin and try to find a valid spot (on the ground)
	//			where mins and maxs will fit.
	// Input  : *pMainEnt - Entity to place
	//			&vOrigin - Point to search around
	//			fRadius - Radius to search within
	//			nTries - Number of tries to attempt
	//			&mins - mins of the Entity
	//			&maxs - maxs of the Entity
	//			&outPos - Return point
	// Output : Returns true and fills in outPos if it found a spot.
	//-----------------------------------------------------------------------------
	bool EntityPlacementTest( CBaseEntity *pMainEnt, const Vector &vOrigin, Vector &outPos, bool bDropToGround )
	{
		// This function moves the box out in each dimension in each step trying to find empty space like this:
		//
		//											  X  
		//							   X			  X  
		// Step 1:   X     Step 2:    XXX   Step 3: XXXXX
		//							   X 			  X  
		//											  X  
		//
			 
		Vector mins, maxs;
		pMainEnt->CollisionProp()->WorldSpaceAABB( &mins, &maxs );
		mins -= pMainEnt->GetAbsOrigin();
		maxs -= pMainEnt->GetAbsOrigin();

		// Put some padding on their bbox.
		float flPadSize = 5;
		Vector vTestMins = mins - Vector( flPadSize, flPadSize, flPadSize );
		Vector vTestMaxs = maxs + Vector( flPadSize, flPadSize, flPadSize );

		// First test the starting origin.
		if ( IsSpaceEmpty( pMainEnt, vOrigin + vTestMins, vOrigin + vTestMaxs ) )
		{
			outPos = MaybeDropToGround( pMainEnt, bDropToGround, vOrigin, vTestMins, vTestMaxs );
			return true;
		}

		Vector vDims = vTestMaxs - vTestMins;


		// Keep branching out until we get too far.
		int iCurIteration = 0;
		int nMaxIterations = 15;
		
		int offset = 0;
		do
		{
			for ( int iDim=0; iDim < 3; iDim++ )
			{
				float flCurOffset = offset * vDims[iDim];

				for ( int iSign=0; iSign < 2; iSign++ )
				{
					Vector vBase = vOrigin;
					vBase[iDim] += (iSign*2-1) * flCurOffset;
				
					if ( IsSpaceEmpty( pMainEnt, vBase + vTestMins, vBase + vTestMaxs ) )
					{
						// Ensure that there is a clear line of sight from the spawnpoint entity to the actual spawn point.
						// (Useful for keeping things from spawning behind walls near a spawn point)
						trace_t tr;
						UTIL_TraceLine( vOrigin, vBase, MASK_SOLID, pMainEnt, COLLISION_GROUP_NONE, &tr );

						if ( tr.fraction != 1.0 )
						{
							continue;
						}

						outPos = MaybeDropToGround( pMainEnt, bDropToGround, vBase, vTestMins, vTestMaxs );
						return true;
					}
				}
			}

			++offset;
		} while ( iCurIteration++ < nMaxIterations );

	//	Warning( "EntityPlacementTest for ent %d:%s failed!\n", pMainEnt->entindex(), pMainEnt->GetClassname() );
		return false;
	}


	//-----------------------------------------------------------------------------
	// Purpose: 
	//-----------------------------------------------------------------------------
	CBaseEntity *CTeamFortress::GetPlayerSpawnSpot( CBasePlayer *pPlayer )
	{
		CBaseEntity *pSpawnSpot = pPlayer->EntSelectSpawnPoint();

		// Make sure the spawn spot isn't blocked...
		Vector vecTestOrg = pSpawnSpot->GetAbsOrigin();
		Vector origin;
		if (!EntityPlacementTest(pPlayer, vecTestOrg, origin, true)) {
			origin = pSpawnSpot->GetAbsOrigin() + Vector( 0, 5.0f, 0 );
			UTIL_DropToFloor(pPlayer, MASK_SOLID);
		}
		
		// Move the player to the place it said.
		pPlayer->Teleport(&origin, &pSpawnSpot->GetLocalAngles(), &vec3_origin);
		pPlayer->m_Local.m_vecPunchAngle = vec3_angle;
		
		return pSpawnSpot;
	}

	CTeamFortress::CTeamFortress()
	{
		m_bAllowWeaponSwitch = true;

		// Create the team managers
		for ( int i = 0; i < MAX_TF_TEAMS; i++ )
		{
			CTFTeam *pTeam = (CTFTeam*)CreateEntityByName( "tf_team_manager" );
			pTeam->Init( sTeamNames[i], i );

			g_Teams.AddToTail( pTeam );
		}

		// Create the hint manager
		CBaseEntity::Create( "tf_hintmanager", vec3_origin, vec3_angle );
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	//-----------------------------------------------------------------------------
	CTeamFortress::~CTeamFortress()
	{
		// Note, don't delete each team since they are in the gEntList and will 
		// automatically be deleted from there, instead.
		g_Teams.Purge();
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	//-----------------------------------------------------------------------------
	void CTeamFortress::UpdateClientData( CBasePlayer *player )
	{
	}

	//-----------------------------------------------------------------------------
	// Purpose: Called after every level and load change
	//-----------------------------------------------------------------------------
	void CTeamFortress::LevelInitPostEntity()
	{
		g_flNextReinforcementTime = gpGlobals->curtime + REINFORCEMENT_TIME;
		BaseClass::LevelInitPostEntity();
	}

	//-----------------------------------------------------------------------------
	// Purpose: The gamerules think function
	//-----------------------------------------------------------------------------
	void CTeamFortress::Think( void )
	{
		BaseClass::Think();

		// Check the reinforcement time
		if ( g_flNextReinforcementTime <= gpGlobals->curtime )
		{
			//Msg( "Reinforcement Tick\n" );

			// Reinforce any dead players
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBaseTFPlayer *pPlayer = ToBaseTFPlayer( UTIL_PlayerByIndex(i) );
				if ( pPlayer )
				{
					// Ready to respawn?
					if ( pPlayer->IsReadyToReinforce() )
					{
						pPlayer->Reinforce();
						//pPlayer->GetTFTeam()->PostMessage( TEAMMSG_REINFORCEMENTS_ARRIVED );
					}
				}
			}

			g_flNextReinforcementTime += REINFORCEMENT_TIME;
		}

		// Tell each Team to think
		for ( int i = 0; i < GetNumberOfTeams(); i++ )
			GetGlobalTeam( i )->Think();
	}

	//-----------------------------------------------------------------------------
	// Purpose: Player has just left the game
	//-----------------------------------------------------------------------------
	void CTeamFortress::ClientDisconnected( edict_t *pClient )
	{
		CBaseTFPlayer *pPlayer = (CBaseTFPlayer *)CBaseEntity::Instance( pClient );
		if ( pPlayer )
		{
			// Tell all orders that this player's left
			COrderEvent_PlayerDisconnected order( pPlayer );
			GlobalOrderEvent( &order );

			// Delete this player's playerclass
			pPlayer->ClearPlayerClass();
		}

		BaseClass::ClientDisconnected( pClient );
	}

	//-----------------------------------------------------------------------------
	// Purpose: TF2 Specific Client Commands
	// Input  :
	// Output :
	//-----------------------------------------------------------------------------
	bool CTeamFortress::ClientCommand(CBaseEntity *pEdict, const CCommand &args)
	{
		CBaseTFPlayer *pPlayer = (CBaseTFPlayer *)pEdict;

		const char *pcmd = args[0];
		if ( FStrEq( pcmd, "objcmd" ) )
		{
			if ( args.ArgC() < 3 )
				return true;

			int entindex = atoi( args[1] );
			edict_t* pEdict = INDEXENT(entindex);
			if (pEdict)
			{
				CBaseEntity* pBaseEntity = GetContainingEntity(pEdict);
				CBaseObject* pObject = dynamic_cast<CBaseObject*>(pBaseEntity);
				if (pObject && pObject->InSameTeam(pPlayer))
				{
					// We have to be relatively close to the object too...

					// FIXME: When I put in a better dismantle solution (namely send the dismantle 
					// command along with a cancledismantle command), re-enable this.
					// Also, need to solve the problem of control panels on large objects
					// For the battering ram, for instance, this distance is too far.

	//				float flDistSq = pObject->GetAbsOrigin().DistToSqr( pPlayer->GetAbsOrigin() );
	//				if (flDistSq <= (MAX_OBJECT_COMMAND_DISTANCE * MAX_OBJECT_COMMAND_DISTANCE))
					{
						CCommand objectArgs( args.ArgC() - 2, &args.ArgV()[2]);
						pObject->ClientCommand(pPlayer, objectArgs);
					}
				}
			}

			return true;
		}

		if ( FStrEq( pcmd, "buildvehicle" ) )
		{
			if ( args.ArgC() < 3 )
				return true;

			int entindex = atoi( args[1] );
			int ivehicle = atoi( args[2] );
			edict_t *pEdict = INDEXENT(entindex);
			if (pEdict)
			{
				CBaseEntity *pBaseEntity = GetContainingEntity(pEdict);
				CVGuiScreenVehicleBay *pBayScreen = dynamic_cast<CVGuiScreenVehicleBay*>(pBaseEntity);
				if ( pBayScreen && pBayScreen->InSameTeam(pPlayer) )
				{
					// Need the same logic as objcmd above to ensure the player's near the vehicle bay vgui screen
					pBayScreen->BuildVehicle( pPlayer, ivehicle );
				}
			}

			return true;
		}

		// TF Commands
		if ( FStrEq( pcmd, "changeclass" ) ) {
			// TODO: revisit this...

			if ( args.ArgC() < 2 ) {
				return true;
			}

			int newClass = atoi( args.Arg( 1 ) );
			int oldClass = pPlayer->PlayerClass();
			if ( newClass == oldClass ) {
				// Nothing more to do, we're already that class.
				return true;
			}

			pPlayer->HideViewModels();

			// Random class selection.
			if ( newClass <= -1 ) {
				newClass = random->RandomInt( TFCLASS_RECON, TFCLASS_CLASS_COUNT - 1 );
			}

			// Why are we doing this before we're dead? Because then we can't pick up our new items.
			pPlayer->ChangeClass( ( TFClass ) newClass );
			return true;
		} else if ( FStrEq( pcmd, "changeteam" ) ) {
			if ( args.ArgC() < 2 ) {
				return true;
			}

			int newTeam = atoi( args.Arg( 1 ) );
			int oldTeam = pPlayer->GetTeamNumber();
			if ( newTeam == oldTeam ) {
				// Nothing more to do, we're already that team.
				return true;
			}

			pPlayer->HideViewModels();
			
			// Automatic team selection.
			if ( newTeam <= -1 ) {
				pPlayer->PlacePlayerInTeam();
			} else {
				// Otherwise throw us into our selected team.
				pPlayer->ChangeTeam( newTeam );
			}

			// Don't commit suicide unless we're already in a team.
			if ( !pPlayer->IsDead() && oldTeam != TEAM_UNASSIGNED ) {
				pPlayer->CommitSuicide( false, true );
				pPlayer->IncrementFragCount( 1 );
			} else {
				pPlayer->ForceRespawn();
			}

			return true;
		}
		else if ( FStrEq( pcmd, "tactical" ) )
		{
			bool bTactical = args[1][0] == '!' ? !pPlayer->GetLocalData()->m_nInTacticalView : (atoi( args[1] ) ? true : false);

			pPlayer->ShowTacticalView( bTactical );
			return true;
		}
		else if ( FStrEq( pcmd, "tech" ) )
		{
			CTFTeam *pTFTeam = pPlayer->GetTFTeam();
			if ( !pTFTeam )
				return true;

			if ( args.ArgC() == 2 )
			{
				const char *name = args[1];

				CBaseTechnology *tech = pTFTeam->m_pTechnologyTree->GetTechnology( name );
				if ( tech )
					pTFTeam->EnableTechnology( tech );
			}
			else
				Msg( "usage:  tech <name>\n" );

			return true;	   
		}
		else if ( FStrEq( pcmd, "techall" ) )
		{
			if ( pPlayer->GetTFTeam() )
				pPlayer->GetTFTeam()->EnableAllTechnologies();

			return true;
		}
		else if ( FStrEq( pcmd, "tank" ) )
			CBaseEntity::Create( "tank", pPlayer->WorldSpaceCenter(), pPlayer->GetLocalAngles() );
		else if ( FStrEq( pcmd, "addres" ) || FStrEq( pcmd, "ar" ) )
		{
			if ( args.ArgC() == 3 )
			{
				int team = atoi( args[1] );
				float flResourceAmount = atof( args[2] );
				if ( team > 0 && team <= GetNumberOfTeams() ) 
					GetGlobalTFTeam( team )->AddTeamResources( flResourceAmount );
			}
			else
				Msg( "usage:  ar <team 1 : 2> <amount>\n" );

			return true;
		}
		else if ( FStrEq( pcmd, "preftech" ) )
		{
			CTFTeam *pTFTeam = pPlayer->GetTFTeam();
			if ( !pTFTeam )
				return true;

			if ( args.ArgC() == 2 )
			{
				int iPrefTechIndex = atoi( args[1] );

				pPlayer->SetPreferredTechnology( pTFTeam->m_pTechnologyTree, iPrefTechIndex );		
			}

			return true;
		}
		else if( FStrEq( pcmd, "decaltest" ) )
		{
			trace_t trace;
			int entityIndex;
			Vector vForward;

			AngleVectors( pEdict->GetAbsAngles(), &vForward, NULL, NULL );

			UTIL_TraceLine( pEdict->GetAbsOrigin(), pEdict->GetAbsOrigin() + vForward * 10000,  MASK_SOLID_BRUSHONLY, pEdict, COLLISION_GROUP_NONE, &trace );

			entityIndex = trace.GetEntityIndex();

			int id = UTIL_PrecacheDecal( "decals/tscorch", true );
			CBroadcastRecipientFilter filter;
			te->BSPDecal( filter, 0.0, 
				&trace.endpos, entityIndex, id );

			return true; 
		}
		else if( FStrEq( pcmd, "killorder" ) )
		{
			if( pPlayer->GetTFTeam() )
				pPlayer->GetTFTeam()->RemoveOrdersToPlayer( pPlayer );
		
			return true;
		}
		else if( BaseClass::ClientCommand( pEdict, args ) )
			return true;
		else
			return pPlayer->ClientCommand(args);

		return false;
	}

	//-----------------------------------------------------------------------------
	// Purpose: Player has just spawned. Equip them.
	//-----------------------------------------------------------------------------
	void CTeamFortress::PlayerSpawn( CBasePlayer *pPlayer )
	{
		pPlayer->EquipSuit();
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	//-----------------------------------------------------------------------------
	bool CTeamFortress::PlayFootstepSounds( CBasePlayer *pl )
	{
		if ( footsteps.GetInt() == 0 )
			return false;

		CBaseTFPlayer *tfPlayer = static_cast< CBaseTFPlayer * >( pl );
		if ( tfPlayer )
			if ( tfPlayer->IsKnockedDown() )
				return false;

		// only make step sounds in multiplayer if the player is moving fast enough
		if ( pl->IsOnLadder() || pl->GetAbsVelocity().Length2D() > 100 )
			return true;  

		return false;
	}

	//-----------------------------------------------------------------------------
	// Purpose: Remove falling damage for jetpacking recons
	//-----------------------------------------------------------------------------
	float CTeamFortress::FlPlayerFallDamage( CBasePlayer *pPlayer )
	{
		int iFallDamage = (int)falldamage.GetFloat();

		CBaseTFPlayer *pTFPlayer = (CBaseTFPlayer *)pPlayer;
		if ( pTFPlayer->IsClass( TFCLASS_RECON ) )
			return 0;

		switch ( iFallDamage )
		{
		case 1://progressive
			pPlayer->m_Local.m_flFallVelocity -= PLAYER_MAX_SAFE_FALL_SPEED;
			return pPlayer->m_Local.m_flFallVelocity * DAMAGE_FOR_FALL_SPEED;
			break;
		default:
		case 0:// fixed
			return 10;
			break;
		}
	}


	//-----------------------------------------------------------------------------
	// Is the ray blocked by enemy shields?
	//-----------------------------------------------------------------------------
	bool CTeamFortress::IsBlockedByEnemyShields( const Vector& src, const Vector& end, int nFriendlyTeam )
	{
		// Iterate over all shields on the same team, disable them so
		// we don't intersect with them...
		CShield::ActivateShields( false, nFriendlyTeam );

		bool bBlocked = CShield::IsBlockedByShields( src, end );

		CShield::ActivateShields( true, nFriendlyTeam );

		return bBlocked;
	}


	//-----------------------------------------------------------------------------
	// Traces a line vs a shield, returns damage reduction
	//-----------------------------------------------------------------------------
	float CTeamFortress::WeaponTraceEntity( CBaseEntity *pEntity, 
		const Vector &src, const Vector &end, unsigned int mask, trace_t *pTrace )
	{
		int damageType = pEntity->GetDamageType();

		// Iterate over all shields on the same team, disable them so
		// we don't intersect with them...
		CShield::ActivateShields( false, pEntity->GetTeamNumber() );

		// Trace it baby...
		float damage = 1.0f;
		bool done;
		do 
		{
			// FIXME: Optimize so we don't test the same ray but start at the
			// previous collision point
			done = true;
			UTIL_TraceEntity( pEntity, src, end, mask, pTrace );

			// Shield check...
			if (pTrace->fraction != 1.0)
			{
				CBaseEntity *pCollidedEntity = pTrace->m_pEnt;

				// Did we hit a shield?
				CShield* pShield = dynamic_cast<CShield*>(pCollidedEntity);
				if (pShield)
				{
					Vector vecDir;
					VectorSubtract( end, src, vecDir );

					// Let's see if we let this damage type through...
					if (pShield->ProtectionAmount( damageType ) == 1.0f)
					{
						// We deflected all of the damage
						pShield->RegisterDeflection( vecDir, damageType, pTrace );
						damage = 0.0f;
					}
					else
					{
						// We deflected part of the damage, but we need to trace again
						// only this time we can't let the shield register a collision
						damage *= 1.0f - pShield->ProtectionAmount( damageType );

						// FIXME: DMG_BULLET should be something else
						pShield->RegisterPassThru( vecDir, damageType, pTrace );
						pShield->ActivateCollisions( false );

						done = false;
					}
				}
			}
		}
		while (!done);

		// Reduce the damage dealt... but don't worry about if if the
		// shield actually deflected it. In that case, we actually want
		// explosive things to explode at full blast power. The shield will prevent
		// the blast damage to things behind the shield
		if (damage != 0.0)
			pEntity->SetDamage(pEntity->GetDamage() * damage);

		// Reactivate all shields
		CShield::ActivateShields( true );

		return damage;
	}

	//-----------------------------------------------------------------------------
	// Purpose: Is trace blocked by a world or a shield?
	//-----------------------------------------------------------------------------
	bool CTeamFortress::IsTraceBlockedByWorldOrShield( const Vector& src, const Vector& end, CBaseEntity *pShooter, int damageType, trace_t* pTrace )
	{
		// Iterate over all shields on the same team, disable them so
		// we don't intersect with them...
		CShield::ActivateShields( false, pShooter->GetTeamNumber() );

		//NDebugOverlay::Line( src, pTrace->endpos, 255,255,255, true, 5.0 );
		//NDebugOverlay::Box( pTrace->endpos, Vector(-2,-2,-2), Vector(2,2,2), 255,255,255, true, 5.0 );

		// Now make sure there isn't something other than team players in the way.
		class CShieldWorldFilter : public CTraceFilterSimple
		{
		public:
			CShieldWorldFilter( CBaseEntity *pShooter ) : CTraceFilterSimple( pShooter, TFCOLLISION_GROUP_WEAPON )
			{
			}

			virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
			{
				CBaseEntity *pEnt = static_cast<CBaseEntity*>(pHandleEntity);

				// Did we hit a brushmodel?
				if ( pEnt->GetSolid() == SOLID_BSP )
					return true;

				// Ignore collisions with everything but shields
				if ( pEnt->GetCollisionGroup() != TFCOLLISION_GROUP_SHIELD )
					return false;

				return CTraceFilterSimple::ShouldHitEntity( pHandleEntity, contentsMask );
			}
		};

		trace_t tr;
		CShieldWorldFilter shieldworldFilter( pShooter );
		UTIL_TraceLine( src, end, MASK_SOLID, &shieldworldFilter, pTrace );

		// Shield check...
		if (pTrace->fraction != 1.0)
		{
			CBaseEntity *pEntity = pTrace->m_pEnt;
			CShield* pShield = dynamic_cast<CShield*>(pEntity);
			if (pShield)
			{
				Vector vecDir;
				VectorSubtract( end, src, vecDir );

				// We deflected all of the damage
				pShield->RegisterDeflection( vecDir, damageType, pTrace );
			}
		}

		// Reactivate all shields
		CShield::ActivateShields( true );

		return ( pTrace->fraction < 1.0 );
	}

	//-----------------------------------------------------------------------------
	// Default implementation of radius damage
	//-----------------------------------------------------------------------------
	void CTeamFortress::RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrcIn, float flRadius, int iClassIgnore )
	{
		CBaseEntity *pEntity = NULL;
		trace_t		tr;
		float		flAdjustedDamage, falloff;
		Vector		vecSpot;
		Vector vecSrc = vecSrcIn;

		if ( flRadius )
			falloff = info.GetDamage() / flRadius;
		else
			falloff = 1.0;

		int bInWater = (UTIL_PointContents ( vecSrc ) & MASK_WATER) ? true : false;

		// in case grenade is lying on the ground
		// Is this even needed anymore? Grenades already jump up in their explode code...
		vecSrc.z += 1;

		// iterate on all entities in the vicinity.
		for ( CEntitySphereQuery sphere( vecSrc, flRadius ); ; sphere.NextEntity() )
		{
			pEntity = sphere.GetCurrentEntity();
			if (!pEntity)
				break;

			if ( pEntity->m_takedamage != DAMAGE_NO )
			{
				// UNDONE: this should check a damage mask, not an ignore
				if ( iClassIgnore != CLASS_NONE && pEntity->Classify() == iClassIgnore )
					continue;

				// blast's don't tavel into or out of water
				if (bInWater && pEntity->GetWaterLevel() == 0)
					continue;
				if (!bInWater && pEntity->GetWaterLevel() == 3)
					continue;

				// Copy initial values out of the info
				CTakeDamageInfo subInfo = info;
				if ( !subInfo.GetAttacker() )
					subInfo.SetAttacker( subInfo.GetInflictor() );

				// Don't bother with hitboxes on this test
				vecSpot = pEntity->WorldSpaceCenter( );
				WeaponTraceLine ( vecSrc, vecSpot, MASK_SHOT & (~CONTENTS_HITBOX), subInfo.GetInflictor(), subInfo.GetDamageType(), &tr );
				if ( tr.fraction != 1.0 && tr.m_pEnt != pEntity )
					continue;

				// We're going to need to see if it actually hit a shield
				// the explosion can 'see' this entity, so hurt them!
				if (tr.startsolid)
				{
					// if we're stuck inside them, fixup the position and distance
					tr.endpos = vecSrc;
					tr.fraction = 0.0;
				}
				
				// decrease damage for an ent that's farther from the bomb.
				flAdjustedDamage = ( vecSrc - tr.endpos ).Length() * falloff;
				flAdjustedDamage = subInfo.GetDamage() - flAdjustedDamage;
				if ( flAdjustedDamage > 0 )
				{
					// Knockdown
					// For now, just use damage. Eventually we should do it on a per-weapon basis.
					/*
					if ( pEntity->IsPlayer() && flAdjustedDamage > 40 )
					{
						Vector vecForce = vecSpot - vecSrc;
						// Reduce the Z component and increase the X,Y
						vecForce.x *= 3.0;
						vecForce.y *= 3.0;
						vecForce.z *= 0.5;
						VectorNormalize( vecForce );
						((CBaseTFPlayer*)pEntity)->KnockDownPlayer( vecForce, flAdjustedDamage * 15.0f, tf_knockdowntime.GetFloat() );
					}
					*/
					
					// Msg( "hit %s\n", pEntity->GetClassname() );
					subInfo.SetDamage( flAdjustedDamage );

					Vector dir = tr.endpos - vecSrc;
					if ( VectorNormalize( dir ) == 0 )
					{
						dir = vecSpot - vecSrc;
						VectorNormalize( dir );
					}

					// If we don't have a damage force, manufacture one
					if ( subInfo.GetDamagePosition() == vec3_origin || subInfo.GetDamageForce() == vec3_origin )
						CalculateExplosiveDamageForce( &subInfo, dir, vecSrc );

					if (tr.fraction != 1.0)
					{
						ClearMultiDamage( );

						pEntity->DispatchTraceAttack( subInfo, dir, &tr );
						ApplyMultiDamage();
					}
					else
						pEntity->TakeDamage( subInfo );
				}
			}
		}
	}

	//-----------------------------------------------------------------------------
	// Purpose: Find out if this player had an assistant when he killed an enemy
	//-----------------------------------------------------------------------------
	CBasePlayer *CTeamFortress::GetDeathAssistant( CBaseEntity *pKiller, CBaseEntity *pInflictor )
	{
		if ( !pKiller || pKiller->Classify() != CLASS_PLAYER )
			return NULL;

		CBaseTFPlayer *pAssistant = NULL;

		// Killing entity might be specifying a scorer player
		IScorer *pScorerInterface = dynamic_cast<IScorer*>( pKiller );
		if ( pScorerInterface )
		{
			pAssistant = (CBaseTFPlayer*)pScorerInterface->GetAssistant();
		}

		// Inflicting entity might be specifying a scoring player
		if ( !pAssistant )
		{
			pScorerInterface = dynamic_cast<IScorer*>( pInflictor );
			if ( pScorerInterface )
			{
				pAssistant = (CBaseTFPlayer*)pScorerInterface->GetAssistant();
			}
		}

		// Don't allow self assistance
		Assert( pAssistant != pKiller );
		return pAssistant;
	}

	void CTeamFortress::DeathNotice( CBasePlayer *pVictim, const CTakeDamageInfo &info )
	{
		// Work out what killed the player, and send a message to all clients about it
		const char *killer_weapon_name = "world";		// by default, the player is killed by the world
		int killer_index = 0;
		int assist_index = 0;

		// Find the killer & the scorer
		CBaseEntity *pInflictor = info.GetInflictor();
		CBaseEntity *pKiller = info.GetAttacker();
		CBasePlayer *pScorer = GetDeathScorer( pKiller, pInflictor );
		CBasePlayer *pAssistant = GetDeathAssistant( pKiller, pInflictor );

		if ( pAssistant )
			assist_index = pAssistant->entindex();

		// Custom kill type?
		if ( info.GetDamageCustom() )
		{
			killer_weapon_name = GetDamageCustomString(info);
			if ( pScorer )
				killer_index = pScorer->entindex();
		}
		else
		{
			// Is the killer a client?
			if ( pScorer )
			{
				killer_index = pScorer->entindex();
				
				if ( pInflictor )
				{
					if ( pInflictor == pScorer )
					{
						// If the inflictor is the killer,  then it must be their current weapon doing the damage
						if ( pScorer->GetActiveWeapon() )
							killer_weapon_name = pScorer->GetActiveWeapon()->GetDeathNoticeName();
					}
					else
						killer_weapon_name = STRING( pInflictor->m_iClassname );  // it's just that easy
				}
			}
			else
				killer_weapon_name = STRING( pInflictor->m_iClassname );

			// strip the NPC_* or weapon_* from the inflictor's classname
			if ( strncmp( killer_weapon_name, "weapon_", 7 ) == 0 )
				killer_weapon_name += 7;
			else if ( strncmp( killer_weapon_name, "NPC_", 8 ) == 0 )
				killer_weapon_name += 8;
			else if ( strncmp( killer_weapon_name, "func_", 5 ) == 0 )
				killer_weapon_name += 5;
		}

#if 0
		// Did he kill himself?
		if ( pVictim == pScorer )  
			UTIL_LogPrintf( "\"%s<%i>\" killed self with %s\n",  STRING( pVictim->PlayerData()->netname ), engine->GetPlayerUserId( pVictim->edict() ), killer_weapon_name );
		else if ( pScorer )
		{
			UTIL_LogPrintf( "\"%s<%i>\" killed \"%s<%i>\" with %s\n",  STRING( pScorer->pl.netname ),
				engine->GetPlayerUserId( pScorer->edict() ),
				STRING( pVictim->PlayerData()->netname ),
				engine->GetPlayerUserId( pVictim->edict() ),
				killer_weapon_name );
		}
		else
			// killed by the world
			UTIL_LogPrintf( "\"%s<%i>\" killed by world with %s\n",  STRING( pVictim->PlayerData()->netname ), engine->GetPlayerUserId( pVictim->edict() ), killer_weapon_name );
#endif
		IGameEvent * event = gameeventmanager->CreateEvent( "player_death" );
		if(event)
		{
			event->SetInt( "userid", pVictim->GetUserID() );
			event->SetInt( "attacker", pScorer ? pScorer->GetUserID() : 0 );
			event->SetInt( "assister", pAssistant ? pAssistant->GetUserID() : 0 );
			event->SetString( "weapon", killer_weapon_name );

			gameeventmanager->FireEvent( event, false );
		}
	}

	//-----------------------------------------------------------------------------
	// Purpose: Custom kill types for TF
	//-----------------------------------------------------------------------------
	const char *CTeamFortress::GetDamageCustomString(const CTakeDamageInfo &info)
	{
		switch( info.GetDamageCustom() )
		{
		case DMG_KILL_BULLRUSH:
			return "bullrush";
			break;
		default:
			break;
		};

		return "INVALID CUSTOM KILL TYPE";
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	// Input  : *pListener - 
	//			*pSpeaker - 
	// Output : Returns true on success, false on failure.
	//-----------------------------------------------------------------------------
	bool CTeamFortress::PlayerCanHearChat( CBasePlayer *pListener, CBasePlayer *pSpeaker )
	{
		if ( BaseClass::PlayerCanHearChat( pListener, pSpeaker ) )
			return true;

		CBaseTFPlayer *listener = static_cast< CBaseTFPlayer * >( pListener );
		CBaseTFPlayer *speaker = static_cast< CBaseTFPlayer * >( pSpeaker );

		if ( listener && speaker )
		{
			if ( listener->IsClass( TFCLASS_INFILTRATOR ) )
			{
				Vector delta;
				delta = listener->EarPosition() - speaker->GetAbsOrigin();
				if ( delta.Length() < INFILTRATOR_EAVESDROP_RADIUS )
					return true;
			}
		}

		return false;
	}


	void CTeamFortress::InitDefaultAIRelationships( void )
	{
		//  Allocate memory for default relationships
		CBaseCombatCharacter::AllocateDefaultRelationships();

		// --------------------------------------------------------------
		// First initialize table so we can report missing relationships
		// --------------------------------------------------------------
		int i, j;
		for (i=0;i<NUM_AI_CLASSES;i++)
		{
			for (j=0;j<NUM_AI_CLASSES;j++)
			{
				// By default all relationships are neutral of priority zero
				CBaseCombatCharacter::SetDefaultRelationship( (Class_T)i, (Class_T)j, D_NU, 0 );
			}
		}

		// ------------------------------------------------------------
		//	> CLASS_NONE
		// ------------------------------------------------------------
		CBaseCombatCharacter::SetDefaultRelationship(CLASS_NONE,				CLASS_NONE,				D_NU, 0);			
		CBaseCombatCharacter::SetDefaultRelationship(CLASS_NONE,				CLASS_PLAYER,			D_NU, 0);			

		// ------------------------------------------------------------
		//	> CLASS_PLAYER
		// ------------------------------------------------------------
		CBaseCombatCharacter::SetDefaultRelationship(CLASS_PLAYER,			CLASS_NONE,				D_NU, 0);			
		CBaseCombatCharacter::SetDefaultRelationship(CLASS_PLAYER,			CLASS_PLAYER,			D_NU, 0);
	}

	//-----------------------------------------------------------------------------
	// Purpose: Return a pointer to the opposing team
	//-----------------------------------------------------------------------------
	CTFTeam *GetOpposingTeam( CTeam *pTeam )
	{
		// Hacky!
		return pTeam->GetTeamNumber() == TEAM_HUMANS ?
			GetGlobalTFTeam(TEAM_ALIENS) :
			GetGlobalTFTeam(TEAM_HUMANS);
	}


	//------------------------------------------------------------------------------
	// Purpose : Return classify text for classify type
	//------------------------------------------------------------------------------
	const char *CTeamFortress::AIClassText(int classType)
	{
		switch (classType)
		{
			case CLASS_NONE:			return "CLASS_NONE";
			case CLASS_PLAYER:			return "CLASS_PLAYER";

			default:					return "MISSING CLASS in ClassifyText()";
		}
	}

	//-----------------------------------------------------------------------------
	// Purpose: When gaining new technologies in TF, prevent auto switching if we
	//  receive a weapon during the switch
	// Input  : *pPlayer - 
	//			*pWeapon - 
	// Output : Returns true on success, false on failure.
	//-----------------------------------------------------------------------------
	bool CTeamFortress::FShouldSwitchWeapon( CBasePlayer *pPlayer, CBaseCombatWeapon *pWeapon )
	{
		if ( !GetAllowWeaponSwitch() )
			return false;

		// Never auto switch to object placement
		if ( dynamic_cast<CWeaponBuilder*>(pWeapon) ) 
			return false;
		if ( dynamic_cast<CWeaponObjectSelection*>(pWeapon) ) 
			return false;

		return BaseClass::FShouldSwitchWeapon( pPlayer, pWeapon );
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	// Input  : allow - 
	//-----------------------------------------------------------------------------
	void CTeamFortress::SetAllowWeaponSwitch( bool allow )
	{
		m_bAllowWeaponSwitch = allow;
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	// Output : Returns true on success, false on failure.
	//-----------------------------------------------------------------------------
	bool CTeamFortress::GetAllowWeaponSwitch( void )
	{
		return m_bAllowWeaponSwitch;
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	// Input  : *pPlayer - 
	// Output : const char
	//-----------------------------------------------------------------------------
	const char *CTeamFortress::SetDefaultPlayerTeam( CBasePlayer *pPlayer )
	{
		Assert( pPlayer );

		return BaseClass::SetDefaultPlayerTeam( pPlayer );
	}

	// Called when game rules are created by CWorld
	void CTeamFortress::LevelInitPreEntity( void )
	{ 
		BaseClass::LevelInitPreEntity();

		g_flNextReinforcementTime = 0.0f;
	}

	// Called when game rules are destroyed by CWorld
	void CTeamFortress::LevelShutdown( void )
	{
		g_flNextReinforcementTime = 0.0f;
		g_hCurrentAct = NULL;

		BaseClass::LevelShutdown();
	}

	bool CTeamFortress::IsConnectedUserInfoChangeAllowed( CBasePlayer *pPlayer )
	{
		return true;
	}

	void InitBodyQue(void)
	{
		// FIXME: Make this work
	}

#endif


// ----------------------------------------------------------------------------- //
// Shared CTeamFortress code.
// ----------------------------------------------------------------------------- //

//-----------------------------------------------------------------------------
// Purpose: Send the appropriate weapon impact
//-----------------------------------------------------------------------------
void WeaponImpact( trace_t *tr, Vector vecDir, bool bHurt, CBaseEntity *pEntity, int iDamageType )
{
	// If we hit a combat shield, play the hit effect
	if ( iDamageType & (DMG_PLASMA | DMG_ENERGYBEAM) )
	{
		if ( bHurt )
		{
			Assert( pEntity );
			bool bHitHandheldShield = (pEntity->IsPlayer() && ((CBaseTFPlayer*)pEntity)->IsHittingShield( vecDir, NULL ));
			if ( bHitHandheldShield )
				UTIL_ImpactTrace( tr, iDamageType, "PlasmaShield" );
			else
			{
				// Client waits for server version 
#ifndef CLIENT_DLL
				// Make sure the server sends to us, even though we're predicting
				CDisablePredictionFiltering dpf;
				UTIL_ImpactTrace( tr, iDamageType, "PlasmaHurt" );
#endif
			}
		}
		else
			UTIL_ImpactTrace( tr, iDamageType, "PlasmaUnhurt" );
	}
	else
	{
		if ( bHurt )
		{
			Assert( pEntity );
			bool bHitHandheldShield = (pEntity->IsPlayer() && ((CBaseTFPlayer*)pEntity)->IsHittingShield( vecDir, NULL ));
			if ( bHitHandheldShield )
			{
				UTIL_ImpactTrace( tr, iDamageType, "ImpactShield" );
			}
			else
			{
				// Client waits for server version 
#ifndef CLIENT_DLL
				// Make sure the server sends to us, even though we're predicting
				CDisablePredictionFiltering dpf;
				UTIL_ImpactTrace( tr, iDamageType, "Impact" );
#endif
			}
		}
		else
			UTIL_ImpactTrace( tr, iDamageType, "ImpactUnhurt" );
	}
}

static bool CheckCollisionGroupPlayerMovement( int collisionGroup0, int collisionGroup1 )
{
 	if ( collisionGroup0 == COLLISION_GROUP_PLAYER )
	{
		// Players don't collide with objects or other players
		if ( collisionGroup1 == COLLISION_GROUP_PLAYER || collisionGroup1 == TFCOLLISION_GROUP_OBJECT )
			return false;
 	}


	if ( collisionGroup1 == COLLISION_GROUP_PLAYER_MOVEMENT )
	{
		// This is only for probing, so it better not be on both sides!!!
		Assert( collisionGroup0 != COLLISION_GROUP_PLAYER_MOVEMENT );

		// No collide with players any more
		// Nor with objects or grenades
		switch ( collisionGroup0 )
		{
		default:
			break;
		case COLLISION_GROUP_PLAYER:
			return false;
		case TFCOLLISION_GROUP_OBJECT_SOLIDTOPLAYERMOVEMENT:
			// Certain objects are still solid to player movement
			return true;
		case TFCOLLISION_GROUP_COMBATOBJECT:
		case TFCOLLISION_GROUP_OBJECT:
			return false;
		case TFCOLLISION_GROUP_GRENADE:
		case COLLISION_GROUP_DEBRIS:
			return false;
		}
	}

	return true;
}

void CTeamFortress::WeaponTraceLine( const Vector& src, const Vector& end, unsigned int mask, CBaseEntity *pShooter, int damageType, trace_t* pTrace )
{
	// Iterate over all shields on the same team, disable them so
	// we don't intersect with them...
	CShield::ActivateShields( false, pShooter->GetTeamNumber() );

	UTIL_TraceLine(src, end, mask, pShooter, /* TFCOLLISION_GROUP_WEAPON */ COLLISION_GROUP_NONE, pTrace);

#if 0
#if !defined( CLIENT_DLL )
	NDebugOverlay::Line( src, pTrace->endpos, 255,255,255, true, 5.0 );
	NDebugOverlay::Box( pTrace->endpos, Vector(-2,-2,-2), Vector(2,2,2), 255,255,255, true, 5.0 );
#endif
#endif

	// Shield check...
	if (pTrace->fraction != 1.0)
	{
		CBaseEntity *pEntity = pTrace->m_pEnt;
		CShield* pShield = dynamic_cast<CShield*>(pEntity);
		if (pShield)
		{
			Vector vecDir;
			VectorSubtract( end, src, vecDir );

			// We deflected all of the damage
			pShield->RegisterDeflection( vecDir, damageType, pTrace );
		}
	}

	// Reactivate all shields
	CShield::ActivateShields( true );
}


bool CTeamFortress::ShouldCollide( int collisionGroup0, int collisionGroup1 )
{
	if ( collisionGroup0 > collisionGroup1 )
		// swap so that lowest is always first
		V_swap(collisionGroup0,collisionGroup1);

	// Ignore base class HL2 definition for COLLISION_GROUP_WEAPON, change to COLLISION_GROUP_NONE
	if ( collisionGroup0 == COLLISION_GROUP_WEAPON )
		collisionGroup0 = COLLISION_GROUP_NONE;

	if ( collisionGroup1 == COLLISION_GROUP_WEAPON )
		collisionGroup1 = COLLISION_GROUP_NONE;

	// Shields collide with weapons + grenades only
	if ( collisionGroup0 == TFCOLLISION_GROUP_SHIELD )
		return ((collisionGroup1 == TFCOLLISION_GROUP_WEAPON) || (collisionGroup1 == TFCOLLISION_GROUP_GRENADE));

	// Weapons can collide with things (players) in vehicles.
	if( collisionGroup0 == COLLISION_GROUP_IN_VEHICLE && collisionGroup1 == TFCOLLISION_GROUP_WEAPON )
		return true;

	// COLLISION TEST:
	// Players don't collide with other players or objects
	if ( !CheckCollisionGroupPlayerMovement( collisionGroup0, collisionGroup1 ) )
		return false;
	// Reciprocal test
	if ( !CheckCollisionGroupPlayerMovement( collisionGroup1, collisionGroup0 ) )
		return false;

	// Grenades don't collide with debris
	if ( collisionGroup1 == TFCOLLISION_GROUP_GRENADE )
		if ( collisionGroup0 == COLLISION_GROUP_DEBRIS )
			return false;

	// Combat objects don't collide with players
	if ( collisionGroup1 == TFCOLLISION_GROUP_COMBATOBJECT )
		if ( collisionGroup0 == COLLISION_GROUP_PLAYER )
			return false;

	// Resource chunks don't collide with each other or vehicles
	if ( collisionGroup1 == TFCOLLISION_GROUP_RESOURCE_CHUNK )
	{
		if ( collisionGroup0 == TFCOLLISION_GROUP_RESOURCE_CHUNK )
			return false;
//		if ( collisionGroup0 == COLLISION_GROUP_VEHICLE )
//			return false;
	}

	return BaseClass::ShouldCollide( collisionGroup0, collisionGroup1 ); 
}

//-----------------------------------------------------------------------------
// Purpose: Fire a generic bullet
//-----------------------------------------------------------------------------
void CTeamFortress::FireBullets( const CTakeDamageInfo &info, int cShots, const Vector &vecSrc, const Vector &vecDirShooting, 
								const Vector &vecSpread, float flDistance, int iAmmoType, 
								int iTracerFreq, int firingEntID, int attachmentID, const char *sCustomTracer )
{
	// This function should've been implemented as an override for FireBullets in the CBaseEntity class, under CBaseTFCombatWeapon
	// instead of here. So in the long term we should probably move it across to there (and use FireBulletsInfo_t).

	static int tracerCount;
	bool tracer;
	trace_t	tr;
	CTakeDamageInfo subInfo = info;
	CBaseTFCombatWeapon *pWeapon = dynamic_cast<CBaseTFCombatWeapon*>(info.GetInflictor());

	Assert( subInfo.GetInflictor() );

	// Default attacker is the inflictor
	if ( subInfo.GetAttacker() == NULL )
		subInfo.SetAttacker( subInfo.GetInflictor() );

	// --------------------------------------------------
	//  Get direction vectors for spread
	// --------------------------------------------------
	Vector vecUp = Vector(0,0,1);
	Vector vecRight;
	CrossProduct ( vecDirShooting,  vecUp,		vecRight );	
	CrossProduct ( vecDirShooting, -vecRight,   vecUp  );	

#ifndef CLIENT_DLL
	ClearMultiDamage();
#endif

	int seed = 0;

	for (int iShot = 0; iShot < cShots; iShot++)
	{
		// get circular gaussian spread
		float x, y, z;

		do 
		{
			float x1, x2, y1, y2;

			// Note the additional seed because otherwise we get the same set of random #'s and will get stuck
			//  in an infinite loop here potentially
			// FIXME:  Can we use a gaussian random # function instead?  ywb
			if ( CBaseEntity::GetPredictionRandomSeed() != -1 )
			{
				x1 = SharedRandomFloat("randshot", -0.5f, 0.5f, ++seed );
				x2 = SharedRandomFloat("randshot", -0.5f, 0.5f, ++seed );
				y1 = SharedRandomFloat("randshot", -0.5f, 0.5f, ++seed );
				y2 = SharedRandomFloat("randshot", -0.5f, 0.5f, ++seed );
			}
			else
			{
				x1 = RandomFloat( -0.5, 0.5 );
				x2 = RandomFloat( -0.5, 0.5 );
				y1 = RandomFloat( -0.5, 0.5 );
				y2 = RandomFloat( -0.5, 0.5 );
			}

			x = x1 + x2;
			y = y1 + y2;

			z = x*x+y*y;
		} while (z > 1);

		Vector vecDir = vecDirShooting + x * vecSpread.x * vecRight + y * vecSpread.y * vecUp;
		Vector vecEnd = vecSrc + vecDir * flDistance;

		// Try the trace
		WeaponTraceLine( vecSrc, vecEnd, MASK_SHOT, subInfo.GetInflictor(), subInfo.GetDamageType(), &tr );

		tracer = false;
		if (iTracerFreq != 0 && (tracerCount++ % iTracerFreq) == 0)
		{
			Vector vecTracerSrc;

			// adjust tracer position for player
			if ( subInfo.GetInflictor()->IsPlayer() )
			{
				Vector forward, right;
				CBasePlayer *pPlayer = ToBasePlayer( subInfo.GetInflictor() );
				pPlayer->EyeVectors( &forward, &right, NULL );
				vecTracerSrc = vecSrc + Vector ( 0 , 0 , -4 ) + right * 2 + forward * 16;
			}
			else
			{
				vecTracerSrc = vecSrc;
			}
			
			if ( iTracerFreq != 1 )		// guns that always trace also always decal
				tracer = true;

			if ( sCustomTracer )
				UTIL_Tracer( vecTracerSrc, tr.endpos, subInfo.GetInflictor()->entindex(), TRACER_DONT_USE_ATTACHMENT, 0, false, (char*)sCustomTracer );
			else
				UTIL_Tracer( vecTracerSrc, tr.endpos, subInfo.GetInflictor()->entindex() );
		}

		// do damage, paint decals
		if ( tr.fraction != 1.0 )
		{
			CBaseEntity *pEntity = tr.m_pEnt;

			// NOTE: If we want to know more than whether or not the entity can actually be hurt
			//		 for the purposes of impact effects, the client needs to know a lot more.
			bool bTargetCouldBeHurt = false;
			if ( pEntity->m_takedamage )
			{
				if ( !pEntity->InSameTeam( subInfo.GetInflictor() ) )
				{
					bTargetCouldBeHurt = true;
				}

#ifndef CLIENT_DLL
				subInfo.SetDamagePosition( vecSrc );
				// Hit the target 
				pEntity->DispatchTraceAttack( subInfo, vecDir, &tr ); 
#endif
			}
 
			// No decal if we hit a shield
			if ( pEntity->GetCollisionGroup() != TFCOLLISION_GROUP_SHIELD )
				WeaponImpact( &tr, vecDir, bTargetCouldBeHurt, pEntity, subInfo.GetDamageType() );
		}

		if ( pWeapon )
			pWeapon->BulletWasFired( vecSrc, tr.endpos );
	}

	// Apply any damage we've stacked up
#ifndef CLIENT_DLL
	ApplyMultiDamage();
#endif
}



//-----------------------------------------------------------------------------
// Purpose: Init TF2 ammo definitions
//-----------------------------------------------------------------------------
CAmmoDef *GetAmmoDef()
{
	static CAmmoDef def;
	static bool bInitted = false;

	if ( !bInitted )
	{
		bInitted = true;
		
		// Added some basic physics force ~hogsy
		def.AddAmmoType( "Harpoons", DMG_CLUB, TRACER_NONE, 0, 0, 0, 0, 0 );
		def.AddAmmoType( "Bullets", DMG_BULLET, TRACER_LINE_AND_WHIZ, 0, 0, INFINITE_AMMO, 2, 0 );
		def.AddAmmoType( "Rockets", DMG_BLAST, TRACER_NONE, 0, 0, 6, 32, 0 );
		def.AddAmmoType( "Grenades", DMG_BLAST, TRACER_NONE, 0, 0, 3, 32, 0 );
		def.AddAmmoType( "ShieldGrenades", DMG_ENERGYBEAM, TRACER_NONE, 0, 0, 5, 0, 0 );
		def.AddAmmoType( "ShotgunEnergy", DMG_ENERGYBEAM, TRACER_LINE, 0, 0, INFINITE_AMMO, 0, 0 );
		def.AddAmmoType( "PlasmaGrenade", DMG_ENERGYBEAM | DMG_BLAST, TRACER_NONE, 0, 0, 30, 0, 0 );
		def.AddAmmoType( "ResourceChunks", DMG_GENERIC, TRACER_NONE, 0, 0, 4, 0, 0 );		// Resource chunks
		def.AddAmmoType( "Limpets", DMG_BLAST, TRACER_NONE, 0, 0, 40, 0, 0 );
		def.AddAmmoType( "Gasoline", DMG_BURN, TRACER_NONE, 0, 0, 80, 0, 0 );

		// Combat Objects
		def.AddAmmoType("RallyFlags",		DMG_GENERIC,				TRACER_NONE,	0,	0,	1,	0,	0);
		def.AddAmmoType("EMPGenerators",	DMG_GENERIC,				TRACER_NONE,	0,	0,	1,	0,	0);
	}

	return &def;
}
