#include "cbase.h"
#include "tf_gamemovement.h"
#include "in_buttons.h"
#include "tier0/vprof.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"

#define	SPEED_STOP_THRESHOLD		1.0f
#define BUMP_MAX_COUNT				8

#define	IMPACT_NORMAL_FLOOR			0.7f
#define IMPACT_NORMAL_WALL			0.0f

enum
{
	MOVEMENT_BLOCKED_NONE  = 0x0,
	MOVEMENT_BLOCKED_WALL  = 0x1,
	MOVEMENT_BLOCKED_FLOOR = 0x2,
	MOVEMENT_BLOCKED_ALL   = 0x4
};

#define SPEED_CROP_FRACTION_WALKING		0.4f
#define SPEED_CROP_FRACTION_USING		0.3f
#define SPEED_CROP_FRACTION_DUCKING		0.3f

char    *va(char *format, ...);

/*	Basically, in the original code-base the following would call up the base class' ProcessMovement func
	which would then set the SpeedCrop flag as false, then redirect to _ProcessMovement.
	Since that was dumb, and isn't necessary anymore, we just throw ourselves to this directly instead
	and update the flag ourselves. ~hogsy
*/
void CTFGameMovement::ProcessMovement( CBasePlayer *pPlayer, CMoveData *pMove )
{
	mv	= pMove;
	player = pPlayer;

	Assert( mv );
	Assert( player );

	m_iSpeedCropped = SPEED_CROPPED_RESET;

	// bisect time interval for very long commands
	if (gpGlobals->frametime > 0.05f)
	{
		float savet = gpGlobals->frametime;
		
		float t = gpGlobals->frametime * 0.5f;

		gpGlobals->frametime = t;
		
		ProcessMovement( player, mv );

		// NOTE:  Only fire impulse on first time through
		mv->m_nImpulseCommand = 0;

		// Make sure frametime is valid
		gpGlobals->frametime = t;

		ProcessMovement( player, mv );

		// Reset frametime so other functionas after this aren't hosed
		gpGlobals->frametime = savet;

		return;
	}

	mv->m_flMaxSpeed = sv_maxspeed.GetFloat();

	// Run the command.
	PlayerMove();

	FinishMove();
}

void CTFGameMovement::CategorizePosition( void )
{
	VPROF( "CTFGameMovement::CategorizePosition" );

	// Doing this before we move may introduce a potential latency in water detection, but
	// doing it after can get us stuck on the bottom in water if the amount we move up
	// is less than the 1 pixel 'threshold' we're about to snap to.	Also, we'll call
	// this several times per frame, so we really need to avoid sticking to the bottom of
	// water on each call, and the converse case will correct itself if called twice.
	CheckWater();

	// observers don't have a ground entity
	if ( player->IsObserver() )
		return;

	trace_t trace;
	Vector vStart( mv->GetAbsOrigin().x, mv->GetAbsOrigin().y, mv->GetAbsOrigin().z + 2 );
	Vector vEnd( mv->GetAbsOrigin().x, mv->GetAbsOrigin().y, mv->GetAbsOrigin().z - 66 );

	// Assume we are not on the ground
	player->SetGroundEntity( (CBaseEntity *)NULL );
	player->m_pSurfaceData = NULL;
	player->m_surfaceFriction = 1.0f;
	player->m_chTextureType = 'C';

	// Check our velocity in z (are we shooting up really fast - then we are not on ground).
	if ( mv->m_vecVelocity.z <= 180.0f )
	{
		// Move downward.
		TracePlayerBBox( vStart, vEnd, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, trace );

		//Moving up two units got us stuck in something, start tracing down exactly at our
		//  current origin (since CheckStuck allowed us to get here, that pos is valid)
		if ( trace.startsolid )
		{
			vStart = mv->GetAbsOrigin();
			TracePlayerBBox( vStart, vEnd, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, trace );
		}

		// Check to see if we are on the ground
		if ( ( ( trace.plane.normal.z >= IMPACT_NORMAL_FLOOR ) || trace.IsDispSurfaceWalkable() ) && ( trace.fraction < 0.06f ) )
		{
			player->SetGroundEntity( trace.m_pEnt );
			VectorCopy( trace.plane.normal, m_vecGroundNormal );

			// Add standing on object to touch list
			if ( trace.DidHitNonWorldEntity() )
				MoveHelper()->AddToTouched( trace, mv->m_vecVelocity );

			// Reset water jumping.
			player->m_flWaterJumpTime = 0.0f;

			// If we could make the move, drop us down that 1 pixel
			if ( ( int )player->GetWaterLevel() < WL_Waist && !trace.startsolid && !trace.allsolid )
			{
				// check distance we would like to move -- this is supposed to just keep up
				// "on the ground" surface not stap us back to earth
				mv->SetAbsOrigin( vStart + trace.fraction * ( vEnd - vStart ) );
//				VectorCopy( trace.endpos, mv->m_vecOrigin );
			}		
		}

		// NOTE: should this be surrounded by trace.fraction <= 1.0f ?????

		// Setup surface properties.
		{
			VPROF( "CTFGameMovement::CategorizePosition / Surface Props" );
			IPhysicsSurfaceProps *physprops = MoveHelper( )->GetSurfaceProps();
			player->m_surfaceProps = trace.surface.surfaceProps;
			player->m_pSurfaceData = physprops->GetSurfaceData( player->m_surfaceProps );
			physprops->GetPhysicsProperties( player->m_surfaceProps, NULL, NULL, &player->m_surfaceFriction, NULL );
		}

		// HACKHACK: Scale this to fudge the relationship between vphysics friction values and player friction values.
		// A value of 0.8f feels pretty normal for vphysics, whereas 1.0f is normal for players.
		// This scaling trivially makes them equivalent.  REVISIT if this affects low friction surfaces too much.
		player->m_surfaceFriction *= 1.25f;
		if ( player->m_surfaceFriction > 1.0f )
			player->m_surfaceFriction = 1.0f;
		player->m_chTextureType = player->m_pSurfaceData->game.material;
	}

	// If we are not on the ground...
	if ( player->GetGroundEntity() == NULL )
		player->m_Local.m_flFallVelocity = -mv->m_vecVelocity.z;

	// Store off the starting water level
	m_nOldWaterLevel = player->GetWaterLevel();
}

