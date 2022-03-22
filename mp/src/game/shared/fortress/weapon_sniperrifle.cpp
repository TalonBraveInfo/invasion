/* Copyright (C) 2020 Mark Sowden <markelswo@gmail.com>
** Project Invasion
*/

#include "cbase.h"
#include "weapon_combat_usedwithshieldbase.h"
#include "in_buttons.h"
#include "takedamageinfo.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "tf_gamerules.h"

// Damage CVars
ConVar weapon_sniperrifle_damage( "weapon_sniperrifle_damage", "50", FCVAR_REPLICATED, "Sniper damage per pellet" );
ConVar weapon_sniperrifle_range( "weapon_sniperrifle_range", "10000", FCVAR_REPLICATED, "Sniper maximum range" );
ConVar weapon_sniperrifle_rate( "weapon_sniperrifle_rate", "1.15", FCVAR_REPLICATED, "Sniper rate of fire" );

#if defined( CLIENT_DLL )
#include "hud.h"
#include "fx.h"
#define CWeaponSniperRifle C_WeaponSniperRifle
extern ConVar zoom_sensitivity_ratio;
extern ConVar default_fov;
#else
#endif

// Time taken to fully wind up/down

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CWeaponSniperRifle : public CWeaponCombatUsedWithShieldBase {
	DECLARE_CLASS( CWeaponSniperRifle, CWeaponCombatUsedWithShieldBase );
public:
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponSniperRifle( void );

	void Precache() override;

	const Vector &GetBulletSpread( void ) override;
	void ItemPostFrame( void ) override;

	void PrimaryAttack() override;
	void SecondaryAttack() override;

	void AddViewKick( void ) override;
	float GetFireRate( void ) override;
	void BulletWasFired( const Vector &vecStart, const Vector &vecEnd ) override;

	// All predicted weapons need to implement and return true
	virtual bool	IsPredicted( void ) const {
		return true;
	}

#if defined( CLIENT_DLL )
public:
	virtual bool	ShouldPredict( void ) {
		if( GetOwner() &&
			GetOwner() == C_BasePlayer::GetLocalPlayer() )
			return true;

		return BaseClass::ShouldPredict();
	}

	virtual bool	OnFireEvent( C_BaseViewModel *pViewModel, const Vector &origin, const QAngle &angles, int event, const char *options );
	virtual void	OnDataChanged( DataUpdateType_t updateType );
	virtual void	ClientThink( void );
#endif

public:
	//float	m_flOwnersMaxSpeed;
	//CNetworkVar( float, m_flRotationSpeed );	// When 1, firing commences.
	bool m_bZoomed;
	CNetworkVar( float, m_flCharge );
	bool	m_bSoundPlaying;

private:
	CWeaponSniperRifle( const CWeaponSniperRifle & );
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWeaponSniperRifle::CWeaponSniperRifle( void ) {
	SetPredictionEligible( true );
	//m_flRotationSpeed = 0;
	m_bSoundPlaying = false;
}

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponSniperRifle, DT_WeaponSniperRifle )

BEGIN_NETWORK_TABLE( CWeaponSniperRifle, DT_WeaponSniperRifle )
#if !defined( CLIENT_DLL )
//SendPropFloat( SENDINFO( m_flRotationSpeed ), 8, SPROP_ROUNDDOWN, 0, 1 ),
SendPropFloat( SENDINFO( m_flCharge ), 8, SPROP_ROUNDDOWN, 0, 1 ),
#else
//RecvPropFloat( RECVINFO( m_flRotationSpeed ) ),
RecvPropFloat( RECVINFO( m_flCharge ) ),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL

BEGIN_PREDICTION_DATA( CWeaponSniperRifle )
//DEFINE_PRED_FIELD_TOL( m_flRotationSpeed, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, 0.01f ),
END_PREDICTION_DATA()

#endif

LINK_ENTITY_TO_CLASS( weapon_sniperrifle, CWeaponSniperRifle );
PRECACHE_WEAPON_REGISTER( weapon_sniperrifle );

