
// Purpose: Client's CWeaponBuilder class

#include "cbase.h"
#include "hud.h"
#include "in_buttons.h"
#include "clientmode_tfnormal.h"
#include "engine/IEngineSound.h"
#include "imessagechars.h"
#include "weapon_builder.h"
#include "c_weapon__stubs.h"
#include "iinput.h"
#include "ObjectControlPanel.h"
#include <vgui/IVGui.h>

#include "vgui_bitmapimage.h"
#include "vgui_bitmappanel.h"


//-----------------------------------------------------------------------------
// Control screen 
//-----------------------------------------------------------------------------
class CHumanPDAPanel : public CVGuiScreenPanel
{
	DECLARE_CLASS( CHumanPDAPanel, CVGuiScreenPanel );

public:
	CHumanPDAPanel( vgui::Panel *parent, const char *panelName );
	~CHumanPDAPanel();
	virtual bool Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData );
	virtual void OnTick();

private:
	C_BaseCombatWeapon *GetOwningWeapon();

	vgui::Label *m_pObjectName;
	vgui::Label *m_pObjectCost;
	vgui::Label *m_pObjectOnTeamCount;
	vgui::Label *m_pObjectPlacementDetails;

	CBitmapPanel *m_pBitmapPanel;

	BitmapImage *m_pObjectImage;

	int				m_nLastObjectID;
	int				m_nLastObjectCount;
	int				m_nLastObjectCost;

};


DECLARE_VGUI_SCREEN_FACTORY( CHumanPDAPanel, "human_pda" );


//-----------------------------------------------------------------------------
// Constructor: 
//-----------------------------------------------------------------------------
CHumanPDAPanel::CHumanPDAPanel( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, "CHumanPDAPanel", vgui::scheme()->LoadSchemeFromFile( "resource/PDAControlPanelScheme.res", "TFBase" ) ) 
{
	SetMouseInputEnabled( false );

	m_pObjectImage = NULL;

	m_pObjectName = new vgui::Label( this, "ObjectName", "" );
	m_pObjectCost = new vgui::Label( this, "ObjectCost", "" );
	m_pObjectOnTeamCount = new vgui::Label( this, "ObjectOnTeamCount", "" );
	m_pObjectPlacementDetails = new vgui::Label( this, "ObjectPlacementDetails", "" );

	m_pBitmapPanel = new CBitmapPanel( this, "ObjectImage" );
	m_pObjectImage = new BitmapImage();
	m_pObjectImage->UsePanelRenderSize( m_pBitmapPanel->GetVPanel() );
	m_pBitmapPanel->SetImage( m_pObjectImage );

	m_nLastObjectID = -1;
	m_nLastObjectCount = -1;
	m_nLastObjectCost = -1;
}

CHumanPDAPanel::~CHumanPDAPanel()
{
	if ( m_pObjectImage )
	{
		delete m_pObjectImage;
	}
}


//-----------------------------------------------------------------------------
// Initialization 
//-----------------------------------------------------------------------------
bool CHumanPDAPanel::Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData )
{
	// Make sure we get ticked...
	vgui::ivgui()->AddTickSignal( GetVPanel() );

	if (!BaseClass::Init(pKeyValues, pInitData))
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Returns the object it's attached to 
//-----------------------------------------------------------------------------
C_BaseCombatWeapon *CHumanPDAPanel::GetOwningWeapon()
{
	C_BaseEntity *pScreenEnt = GetEntity();
	if (!pScreenEnt)
		return NULL;

	C_BaseEntity *pOwner = pScreenEnt->GetOwnerEntity();
	if (!pOwner)
		return NULL;

	C_BaseViewModel *pViewModel = dynamic_cast< C_BaseViewModel * >( pOwner );
	if ( !pViewModel )
		return NULL;

	return pViewModel->GetOwningWeapon();
}

//-----------------------------------------------------------------------------
// Frame-based update
//-----------------------------------------------------------------------------
void CHumanPDAPanel::OnTick()
{
	BaseClass::OnTick();

	SetVisible( true );

	char buf[256];

	C_BaseCombatWeapon *weapon = GetOwningWeapon();
	if ( !weapon )
		return;

	CWeaponBuilder *builder = dynamic_cast< CWeaponBuilder * >( weapon );
	if ( !builder )
		return;

	CBaseTFPlayer *pOwner = ToBaseTFPlayer( builder->GetOwner() );
	if ( !pOwner )
		return;

	// FIXME: Check build state??

	int objectType = builder->m_iCurrentObject;
	CObjectInfo const *info = GetObjectInfo( objectType );
	if ( !info )
		return;

	int numOwned = pOwner->GetNumObjects(objectType);
	int iCost = CalculateObjectCost( objectType, numOwned, pOwner->GetTeamNumber() );

	if ( m_nLastObjectID == objectType &&
		m_nLastObjectCount == numOwned &&
		m_nLastObjectCost == iCost)
		return;

	m_nLastObjectID = objectType;
	m_nLastObjectCount = numOwned;
	m_nLastObjectCost = iCost;

	Q_snprintf( buf, sizeof( buf ), "hud/menu/%s", info->m_pClassName );
	m_pObjectImage->SetImageFile( buf );
	m_pObjectImage->SetColor( GetFgColor() );
	
	Q_snprintf( buf, 256, "%s", info->m_pStatusName );
	m_pObjectName->SetText( buf );

	Q_snprintf( buf, 256, "Cost:  %i", iCost );
	m_pObjectCost->SetText( buf );

	Q_snprintf( buf, 256, "You own:  %i", numOwned );
	m_pObjectOnTeamCount->SetText( buf );

	Q_snprintf( buf, 256, "%s", info->m_pBuilderPlacementString ? info->m_pBuilderPlacementString : "" );
	m_pObjectPlacementDetails->SetText( buf );
	//m_pObjectPlacementDetails->SizeToContents();
}