void CTFGameMovement::HandleLadder( void )
{
	// No ladder movement if the player is dead or on a train.
	if ( player->pl.deadflag || ( player->GetFlags() & FL_ONTRAIN ) )
		return;

	// Ladder initialization.
	m_nOnLadder = 0;

	if ( !BaseClass::LadderMove() && ( player->GetMoveType() == MOVETYPE_LADDER ) )
	{
		// clear ladder stuff unless player is noclipping or being lifted by barnacle. 
		// it will be set immediately again next frame if necessary
		player->SetMoveType( MOVETYPE_WALK );
		player->SetMoveCollide( MOVECOLLIDE_DEFAULT );
	}
}

void CTFGameMovement::SpeedCrop( void )
{
	// Verify speed hasn't been cropped already (shouldn't have!!!).
	if ( m_iSpeedCropped )
		return;

	// Set the speed cropped flag.
	m_iSpeedCropped |= SPEED_CROPPED_DUCK;

	// If the "walking" key is pressed -- crop speed.
	if ( mv->m_nButtons & IN_SPEED )
	{
		mv->m_flForwardMove *= SPEED_CROP_FRACTION_WALKING;
		mv->m_flSideMove *= SPEED_CROP_FRACTION_WALKING;
		mv->m_flUpMove *= SPEED_CROP_FRACTION_WALKING;
	}
	// If the "use" key is pressed and the player is on the ground -- crop speed.
	else if ( ( mv->m_nButtons & IN_USE ) && ( player->GetGroundEntity() != NULL ) )
	{
		mv->m_flForwardMove *= SPEED_CROP_FRACTION_USING;
		mv->m_flSideMove *= SPEED_CROP_FRACTION_USING;
		mv->m_flUpMove *= SPEED_CROP_FRACTION_USING;
	}
	// If the player is "ducking" -- crop speed
	else if ( player->GetFlags() & FL_DUCKING )
	{
		mv->m_flForwardMove *= SPEED_CROP_FRACTION_DUCKING;
		mv->m_flSideMove *= SPEED_CROP_FRACTION_DUCKING;
		mv->m_flUpMove *= SPEED_CROP_FRACTION_DUCKING;
	}

	// Moving backwards happens more slowly
	float flAngle = atan2( mv->m_flSideMove, mv->m_flForwardMove ) / M_PI;
	flAngle = 2.0f * (fabs(flAngle) - 0.5f);
	flAngle = clamp( flAngle, 0.0f, 1.0f );
	float flFactor = 1.0f - fabs(flAngle) * (1.0f - sv_backspeed.GetFloat());

	mv->m_flForwardMove *= flFactor;
	mv->m_flSideMove *= flFactor;
}

/*	Figures out how the constraint should slow us down.
*/
float CTFGameMovement::ComputeConstraintSpeedFactor( void )
{
	// If we have a constraint, slow down because of that too...
	// Get the TF movement data.
	CTFMoveData *pTFMove = TFMove();
	if ( !pTFMove || pTFMove->m_flConstraintRadius == 0.0f )
		return 1.0f;

	float flDistSq = mv->GetAbsOrigin().DistToSqr( pTFMove->m_vecConstraintCenter );

	float flOuterRadiusSq = pTFMove->m_flConstraintRadius * pTFMove->m_flConstraintRadius;
	float flInnerRadiusSq = pTFMove->m_flConstraintRadius - pTFMove->m_flConstraintWidth;
	flInnerRadiusSq *= flInnerRadiusSq;

	// Only slow us down if we're inside the constraint ring
	if ((flDistSq <= flInnerRadiusSq) || (flDistSq >= flOuterRadiusSq))
		return 1.0f;

	// Only slow us down if we're running away from the center
	Vector vecDesired;
	VectorMultiply( m_vecForward, mv->m_flForwardMove, vecDesired );
	VectorMA( vecDesired, mv->m_flSideMove, m_vecRight, vecDesired );
	VectorMA( vecDesired, mv->m_flUpMove, m_vecUp, vecDesired );

	Vector vecDelta;
	VectorSubtract( mv->GetAbsOrigin(), pTFMove->m_vecConstraintCenter, vecDelta );
	VectorNormalize( vecDelta );
	VectorNormalize( vecDesired );
	if (DotProduct( vecDelta, vecDesired ) < 0.0f)
		return 1.0f;

	float flFrac = (sqrt(flDistSq) - (pTFMove->m_flConstraintRadius - pTFMove->m_flConstraintWidth)) / pTFMove->m_flConstraintWidth;

	float flSpeedFactor = Lerp( flFrac, 1.0f, pTFMove->m_flConstraintSpeedFactor ); 
	return flSpeedFactor;
}


//-----------------------------------------------------------------------------
// Purpose: Called in PrePlayerMove(), this function clamps the player's overall
//          speed.  It is clamped to either the maximum client's or server's 
//          speed (whichever is lower).
//-----------------------------------------------------------------------------
void CTFGameMovement::SetupSpeed( void )
{
	// Reset the outgoing applied velocity.
	mv->m_outWishVel.Init();

	// Don't clamp speed if we are sin an "ISOMETRIC" or "NOCLIP" movetype.
	if ( ( player->GetMoveType() == MOVETYPE_ISOMETRIC ) ||
		 ( player->GetMoveType() == MOVETYPE_NOCLIP ) )
		 return;

	// Some events negate speed, check for these first.
	if ( ( player->GetFlags() & FL_FROZEN ) || ( player->GetFlags() & FL_ONTRAIN ) ||  IsDead() )
	{
		mv->m_flForwardMove = 0;
		mv->m_flSideMove = 0;
		mv->m_flUpMove = 0;
		return;
	}

	// Calculate the players max speed given movements.
	float flSpeed = ( mv->m_flForwardMove * mv->m_flForwardMove ) +
		            ( mv->m_flSideMove * mv->m_flSideMove ) +
			 	    ( mv->m_flUpMove * mv->m_flUpMove );
	if ( flSpeed == 0.0f )
		return;

	flSpeed = sqrt( flSpeed );

	// NOTE: The client max speed was set to the movement max speed (mv->m_flMaxSpeed)
	//       in the SetupMove, however the _ProcessMovement code post sets
	//       mv->m_flMaxSpeed to the maximum server speed (sv_maxspeed),
	//       thus, the need to the check.  If we forego the maximum server speed, 
	//       this code can be removed.
	if ( mv->m_flClientMaxSpeed )
		mv->m_flMaxSpeed = min( mv->m_flClientMaxSpeed, mv->m_flMaxSpeed );

	// Slow down by the speed factor
	float flSpeedFactor = 1.0f;
	if (player->m_pSurfaceData)
		flSpeedFactor = player->m_pSurfaceData->game.maxSpeedFactor;

	// If we have a constraint, slow down because of that too...
	// Get the TF movement data
	float flConstraintSpeedFactor = ComputeConstraintSpeedFactor();
	if (flConstraintSpeedFactor < flSpeedFactor)
		flSpeedFactor = flConstraintSpeedFactor;

	mv->m_flMaxSpeed *= flSpeedFactor;

	if ( flSpeed > mv->m_flMaxSpeed )
	{
		float flRatio = mv->m_flMaxSpeed / flSpeed;
		mv->m_flForwardMove *= flRatio;
		mv->m_flSideMove *= flRatio;
		mv->m_flUpMove *= flRatio;
	}

	// Crop speed if necessary.
	SpeedCrop();
}

