
// Purpose: Game-specific impact effect hooks

#include "cbase.h"
#include "fx_impact.h"
#include "decals.h"
#include "IEffects.h"
#include "c_breakableprop.h"
#include "tempent.h"
#include "c_te_legacytempents.h"
#include "tf_shareddefs.h"
#include "fx.h"

void ImpactCreateHurtShards( Vector &vecOrigin, trace_t &tr, Vector &shotDir, int iMaterial, bool bLarge, bool bBlood );

void MakeHurt( const CEffectData &data, bool bBlood )
{
	trace_t tr;
	Vector vecOrigin, vecStart, vecShotDir;
	int iMaterial, iDamageType, iHitbox;
	short nSurfaceProp;
	C_BaseEntity *pEntity = ParseImpactData( data, &vecOrigin, &vecStart, &vecShotDir, nSurfaceProp, iMaterial, iDamageType, iHitbox );

	if ( !pEntity )
		return;

	// If we hit, perform our custom effects and play the sound
	if ( Impact( vecOrigin, vecStart, iMaterial, iDamageType, iHitbox, pEntity, tr ) )
	{
		// Throw out shards to show the player he's hurting it
		ImpactCreateHurtShards( vecOrigin, tr, vecShotDir, iMaterial, false, bBlood );

		// Check for custom effects based on the Decal index
		PerformCustomEffects( vecOrigin, tr, vecShotDir, iMaterial, 1.0 );
	}

	PlayImpactSound( pEntity, tr, vecOrigin, nSurfaceProp );
}

//-----------------------------------------------------------------------------
// Purpose: Impact for:
//				Bullets hitting targets that CAN be hurt
//-----------------------------------------------------------------------------
void ImpactCallback( const CEffectData &data )
{
	MakeHurt( data, true );
}

DECLARE_CLIENT_EFFECT( "Impact", ImpactCallback );

//-----------------------------------------------------------------------------
// Purpose: Bloodless Impact for:
//				Bullets hitting targets that CAN be hurt
//-----------------------------------------------------------------------------
void ImpactNoBloodCallback( const CEffectData &data )
{
	MakeHurt( data, false );
}

DECLARE_CLIENT_EFFECT( "ImpactNoBlood", ImpactNoBloodCallback );

//-----------------------------------------------------------------------------
// Purpose: Impact for:
//				Bullets hitting targets that CANNOT be hurt
//-----------------------------------------------------------------------------
void ImpactUnhurtCallback( const CEffectData &data )
{
	trace_t tr;
	Vector vecOrigin, vecStart, vecShotDir;
	int iMaterial, iDamageType, iHitbox;
	short nSurfaceProp;
	C_BaseEntity *pEntity = ParseImpactData( data, &vecOrigin, &vecStart, &vecShotDir, nSurfaceProp, iMaterial, iDamageType, iHitbox );

	if ( !pEntity )
		return;

	// If we hit, perform our custom effects and play the sound
	// Don't decal team members, but decal everything else

	bool bShouldDecal = !( pEntity && pEntity->IsPlayer() );
	if ( Impact( vecOrigin, vecStart, iMaterial, iDamageType, iHitbox, pEntity, tr, bShouldDecal ? 0 : IMPACT_NODECAL ) )
	{
		// Check for custom effects based on the Decal index
		PerformCustomEffects( vecOrigin, tr, vecShotDir, iMaterial, 0 );
	}

	PlayImpactSound( pEntity, tr, vecOrigin, nSurfaceProp );
}

DECLARE_CLIENT_EFFECT( "ImpactUnhurt", ImpactUnhurtCallback );

//-----------------------------------------------------------------------------
// Purpose: Impact for:
//				Bullets hitting player's handheld shields
//-----------------------------------------------------------------------------
void ImpactShieldCallback( const CEffectData &data )
{
	trace_t tr;
	Vector vecOrigin, vecStart, vecShotDir;
	int iMaterial, iDamageType, iHitbox;
	short nSurfaceProp;
	ParseImpactData( data, &vecOrigin, &vecStart, &vecShotDir, nSurfaceProp, iMaterial, iDamageType, iHitbox );

	// Don't call Impact() because we don't want to decal the player
	Vector reflect = -vecShotDir;
	reflect[0] += random->RandomFloat( -0.5f, 0.5f );
	reflect[1] += random->RandomFloat( -0.5f, 0.5f );
	reflect[2] += random->RandomFloat( 0, 0.5f );
	FX_MetalSpark( vecOrigin, reflect, -vecShotDir, 3 );
}

DECLARE_CLIENT_EFFECT( "ImpactShield", ImpactShieldCallback );

void MakePlasmaHurt( const CEffectData &data, bool bBlood )
{
	trace_t tr;
	Vector vecOrigin, vecStart, vecShotDir;
	int iMaterial, iDamageType, iHitbox;
	short nSurfaceProp;
	C_BaseEntity *pEntity = ParseImpactData( data, &vecOrigin, &vecStart, &vecShotDir, nSurfaceProp, iMaterial, iDamageType, iHitbox );

	if ( !pEntity )
		return;

	// If we hit, play our splash
	if ( Impact( vecOrigin, vecStart, iMaterial, iDamageType, iHitbox, pEntity, tr ) )
	{
		// Throw out shards to show the player he's hurting it
		ImpactCreateHurtShards( vecOrigin, tr, vecShotDir, iMaterial, false, bBlood );

		// Check for custom effects based on the Decal index
		PerformCustomEffects(vecOrigin, tr, vecShotDir, iMaterial, 1.0);
	}

	PlayImpactSound(pEntity, tr, vecOrigin, nSurfaceProp);
}