void CWeaponSniperRifle::Precache() {
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: Get the accuracy derived from weapon and player, and return it
//-----------------------------------------------------------------------------
const Vector &CWeaponSniperRifle::GetBulletSpread( void ) {
	static Vector v = VECTOR_CONE_1DEGREES;
	return v;
}

void CWeaponSniperRifle::ItemPostFrame( void ) {
	CBaseTFPlayer *pOwner = ToBaseTFPlayer( GetOwner() );
	if( !pOwner )
		return;

	// This should work, and avoids sending extra network data. If it doesn't, we'll have to send down the unchanged max speed.
	//if( !m_flRotationSpeed ) {
	//	m_flOwnersMaxSpeed = pOwner->MaxSpeed();
	//}

	//float flLastRotationSpeed = m_flRotationSpeed;

	CheckReload();

	// Handle firing
	if( ( pOwner->m_nButtons & IN_ATTACK ) && ( m_flNextPrimaryAttack <= gpGlobals->curtime ) ) {
		if( m_iClip1 > 0 ) {
			PrimaryAttack();
		} else {
			Reload();
		}
	}

	if ( ( pOwner->m_nButtons & IN_ATTACK2 ) && ( m_flNextSecondaryAttack <= gpGlobals->curtime ) ) {
		SecondaryAttack();
	}

	// Reload button (or fire button when we're out of ammo)
	if( m_flNextPrimaryAttack <= gpGlobals->curtime ) {
		if( pOwner->m_nButtons & IN_RELOAD ) {
			Reload();
		} else if( !( ( pOwner->m_nButtons & IN_ATTACK ) || ( pOwner->m_nButtons & IN_ATTACK2 ) || ( pOwner->m_nButtons & IN_RELOAD ) ) ) {
			if( !m_iClip1 && HasPrimaryAmmo() ) {
				Reload();
			} else {
				//ReduceRotation();
			}
		}
	}

	// If the speed changed, modify our movement speed
	//if( m_flRotationSpeed != flLastRotationSpeed ) {
	//	pOwner->SetMaxSpeed( m_flOwnersMaxSpeed * ( 1.0 - ( m_flRotationSpeed * 0.5 ) ) );
	//}

	WeaponIdle();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponSniperRifle::PrimaryAttack( void ) {
	CBaseTFPlayer *pPlayer = (CBaseTFPlayer *)GetOwner();
	if( !pPlayer )
		return;

	// Fire the bullets
	Vector vecSrc = pPlayer->Weapon_ShootPosition();
	Vector vecAiming;
	pPlayer->EyeVectors( &vecAiming );

	PlayAttackAnimation( GetPrimaryAttackActivity() );

	// Make a satisfying force, and knock them into the air
	float flForceScale = ( 100 ) * 75 * 4;
	Vector vecForce = vecAiming;
	vecForce.z += 0.7;
	vecForce *= flForceScale;

	CTakeDamageInfo info( this, pPlayer, vecForce, vec3_origin, weapon_sniperrifle_damage.GetFloat(), DMG_BULLET | DMG_BUCKSHOT );
	TFGameRules()->FireBullets( info, 1, vecSrc, vecAiming, GetBulletSpread(), weapon_sniperrifle_range.GetFloat(), m_iPrimaryAmmoType, 0, entindex(), 0 );

	AddViewKick();
	
	WeaponSound( SINGLE );

	m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();
	m_iClip1 = m_iClip1 - 1;
}

/**
 * Secondary attack controls the rifle zoom.
 */
void CWeaponSniperRifle::SecondaryAttack() {
	CBaseTFPlayer *player = (CBaseTFPlayer *)GetOwner();
	if( player == nullptr ) {
		return;
	}

	if ( !m_bZoomed ) {
		WeaponSound( SPECIAL1 );
		player->SetFOV( this, 20, 0.2f, 0 );
		m_bZoomed = true;
	} else {
		WeaponSound( SPECIAL2 );
		player->SetFOV( this, 0, 0.2f, 0 );
		m_bZoomed = false;
	}
	m_flNextSecondaryAttack = gpGlobals->curtime + 0.5f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponSniperRifle::AddViewKick( void ) {
	// Get the view kick
	CBaseTFPlayer *player = ToBaseTFPlayer( GetOwner() );
	if( !player ) {
		return;
	}

	QAngle viewPunch( SHARED_RANDOMFLOAT( -1.0f, -3.00f ), 0.0f, 0.0f );
	if( player->GetFlags() & FL_DUCKING ) {
		viewPunch *= 0.25;
	}

	player->ViewPunch( viewPunch );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CWeaponSniperRifle::GetFireRate( void ) {
	return weapon_sniperrifle_rate.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose: Draw the minigun effect
//-----------------------------------------------------------------------------
void CWeaponSniperRifle::BulletWasFired( const Vector &vecStart, const Vector &vecEnd ) {
	UTIL_Tracer( (Vector &)vecStart, (Vector &)vecEnd, entindex(), 1, 5000, false, "MinigunTracer" );
}

#if defined ( CLIENT_DLL )
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponSniperRifle::OnFireEvent( C_BaseViewModel *pViewModel, const Vector &origin, const QAngle &angles, int event, const char *options ) {
	// Suppress the shell ejection from the HL2 model we're using for prototyping
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : updateType - 
//-----------------------------------------------------------------------------
void CWeaponSniperRifle::OnDataChanged( DataUpdateType_t updateType ) {
	BaseClass::OnDataChanged( updateType );

	if( updateType == DATA_UPDATE_CREATED ) {
		ClientThinkList()->SetNextClientThink( GetClientHandle(), CLIENT_THINK_ALWAYS );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponSniperRifle::ClientThink() {
	CBaseTFPlayer *pPlayer = (CBaseTFPlayer *)GetOwner();
	if( !pPlayer )
		return;

	/*if( m_flRotationSpeed ) {
		WeaponSound_t nSound = SPECIAL1;

		// If we're firing, play that sound instead
		if( m_flRotationSpeed >= 0.99 ) {
			nSound = SINGLE;
		} else {
			m_bSoundPlaying = true;
		}

		// If we have some sounds from the weapon classname.txt file, play a random one of them
		const char *shootsound = GetShootSound( nSound );
		if( !shootsound || !shootsound[ 0 ] )
			return;

		CSoundParameters params;
		if( !GetParametersForSound( shootsound, params, NULL ) )
			return;

		// Shift pitch according to barrel rotation
		float flPitch = 30 + ( 90 * m_flRotationSpeed );

		CPASAttenuationFilter filter( GetOwner(), params.soundlevel );
		Vector vecOrigin = GetOwner()->GetAbsOrigin();

		EmitSound_t ep;
		ep.m_nChannel = CHAN_WEAPON;
		ep.m_pSoundName = shootsound;
		ep.m_flVolume = params.volume;
		ep.m_SoundLevel = params.soundlevel;
		ep.m_nFlags = SND_CHANGE_PITCH;
		ep.m_nPitch = (int)flPitch;
		ep.m_pOrigin = &vecOrigin;


		EmitSound( filter, GetOwner()->entindex(), ep );
	} else if( m_bSoundPlaying ) {
		m_bSoundPlaying = false;
		StopWeaponSound( SPECIAL1 );
		StopWeaponSound( SINGLE );
	}*/
}

#endif

#if 0
/* Alien variant */

#define CWeaponSniperRifleAlien C_WeaponSniperRifleAlien

class CWeaponSniperRifleAlien : public CWeaponSniperRifle {
	DECLARE_CLASS( CWeaponSniperRifleAlien, CWeaponSniperRifle );

public:
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponSniperRifleAlien() {}

private:
	CWeaponSniperRifleAlien( const CWeaponSniperRifleAlien & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponSniperRifleAlien, DT_WeaponSniperRifleAlien )

BEGIN_NETWORK_TABLE( CWeaponSniperRifleAlien, DT_WeaponSniperRifleAlien )
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeaponSniperRifleAlien )
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS( weapon_sniperrifle_alien, CWeaponSniperRifleAlien );
PRECACHE_WEAPON_REGISTER( weapon_sniperrifle_alien );
#endif