void CTFGameMovement::FinishUnDuck( void )
{
	int i;
	trace_t trace;
	Vector newOrigin;

	Vector vDuckHullMin = GetPlayerMins( true );
	Vector vStandHullMin = GetPlayerMins( false );

	VectorCopy( mv->GetAbsOrigin(), newOrigin );

	if ( player->GetGroundEntity() != NULL )
	{
		for ( i = 0; i < 3; i++ )
			newOrigin[i] += ( vDuckHullMin[i] - vStandHullMin[i] );
	}
	else
	{
  		Vector viewDelta = player->GetViewOffset() - GetPlayerViewOffset( false );
		VectorAdd( newOrigin, viewDelta, newOrigin );
	}
	
	TracePlayerBBox( newOrigin, newOrigin, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, trace );

	if ( !trace.startsolid )
	{
		player->m_Local.m_bDucked = false;

		// Oh, no, changing hulls stuck us into something, try unsticking downward first.
		TracePlayerBBox( newOrigin, newOrigin, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, trace );
		if ( trace.startsolid )
		{
			// See if we are stuck?  If so, stay ducked with the duck hull until we have a clear spot
			player->m_Local.m_bDucked = true;
			return;
		}

		player->RemoveFlag( FL_DUCKING );
		player->m_Local.m_bDucking  = false;
		player->SetViewOffset( GetPlayerViewOffset( false ) );
		player->m_Local.m_flDucktime = 0;
		
		// Set our new origin (no need to use VectorCopy). ~hogsy
		mv->SetAbsOrigin(newOrigin);
	}
}

void CTFGameMovement::HandleDuck( void )
{
	// Store button presses and changes.
	int buttonsChanged	= ( mv->m_nOldButtons ^ mv->m_nButtons );	// These buttons have changed this frame
	int buttonsPressed	=  buttonsChanged & mv->m_nButtons;			// The changed ones still down are "pressed"
	int buttonsReleased	=  buttonsChanged & mv->m_nOldButtons;		// The changed ones which were previously down are "released"

	if ( mv->m_nButtons & IN_DUCK )
		mv->m_nOldButtons |= IN_DUCK;
	else
		mv->m_nOldButtons &= ~IN_DUCK;

	// Handle the player dead case.
	if ( IsDead() && ( player->GetFlags() & FL_DUCKING ) )
	{
		FinishUnDuck();
		return;
	}

	if ( ( mv->m_nButtons & IN_DUCK ) || ( player->m_Local.m_bDucking ) || ( player->GetFlags() & FL_DUCKING ) )
	{
#if 0
		// Remove all movement when ducking!
		mv->m_flForwardMove = 0.0f;
		mv->m_flSideMove = 0.0f;
		mv->m_flUpMove = 0.0f;
#endif

		if ( mv->m_nButtons & IN_DUCK )
		{
			if ( ( buttonsPressed & IN_DUCK ) && !( player->GetFlags() & FL_DUCKING ) )
			{
				// Use 1 second so super long jump will work
				player->m_Local.m_flDucktime = 1000;
				player->m_Local.m_bDucking    = true;
			}

			float duckmilliseconds = max( 0.0f, 1000.0f - (float)player->m_Local.m_flDucktime );
			float duckseconds = duckmilliseconds / 1000.0f;
			
			if ( player->m_Local.m_bDucking )
			{
				// Finish ducking immediately if duck time is over or not on ground
				if ( ( duckseconds > TIME_TO_DUCK ) || 
					( player->GetGroundEntity() == NULL ) )
					FinishDuck();
				else
				{
					// Calc parametric time
					float duckFraction = SimpleSpline( duckseconds / TIME_TO_DUCK );
					SetDuckedEyeOffset( duckFraction );
				}
			}
		}
		else
		{
			if ( (buttonsReleased & IN_DUCK ) && ( player->GetFlags() & FL_DUCKING ) )
			{
				// Use 1 second so super long jump will work
				player->m_Local.m_flDucktime = 1000;
				player->m_Local.m_bDucking    = true;  // or unducking
			}

			float duckmilliseconds = max( 0.0f, 1000.0f - (float)player->m_Local.m_flDucktime );
			float duckseconds = duckmilliseconds / 1000.0f;

			if ( player->m_Local.m_bDucking ) // or unducking
			{
				// Finish ducking immediately if duck time is over or not on ground
				if ( ( duckseconds > TIME_TO_UNDUCK ) ||
					 ( player->GetGroundEntity() == NULL ) )
					FinishUnDuck();
				else
				{
					// Calc parametric time
					float duckFraction = SimpleSpline( 1.0f - ( duckseconds / TIME_TO_UNDUCK ) );
					SetDuckedEyeOffset( duckFraction );
				}
			}
		}
	}
}

void CTFGameMovement::SetupViewAngles( void )
{
	// Cache the view angles.
	AngleVectors( mv->m_vecViewAngles, &m_vecForward, &m_vecRight, &m_vecUp );

	// Add a view punch if necessary.
	QAngle	v_angle = ( mv->m_vecViewAngles + player->m_Local.m_vecPunchAngle );
	
	// Adjust the view roll angle.
	if ( ( player->GetMoveType() != MOVETYPE_ISOMETRIC ) && ( player->GetMoveType() != MOVETYPE_NOCLIP ) )
		mv->m_vecViewAngles[ROLL] = CalcRoll( v_angle, mv->m_vecVelocity, sv_rollangle.GetFloat(), sv_rollspeed.GetFloat() ) * 4.0f;
	else
		mv->m_vecViewAngles[ROLL] = 0.0f;

	// Copy the yaw and pitch.
	// NOTE: Adjust the client view angles to match the values on the server.
	mv->m_vecViewAngles[PITCH] = v_angle[PITCH];
	mv->m_vecViewAngles[YAW] = v_angle[YAW];
	if ( mv->m_vecViewAngles[YAW] > 180.0f )
		mv->m_vecViewAngles[YAW] -= 360.0f;
}