//-----------------------------------------------------------------------------
// Purpose: Impact for:
//				Plasma hitting targets that CAN be hurt
//-----------------------------------------------------------------------------
void ImpactPlasmaCallback( const CEffectData &data )
{
	MakePlasmaHurt( data, true );
}

DECLARE_CLIENT_EFFECT( "PlasmaHurt", ImpactPlasmaCallback );

//-----------------------------------------------------------------------------
// Purpose: Impact for:
//				Plasma hitting targets that CAN be hurt
//-----------------------------------------------------------------------------
void ImpactPlasmaNoBloodCallback( const CEffectData &data )
{
	MakePlasmaHurt( data, false );
}

DECLARE_CLIENT_EFFECT( "PlasmaHurtNoBlood", ImpactPlasmaNoBloodCallback );

//-----------------------------------------------------------------------------
// Purpose: Impact for:
//				Plasma hitting targets that CANNOT be hurt
//-----------------------------------------------------------------------------
void ImpactPlasmaUnhurtCallback( const CEffectData &data )
{
	trace_t tr;
	Vector vecOrigin, vecStart, vecShotDir;
	int iMaterial, iDamageType, iHitbox;
	short nSurfaceProp;
	C_BaseEntity *pEntity = ParseImpactData( data, &vecOrigin, &vecStart, &vecShotDir, nSurfaceProp, iMaterial, iDamageType, iHitbox );

	if ( !pEntity )
		return;

	// If we hit, play our splash
	
	bool bShouldDecal = !( pEntity && pEntity->IsPlayer() );
	if ( Impact( vecOrigin, vecStart, iMaterial, iDamageType, iHitbox, pEntity, tr, bShouldDecal ? 0 : IMPACT_NODECAL  ) )
	{
		// Check for custom effects based on the Decal index
		PerformCustomEffects(vecOrigin, tr, vecShotDir, iMaterial, 1.0);

		g_pEffects->EnergySplash( vecOrigin, tr.plane.normal, false );
	}

	PlayImpactSound(pEntity, tr, vecOrigin, nSurfaceProp);
}

DECLARE_CLIENT_EFFECT( "PlasmaUnhurt", ImpactPlasmaUnhurtCallback );

//-----------------------------------------------------------------------------
// Purpose: Impact for:
//				Plasma hitting player's handheld shields
//-----------------------------------------------------------------------------
void ImpactPlasmaShieldCallback( const CEffectData &data )
{
	trace_t tr;
	Vector vecOrigin, vecStart, vecShotDir;
	int iMaterial, iDamageType, iHitbox;
	short nSurfaceProp;
	ParseImpactData( data, &vecOrigin, &vecStart, &vecShotDir, nSurfaceProp, iMaterial, iDamageType, iHitbox );

	// Bounce sparks away from the shield
	// Don't call Impact() because we don't want to decal the player
	Vector offset = vecOrigin - ( vecShotDir * 1.0f );
	Vector vecDir = -vecShotDir + Vector(0,0,1.5);
	g_pEffects->Sparks( offset, 2, 2, &vecDir );
}

DECLARE_CLIENT_EFFECT( "PlasmaShield", ImpactPlasmaShieldCallback );

//-----------------------------------------------------------------------------
// Purpose: Impact for:
//				Strider hitting anything
//-----------------------------------------------------------------------------
void ImpactStriderCallback( const CEffectData &data )
{
	trace_t tr;
	Vector vecOrigin, vecStart, vecShotDir;
	int iMaterial, iDamageType, iHitbox;
	short nSurfaceProp;
	C_BaseEntity *pEntity = ParseImpactData( data, &vecOrigin, &vecStart, &vecShotDir, nSurfaceProp, iMaterial, iDamageType, iHitbox );

	if ( !pEntity )
		return;

	// If we hit, play our splash
	if ( Impact( vecOrigin, vecStart, iMaterial, iDamageType, iHitbox, pEntity, tr ) )
	{
		PerformCustomEffects( vecOrigin, tr, vecShotDir, iMaterial, 5 );
	}
}

DECLARE_CLIENT_EFFECT( "Strider", ImpactStriderCallback );

//-----------------------------------------------------------------------------
// Purpose: Throw out shards from the impact point to show we can hurt the target
//-----------------------------------------------------------------------------
void ImpactCreateHurtShards( Vector &vecOrigin, trace_t &tr, Vector &shotDir, int iMaterial, bool bLarge, bool bBlood )
{
	// Throw out the effect if any of these are true
	if ( tr.surface.flags & (SURF_SKY|SURF_NODRAW|SURF_HINT|SURF_SKIP) )
		return;
	
	int iNumGibs = random->RandomInt( 1, 2 );
	for ( int i = 0; i < iNumGibs; i++ )
	{
		// Select the right chunk for the job
		switch ( iMaterial )
		{
		case CHAR_TEX_FLESH:
			// Spray some blood out
			if ( !bBlood )
				return;

			CEffectData	data;
			data.m_vOrigin = vecOrigin;
			data.m_vNormal = tr.plane.normal;
			data.m_flScale = 4;
			data.m_fFlags = FX_BLOODSPRAY_ALL;
			data.m_hEntity = ClientEntityList().EntIndexToHandle( tr.m_pEnt->entindex() );
			DispatchEffect( "tf2blood", data );
			return;
		}
	}
}
