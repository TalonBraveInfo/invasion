#ifndef C_FUNC_RESOURCE_H
#define C_FUNC_RESOURCE_H

#include "CommanderOverlay.h"
#include "hud_minimap.h"


//-----------------------------------------------------------------------------
// Purpose: A resource zone
//-----------------------------------------------------------------------------
class C_ResourceZone : public C_BaseEntity
{
public:
	DECLARE_CLASS(C_ResourceZone, C_BaseEntity);
	DECLARE_CLIENTCLASS();
	DECLARE_ENTITY_PANEL();
	DECLARE_MINIMAP_PANEL();

	C_ResourceZone();
	virtual void	SetDormant( bool bDormant ) override;
	virtual void	OnDataChanged( DataUpdateType_t updateType ) override;
	const char	*GetTargetDescription( void ) const;

public:
	float	m_flClientResources;	// Amount of resources left
	int		m_nResourcesLeft;
};

//-----------------------------------------------------------------------------
// Purpose: A resource chunk spawning point in a resource zone
//-----------------------------------------------------------------------------
class C_ResourceSpawner : public C_BaseAnimating
{
	DECLARE_CLASS( C_ResourceSpawner, C_BaseAnimating );
public:
	DECLARE_CLIENTCLASS();
	DECLARE_ENTITY_PANEL();
	DECLARE_MINIMAP_PANEL();

	C_ResourceSpawner();
	virtual void OnDataChanged( DataUpdateType_t updateType );
	virtual void ReceiveMessage(int classID, bf_read &msg);
	virtual void SpawnEffect( bool bSpawningChunk );
	virtual void ClientThink( void );

	const char *GetTargetDescription(void) const { return "Resource Zone"; }

public:
	bool m_bActive;
};

#endif // C_FUNC_RESOURCE_H