bool CTFGameMovement::CheckDeath( void )
{
	// If we are dead, setup the appropriate data
	if ( IsDead() )
	{
		mv->m_flForwardMove = 0.0f;
		mv->m_flSideMove = 0.0f;
		mv->m_flUpMove = 0.0f;
		
		VectorCopy( mv->m_vecOldAngles, mv->m_vecViewAngles );

		player->SetViewOffset( VEC_DEAD_VIEWHEIGHT );

		return true;
	}

	return false;
}

void CTFGameMovement::UpdateTimers( void )
{
	BaseClass::ReduceTimers();
	BaseClass::DecayPunchAngle();
}

int CTFGameMovement::CheckStuck( void )
{
	VPROF( "CTFGameMovement::CheckStuck" );

	// NOTE: I am not bothering much with this as I am going to 
	//       implement an new UnSticking process.
	if ( ( player->GetMoveType() == MOVETYPE_NOCLIP ) ||
         ( player->GetMoveType() == MOVETYPE_NONE ) ||
		 ( player->GetMoveType() == MOVETYPE_ISOMETRIC ) )
		 return 0;

	return BaseClass::CheckStuck();
}

bool CTFGameMovement::PrePlayerMove( void )
{
	VPROF( "CTFGameMovement::PrePlayerMove" );

	// Assume we don't touch anything (Reset the touch list).
	MoveHelper()->ResetTouchList();

	// Check to see if we are stuck.
	if ( CheckStuck() )
		return false;

	// Update (reduce) movement timers.
	UpdateTimers();

	// Check to see if the player is dead and setup death data, otherwise setup 
	// the players view angles.
	if ( !CheckDeath() )
		SetupViewAngles();

	// Handle ducking.
	HandleDuck();

	// Handle ladder.
	HandleLadder();

	// Categorize the player's position.
	CategorizePosition();

	// Calculate the player's movement speed (has to happen after categorize position)
	SetupSpeed();

	// Update our stepping sound (based on the player's location).
	player->UpdateStepSound(player->m_pSurfaceData,mv->GetAbsOrigin(),mv->m_vecVelocity);

	return true;
}

void CTFGameMovement::HandlePlayerMove( void )
{
	// Handle movement.
	switch ( player->GetMoveType() )
	{
		case MOVETYPE_NONE:
			break;
		case MOVETYPE_NOCLIP:
			{
				FullNoClipMove(sv_specspeed.GetFloat(),sv_specaccelerate.GetFloat());
				break;
			}
		case MOVETYPE_FLY:
		case MOVETYPE_FLYGRAVITY:
			{
				FullTossMove();
				break;
			}
		case MOVETYPE_LADDER:
			{
				FullLadderMove(); // the below never equated to anything but false!!
//				FullLadderMove( (ladder != INVALID_ENTITY_HANDLE) );
				break;
			}
		case MOVETYPE_WALK:
			{
				// This should be moved elsewhere!!!  Just get it going for now.
				CTFMoveData *pTFMove = TFMove();
				Vector vecPlayerOrigin( mv->GetAbsOrigin().x, mv->GetAbsOrigin().y, mv->GetAbsOrigin().z );
				FullWalkMove(); // the below never equated to anything but false!!
				Vector vecPlayerDelta;
				VectorSubtract( mv->GetAbsOrigin(), vecPlayerOrigin, vecPlayerDelta );

				if ( ( fabs( vecPlayerDelta.x ) > 0.0001f ) || 
					 ( fabs( vecPlayerDelta.y ) > 0.0001f ) ||
					 ( fabs( vecPlayerDelta.z ) > 0.0001f ) )
					VectorCopy( vecPlayerDelta, pTFMove->m_vecPosDelta );

				break;
			}

		case MOVETYPE_ISOMETRIC:
			{
				//IsometricMove();
				// Could also try:  FullTossMove();
				FullWalkMove();
				break;
			}

		default:
			{
				DevMsg( 1, "Bogus pmove player movetype %i on (%i) 0=cl 1=sv\n", player->GetMoveType(), player->IsServer());
				break;
			}
	}
}

void CTFGameMovement::PlayerMove( void )
{
	VPROF( "CTFGameMovement::PlayerMove" );

	// Setup and initialization pre-player movement.
	if (!PrePlayerMove())
		return;

	// Handle Movement
	HandlePlayerMove();

	// Clean-up and updates post-player movement.
	PostPlayerMove();
}

void CTFGameMovement::PostPlayerMove( void )
{
}

void CTFGameMovement::AirMove( void )
{
	// Reduced to this, as it was practically unchanged anyway. ~hogsy
	BaseClass::AirMove();
}

//-----------------------------------------------------------------------------
// Purpose: This is here until the new movement code is complete.  I needed
//          to override hl2's walk move so that "floors" are identified 
//          differently.
//-----------------------------------------------------------------------------
void CTFGameMovement::WalkMove( void )
{
	VPROF( "CTFGameMovement::WalkMove" );

	int clip;
	int i;

	Vector wishvel;
	float spd;
	float fmove, smove;
	Vector wishdir;
	float wishspeed;

	Vector dest, start;
	Vector original, originalvel;
	Vector down, downvel;
	float downdist, updist;
	trace_t pm;
	Vector forward, right, up;

	AngleVectors (mv->m_vecViewAngles, &forward, &right, &up);  // Determine movement angles

	CHandle< CBaseEntity > oldground;
	oldground = player->GetGroundEntity();
	
	// Copy movement amounts
	fmove = mv->m_flForwardMove;
	smove = mv->m_flSideMove;
	
	// Zero out z components of movement vectors
	forward[2] = 0;
	right[2]   = 0;
	
	VectorNormalize (forward);  // Normalize remainder of vectors.
	VectorNormalize (right);    // 

	for (i=0 ; i<2 ; i++)       // Determine x and y parts of velocity
		wishvel[i] = forward[i]*fmove + right[i]*smove;
	
	wishvel[2] = 0;             // Zero out z part of velocity

	VectorCopy (wishvel, wishdir);   // Determine maginitude of speed of move
	wishspeed = VectorNormalize(wishdir);

	//
	// Clamp to server defined max speed
	//
	if (wishspeed > mv->m_flMaxSpeed)
	{
		VectorScale (wishvel, mv->m_flMaxSpeed/wishspeed, wishvel);
		wishspeed = mv->m_flMaxSpeed;
	}

	// Set pmove velocity
	mv->m_vecVelocity[2] = 0;
	Accelerate ( wishdir, wishspeed, sv_accelerate.GetFloat() );
	mv->m_vecVelocity[2] = 0;

	// Add in any base velocity to the current velocity.
	VectorAdd (mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );

	spd = VectorLength( mv->m_vecVelocity );

	if (spd < 1.0f)
	{
		mv->m_vecVelocity.Init();
		return;
	}

	// first try just moving to the destination	
	dest[0] = mv->GetAbsOrigin()[0] + mv->m_vecVelocity[0]*gpGlobals->frametime;
	dest[1] = mv->GetAbsOrigin()[1] + mv->m_vecVelocity[1]*gpGlobals->frametime;	
	dest[2] = mv->GetAbsOrigin()[2];

	// first try moving directly to the next spot
	VectorCopy (dest, start);

	TracePlayerBBox( mv->GetAbsOrigin(), dest, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, pm );

	// If we made it all the way, then copy trace end
	//  as new player position.

	mv->m_outWishVel += wishdir * wishspeed;

	if (pm.fraction == 1)
	{
		mv->SetAbsOrigin(pm.endpos);
		return;
	}

	if (oldground == NULL &&   // Don't walk up stairs if not on ground.
		player->GetWaterLevel()  == 0)
		return;

	if (player->m_flWaterJumpTime)         // If we are jumping out of water, don't do anything more.
		return;

	// Try sliding forward both on ground and up 16 pixels
	//  take the move that goes farthest
	VectorCopy (mv->GetAbsOrigin(), original);       // Save out original pos &
	VectorCopy (mv->m_vecVelocity, originalvel);  //  velocity.

	// Slide move
	clip = TryPlayerMove();

	// Copy the results out
	VectorCopy (mv->GetAbsOrigin()  , down);
	VectorCopy (mv->m_vecVelocity, downvel);

	// Reset original values.
	mv->SetAbsOrigin(original);
	VectorCopy (originalvel, mv->m_vecVelocity);

	// Start out up one stair height
	VectorCopy (mv->GetAbsOrigin(), dest);
	dest[2] += player->m_Local.m_flStepSize;

	TracePlayerBBox( mv->GetAbsOrigin(), dest, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, pm );

	// If we started okay and made it part of the way at least,
	//  copy the results to the movement start position and then
	//  run another move try.
	if (!pm.startsolid && !pm.allsolid)
		mv->SetAbsOrigin(pm.endpos);

	// slide move the rest of the way.
	clip = TryPlayerMove();

	// Now try going back down from the end point
	//  press down the stepheight
	VectorCopy (mv->GetAbsOrigin(), dest);
	dest[2] -= player->m_Local.m_flStepSize;

	TracePlayerBBox( mv->GetAbsOrigin(), dest, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, pm );

	// If we are not on the ground any more then
	//  use the original movement attempt
	if ( pm.plane.normal[2] < IMPACT_NORMAL_FLOOR )
		goto usedown;
	// If the trace ended up in empty space, copy the end
	//  over to the origin.
	if (!pm.startsolid && !pm.allsolid)
		mv->SetAbsOrigin(pm.endpos);

	// Copy this origion to up.
	VectorCopy (mv->GetAbsOrigin(), up);

	// decide which one went farther
	downdist = (down[0] - original[0])*(down[0] - original[0])
		     + (down[1] - original[1])*(down[1] - original[1]);
	updist   = (up[0]   - original[0])*(up[0]   - original[0])
		     + (up[1]   - original[1])*(up[1]   - original[1]);

	if (downdist > updist)
	{
usedown:
	;
		mv->SetAbsOrigin(down);
		VectorCopy (downvel, mv->m_vecVelocity);
	}
	else // copy z value from slide move
		mv->m_vecVelocity[2] = downvel[2];

	float stepDist = mv->GetAbsOrigin().z - original.z;
	if ( stepDist > 0 )
		mv->m_outStepHeight += stepDist;
}

void CTFGameMovement::Accelerate( Vector &wishdir, float wishspeed, float accel )
{
	// No acceleration if the player is water-jumping or dead.
	if ( player->pl.deadflag || player->m_flWaterJumpTime )
		return;

	// See if we are changing direction a bit
//	float flCurrentSpeed = mv->m_vecVelocity.Dot( wishdir );
	float flCurrentSpeed = mv->m_vecVelocity.Length();

	// Reduce wishspeed by the amount of veer.
	float flAddSpeed = wishspeed - flCurrentSpeed;

	// If not going to add any speed, done.
	if ( flAddSpeed <= 0.0f )
		return;

	// Determine amount of accleration.
	float flAccelSpeed = accel * gpGlobals->frametime * wishspeed * player->m_surfaceFriction;

	// Cap at addspeed
	if ( flAccelSpeed > flAddSpeed )
		flAccelSpeed = flAddSpeed;

	// Gravity.
	float flGravityAdj = CalcGravityAdjustment( wishdir );

	// Adjust velocity.
	for ( int iAxis = 0; iAxis < 3; iAxis++ )
		mv->m_vecVelocity[iAxis] += ( flAccelSpeed * wishdir[iAxis] * flGravityAdj );
}

float CTFGameMovement::CalcGravityAdjustment( const Vector &wishdir )
{
	// Get the TF movement data.
	CTFMoveData *pTFMove = TFMove();
	if ( !pTFMove )
		return 1.0f;

	if ( player->GetMoveType() == MOVETYPE_NOCLIP )
		return 1.0f;

	Vector vecPositionDelta = pTFMove->m_vecPosDelta;
	VectorNormalize( vecPositionDelta );

	// Hard-code my table
	float flModifier = 0.0f;
	if ( vecPositionDelta.z < -0.342f )
		flModifier = 1.25f;
	else if ( vecPositionDelta.z < 0.0f )
		flModifier = 1.15f;
	else if ( vecPositionDelta.z < 0.342f )	/* >20 */
		flModifier = 1.0f;
	else if ( vecPositionDelta.z < 0.5f ) /* 20-30 */
	{
		float flDelta = 0.5f - vecPositionDelta.z;
		flDelta /= 0.158f;
		flDelta = 1.0f - flDelta;
		flModifier = 1.0f - ( 0.25 * flDelta );
	}
	else if ( vecPositionDelta.z < 0.707f ) /* 30-45 */
	{
		float flDelta = 0.707f - vecPositionDelta.z;
		flDelta /= 0.207f;
		flDelta = 1.0f - flDelta;
		flModifier = 0.75 - ( 0.25f * flDelta );
	}
	else if ( vecPositionDelta.z < 0.766f ) /* 45-50 */
	{
		float flDelta = 0.766f - vecPositionDelta.z;
		flDelta /= 0.059f;
		flDelta = 1.0f - flDelta;
		flModifier = 0.5f - ( 0.40f * flDelta );
	}
	else
		flModifier = 0.35f;

	AddToMomentumList( flModifier );
	flModifier = GetMomentum();

#if 0
	// Debug!
	if ( player->IsServer() )
	{
		Msg( "Server:Gravity Adj = %lf\n", flModifier );
	}
	else
	{
		Msg( "Client:Gravity Adj = %lf\n", flModifier );
	}
#endif

	return flModifier;
}

bool CTFGameMovement::CalcWishVelocityAndPosition( Vector &vWishPos, Vector &vWishDir,
												   float &flWishSpeed )
{
	//
	// Determine the movement angles.
	//
	Vector vForward, vRight, vUp;
	AngleVectors( mv->m_vecViewAngles, &vForward, &vRight, &vUp );

	//
	// Zero out the z component of the movement vectors and renormalize.
	//
	vForward.z = 0.0f;
	VectorNormalize( vForward );
	vRight.z = 0.0f;
	VectorNormalize( vRight );

	//
	// Determine the xy parts of the velocity.
	//
	Vector vWishVel( 0.0f, 0.0f, 0.0f );
	for ( int axis = 0; axis < 2; axis++ )
	{
		vWishVel[axis] = ( vForward[axis] * mv->m_flForwardMove ) +
			             ( vRight[axis] * mv->m_flSideMove );
	}
	vWishVel.z = 0.0f;

	//
	// Componentize the velocity into direction and speed.
	//
	VectorCopy( vWishVel, vWishDir );
	flWishSpeed = VectorNormalize( vWishDir );
	if ( flWishSpeed > mv->m_flMaxSpeed )
	{
		VectorScale( vWishVel, ( mv->m_flMaxSpeed / flWishSpeed ), vWishVel );
		flWishSpeed = mv->m_flMaxSpeed;
	}

	//
	// Accelerate (in the plane).
	//
	mv->m_vecVelocity.z = 0.0f;
	Accelerate( vWishDir, flWishSpeed, sv_accelerate.GetFloat() );
	mv->m_vecVelocity.z = 0.0f;

	// Add in any base velocity (from conveyers, etc.) to the current velocity.
	VectorAdd( mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );

	//
	// Stop the player (zero out velocity) if the player's speed is below a
	// given threshold.
	//
	float flSpeed = VectorLength( mv->m_vecVelocity );
	if ( flSpeed < SPEED_STOP_THRESHOLD )
	{
		mv->m_vecVelocity.Init();
		return false;
	}

	//
	// Calculate the wish position.
	//
	vWishPos.x = mv->GetAbsOrigin().x + ( mv->m_vecVelocity.x * gpGlobals->frametime );
	vWishPos.y = mv->GetAbsOrigin().y + ( mv->m_vecVelocity.y * gpGlobals->frametime );
	vWishPos.z = mv->GetAbsOrigin().z;

	return true;
}

inline void CTFGameMovement::TracePlayerBBoxWithStep( const Vector &vStart, const Vector &vEnd, 
							unsigned int fMask, int collisionGroup, trace_t &trace )
{
	VPROF( "CTFGameMovement::TracePlayerBBoxWithStep" );

	Vector vHullMin = GetPlayerMins( player->m_Local.m_bDucked );
	vHullMin.z += player->m_Local.m_flStepSize;
	Vector vHullMax = GetPlayerMaxs( player->m_Local.m_bDucked );

	Ray_t ray;
	ray.Init( vStart, vEnd, vHullMin, vHullMax );
	UTIL_TraceRay( ray, fMask, mv->m_nPlayerHandle.Get(), collisionGroup, &trace );
}

inline int CTFGameMovement::BlockerType( const Vector &vImpactNormal )
{
	// If the impact plane has a high z component in the normal, then
	// it is probably a floor.
	if ( vImpactNormal.z > IMPACT_NORMAL_FLOOR )
		return 1;

	// If the impact plane has a zero z component in the normal, then it is
	// probably a step or wall.
	if ( vImpactNormal.z == IMPACT_NORMAL_WALL )
		return 2;

	// None
	return 0;
}

bool CTFGameMovement::RedirectGroundVelocity( const trace_t &trace )
{
	// Check for max planes.
	if ( m_nImpactPlaneCount >= MAX_IMPACT_PLANES )
	{
		mv->m_vecVelocity.Init();
		return false;
	}

	// Add the impact plane normal to the list.
	VectorCopy( trace.plane.normal, m_aImpactPlaneNormals[m_nImpactPlaneCount] );
	m_nImpactPlaneCount++;
	
	int iPlane, iPlane2;
	for ( iPlane = 0; iPlane < m_nImpactPlaneCount; iPlane++ )
	{
		// Reduce and redirect the player's velocity.
		Vector vecVelocity( mv->m_vecVelocity.x, mv->m_vecVelocity.y, mv->m_vecVelocity.z );

		// Don't let negatively sloped surfaces drive you into the ground.
		if ( m_aImpactPlaneNormals[iPlane].z < 0.0f )
		{
			m_aImpactPlaneNormals[iPlane].z = 0.0f;
			VectorNormalize( m_aImpactPlaneNormals[iPlane] );
		}

		ClipVelocity( vecVelocity, m_aImpactPlaneNormals[iPlane], mv->m_vecVelocity, 1.0f );

		// Check to see if we need to continue clipping?
		for( iPlane2 = 0; iPlane2 < m_nImpactPlaneCount; iPlane2++ )
		{
			if ( iPlane != iPlane2 )
			{
				if ( mv->m_vecVelocity.Dot( m_aImpactPlaneNormals[iPlane2] ) < 0.0f )
					break;
			}
		}
		if ( iPlane2 == m_nImpactPlaneCount )
			break;
	}

	if ( iPlane == m_nImpactPlaneCount )
	{
		// Go along the crease here!
		if ( m_nImpactPlaneCount != 2 )
		{
			mv->m_vecVelocity.Init();
			return false;
		}
		else
		{
			Vector vecCross;
			CrossProduct( m_aImpactPlaneNormals[0], m_aImpactPlaneNormals[1], vecCross );
			float flScalar = vecCross.Dot( mv->m_vecVelocity );
			VectorScale( vecCross, flScalar, mv->m_vecVelocity );
		}
	}

	// If the original velocity is against the current velocity, stop dead to 
	// avoid tiny occilations in sloping corners
	if ( mv->m_vecVelocity.Dot( m_vecOriginalVelocity ) <= 0.0f )
	{
		mv->m_vecVelocity.Init();
		return false;
	}

	return true;
}

bool CTFGameMovement::RedirectAirVelocity( const trace_t &trace )
{
	// Check for max planes.
	if ( m_nImpactPlaneCount >= MAX_IMPACT_PLANES )
	{
		mv->m_vecVelocity.Init();
		return false;
	}

	// Add the impact plane normal to the list.
	VectorCopy( trace.plane.normal, m_aImpactPlaneNormals[m_nImpactPlaneCount] );
	m_nImpactPlaneCount++;

	for ( int iPlane = 0; iPlane < m_nImpactPlaneCount; iPlane++ )
	{
		// Reduce and redirect the player's velocity.
		Vector vecVelocity( mv->m_vecVelocity.x, mv->m_vecVelocity.y, mv->m_vecVelocity.z );

		// Don't let negatively sloped surfaces drive you into the ground.
		if ( m_aImpactPlaneNormals[iPlane].z < 0.0f )
		{
			m_aImpactPlaneNormals[iPlane].z = 0.0f;
			VectorNormalize( m_aImpactPlaneNormals[iPlane] );
		}

		if ( m_aImpactPlaneNormals[iPlane].z > IMPACT_NORMAL_FLOOR )
		{
			ClipVelocity( vecVelocity, m_aImpactPlaneNormals[iPlane], mv->m_vecVelocity, 1.0f );
		}
		else
		{
			ClipVelocity( vecVelocity, m_aImpactPlaneNormals[iPlane], mv->m_vecVelocity, 1.0f + sv_bounce.GetFloat() * ( 1.0f - player->m_surfaceFriction ) );
		}
	}	

	return true;
}

void CTFGameMovement::CollisionResponseNone( const trace_t &trace )
{
	// Move the player to the trace's ending position.
	mv->SetAbsOrigin(trace.endpos);

	// Add the position to the stack.
	if ( m_nMovementStackSize < MOVEMENTSTACK_MAXSIZE )
	{
		VectorCopy( mv->GetAbsOrigin(), m_aMovementStack[m_nMovementStackSize].m_vecPosition );
		VectorCopy( mv->m_vecVelocity, m_aMovementStack[m_nMovementStackSize].m_vecVelocity );
		VectorCopy( m_aMovementStack[m_nMovementStackSize].m_vecImpactNormal, 
			m_aMovementStack[m_nMovementStackSize].m_vecImpactNormal );
		m_nMovementStackSize++;
	}
}

void CTFGameMovement::CollisionResponseStuck( void )
{
	mv->m_vecVelocity.Init();
	//DevMsg( 1, "CollisionResponseStuck: %s is stuck (%s).\n", player->GetClassname(), player->IsServer() ? "sv" : "cl" );
}

bool CTFGameMovement::CollisionResponseGeneric( const trace_t &trace, int &nBlocked )
{
	// Check for any movement.
	if ( trace.fraction > 0.0f )
	{
		// Move the partial move and reset the impact plane count.
		mv->SetAbsOrigin(trace.endpos);
		m_nImpactPlaneCount = 0;

		// Add the data to the movement stack. -- the velocity is wrong here!!!
		if ( m_nMovementStackSize < MOVEMENTSTACK_MAXSIZE )
		{
			VectorCopy( mv->GetAbsOrigin(), m_aMovementStack[m_nMovementStackSize].m_vecPosition );
			VectorCopy( mv->m_vecVelocity, m_aMovementStack[m_nMovementStackSize].m_vecVelocity );
			VectorCopy( trace.plane.normal, m_aMovementStack[m_nMovementStackSize].m_vecImpactNormal );
			m_nMovementStackSize++;
		}
	}

	// Sanity check - Impact plane count.
	Assert( m_nImpactPlaneCount < MAX_IMPACT_PLANES );

	// Add the entity to the touched list (list itself maintains uniqueness).
	MoveHelper()->AddToTouched( trace, mv->m_vecVelocity );

	// Check for special "blockers" (walls, steps, floor).
    nBlocked |= BlockerType( trace.plane.normal );
	
	// Re-direct or bounce the player's velocity vector.
	if ( player->GetGroundEntity() != NULL )
		return RedirectGroundVelocity( trace );
	else
		return RedirectAirVelocity( trace );
}

//-----------------------------------------------------------------------------
// Purpose: See comment _WalkMove
//-----------------------------------------------------------------------------
int CTFGameMovement::TryPlayerMove( Vector *pFirstDest, trace_t *pFirstTrace ) {
	VPROF( "CTFGameMovement::TryPlayerMove" );

	return BaseClass::TryPlayerMove( pFirstDest, pFirstTrace );
}

int CTFGameMovement::TryPlayerMove2( void )
{
	VPROF( "CTFGameMovement::TryPlayerMove2" );

	int	nBlocked = MOVEMENT_BLOCKED_NONE;
	trace_t	trace;
	float flTimeLeft = gpGlobals->frametime;
	m_nImpactPlaneCount = 0;

	// Save off the original velocity -- coming in.
	VectorCopy( mv->m_vecVelocity, m_vecOriginalVelocity );

	float flTotalFraction = 0.0f;
	for ( int iBump = 0; iBump < BUMP_MAX_COUNT; iBump++ )
	{
		// Check to see if we have any velocity left. EXPENSIVE!!!!
		if ( mv->m_vecVelocity.Length() == 0.0f )
			break;

		//
		// Calculate the wish position. 
		//
		Vector vecWishPos;
		VectorMA( mv->GetAbsOrigin(), flTimeLeft, mv->m_vecVelocity, vecWishPos );

		//
		// Attempt the move.
		//
		// NOTE:  This check can hit players and objects
		TracePlayerBBoxWithStep( mv->GetAbsOrigin(), vecWishPos, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER, trace );

		//  but if it didn't no need to do another trace for movement only
		if ( trace.fraction == 1.0f )
		{
			flTotalFraction += trace.fraction;
			CollisionResponseNone( trace );
			break;
		}
		else
		{
			// Add the entity to the touched list (list itself maintains uniqueness).
			MoveHelper()->AddToTouched( trace, mv->m_vecVelocity );
			// Do a non-solid to player trace now, instead, and override what's in trace
			TracePlayerBBoxWithStep( mv->GetAbsOrigin(), vecWishPos, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, trace );
		}

		flTotalFraction += trace.fraction;

		// Handle stuck in solid.
		if ( trace.allsolid )
		{
			CollisionResponseStuck();
			return MOVEMENT_BLOCKED_ALL;
		}

		// Handle full movement.
		if ( trace.fraction == 1.0f )
		{
			CollisionResponseNone( trace );
			break;
		}

		// Handle partial movement.
		if ( !CollisionResponseGeneric( trace, nBlocked ) )
			break;

		// Reduce the time left.
		flTimeLeft -= ( flTimeLeft * trace.fraction );

		/*
		//
		// Attempt the move.
		//
		TracePlayerBBoxWithStep( mv->m_vecOrigin, vecWishPos, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, trace );

		flTotalFraction += trace.fraction;

		// Handle stuck in solid.
		if ( trace.allsolid )
		{
			CollisionResponseStuck();
			return MOVEMENT_BLOCKED_ALL;
		}

		// Handle full movement.
		if ( trace.fraction == 1.0f )
		{
			CollisionResponseNone( trace );
			break;
		}

		// Handle partial movement.
		if ( !CollisionResponseGeneric( trace, nBlocked ) )
			break;

		// Reduce the time left.
		flTimeLeft -= ( flTimeLeft * trace.fraction );
		*/
	}
	
	// Finally, check for any movement.
	if ( flTotalFraction == 0.0f )
		CollisionResponseStuck();

	return nBlocked;
}

void CTFGameMovement::TryStanding( void )
{
	VPROF( "CTFGameMovement::TryStanding" );

	// Step down.
	Vector vecStandPos( mv->GetAbsOrigin().x, mv->GetAbsOrigin().y, mv->GetAbsOrigin().z - player->m_Local.m_flStepSize );

	trace_t trace;
	TracePlayerBBoxWithStep( mv->GetAbsOrigin(), vecStandPos, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, trace );
	if ( trace.fraction == 1.0f )
		return;

	// Attempt to stand up.
	vecStandPos.z = mv->GetAbsOrigin().z + ( ( 1.0f - trace.fraction ) * player->m_Local.m_flStepSize );
	TracePlayerBBoxWithStep( mv->GetAbsOrigin(), vecStandPos, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, trace );

	// A fraction of 1.0 means we stood up fine - done.
	if ( trace.fraction == 1.0f )
	{
		mv->SetAbsOrigin(trace.endpos);
		return;
	}

	// Use the movement stack to pop back and resolve the stand.
	if ( m_nMovementStackSize > 0 )
	{
		mv->SetAbsOrigin(m_aMovementStack[m_nMovementStackSize-1].m_vecPosition);
		VectorCopy( m_aMovementStack[m_nMovementStackSize-1].m_vecVelocity, mv->m_vecVelocity );
		m_nMovementStackSize--; 
		TryStanding();
	}
}

void CTFGameMovement::ResolveStanding( void )
{
	VPROF( "CTFGameMovement::ResolveStanding" );

	//
	// Attempt to move down twice your step height.  Anything between 0.5 and 1.0
	// is a valid "stand" value.
	//
	Vector vecStandPos( mv->GetAbsOrigin().x, mv->GetAbsOrigin().y, mv->GetAbsOrigin().z - ( player->m_Local.m_flStepSize * 2.0f ) );

	trace_t trace;
	TracePlayerBBoxWithStep( mv->GetAbsOrigin(), vecStandPos, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, trace );

	// Anything between 0.5 and 1.0 is a valid stand value
	if ( fabs( trace.fraction - 0.5 ) < 0.0004f )
		return;

//	if ( trace.fraction == 0.5 )
//	{
//		VectorCopy( trace.endpos, mv->m_vecOrigin );
//		return;
//	}

	if ( trace.fraction > 0.5f )
	{
		trace.fraction -= 0.5f;
		Vector vecNewOrigin;
		vecNewOrigin = mv->GetAbsOrigin() + trace.fraction * ( vecStandPos - mv->GetAbsOrigin() );
		mv->SetAbsOrigin(vecNewOrigin);
		return;
	}

	// Less than 0.5 mean we need to attempt to push up the difference.
	vecStandPos.z = ( mv->GetAbsOrigin().z + ( ( 0.5f - trace.fraction ) * ( player->m_Local.m_flStepSize * 2.0f ) ) );
	TracePlayerBBoxWithStep( mv->GetAbsOrigin(), vecStandPos, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, trace );
	
	// A fraction of 1.0 means we stood up fine - done.
	if ( trace.fraction == 1.0f )
	{
		mv->SetAbsOrigin(trace.endpos);
		return;
	}

	// Use the movement stack to pop back and resolve the stand.
	if ( m_nMovementStackSize > 0 )
	{
		mv->SetAbsOrigin(m_aMovementStack[m_nMovementStackSize-1].m_vecPosition);
		VectorCopy( m_aMovementStack[m_nMovementStackSize-1].m_vecVelocity, mv->m_vecVelocity );
		m_nMovementStackSize--; 
		ResolveStanding();
	}
}

void CTFGameMovement::SetMomentumList( float flValue )
{
	// Get the TF movement data.
	CTFMoveData *pTFMove = TFMove();
	if ( !pTFMove )
		return;

	// Fill in the momentum list.
	pTFMove->m_iMomentumHead = 0;
	for ( int iMomentum = 0; iMomentum < CTFMoveData::MOMENTUM_MAXSIZE; iMomentum++ )
		pTFMove->m_aMomentum[iMomentum] = flValue;
}

void CTFGameMovement::AddToMomentumList( float flValue )
{
	// Get the TF movement data.
	CTFMoveData *pTFMove = TFMove();
	if ( !pTFMove )
		return;

	// Insert the new gravity value into the list.
	pTFMove->m_aMomentum[pTFMove->m_iMomentumHead] = flValue;
	pTFMove->m_iMomentumHead = ( pTFMove->m_iMomentumHead + 1 ) % CTFMoveData::MOMENTUM_MAXSIZE;
}

float CTFGameMovement::GetMomentum( void )
{
	// Get the TF movement data.
	CTFMoveData *pTFMove = TFMove();
	if ( !pTFMove )
		return 1.0f;

	float flTotal = 0.0f;
	for ( int iMomentum = 0; iMomentum < CTFMoveData::MOMENTUM_MAXSIZE; iMomentum++ )
		flTotal += pTFMove->m_aMomentum[iMomentum];
	
	flTotal /= CTFMoveData::MOMENTUM_MAXSIZE;

	return flTotal;
}
