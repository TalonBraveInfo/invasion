//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: This is a panel which is rendered on top of an entity
//
// $Revision: $
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "vgui_entitypanel.h"
#include "ienginevgui.h"
#include "c_basetfplayer.h"
#include "clientmode_tfnormal.h"
#include "hud_commander_statuspanel.h"
#include <KeyValues.h>
#include "commanderoverlaypanel.h"
#include <vgui/IVGui.h>
#include "cdll_util.h"
#include "view.h"

//-----------------------------------------------------------------------------
// constructor
//-----------------------------------------------------------------------------

CEntityPanel::CEntityPanel( vgui::Panel *pParent, const char *panelName )
: BaseClass( pParent, panelName )
{
	SetPaintBackgroundEnabled( false );

	m_pBaseEntity = NULL;

	// FIXME: ComputeParent is yucky... can we be rid of it?
	if (!pParent)
	{
		ComputeParent();
	}

	m_szMouseOverText[ 0 ] = 0;

	// Send mouse inputs (but not cursorenter/exit for now) up to parent
	SetReflectMouse( true );

	m_bShowInNormal = false;

	m_flScale = 0;
	m_OffsetX = 0;
	m_OffsetY = 0;
}

//-----------------------------------------------------------------------------
// Attach to a new entity
//-----------------------------------------------------------------------------
void CEntityPanel::SetEntity( C_BaseEntity* pEntity )
{
	m_pBaseEntity = pEntity;

	// Recompute position
	OnTick();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEntityPanel::ComputeParent( void )
{
	vgui::VPANEL parent = NULL;

	if ( IsLocalPlayerInTactical() || !m_bShowInNormal )
	{
		ClientModeTFNormal *commander = (ClientModeTFNormal *)g_pClientMode;
		Assert( commander );
		parent = commander->GetCommanderOverlayPanel()->GetVPanel();
	}
	else
	{
		parent = enginevgui->GetPanel( PANEL_CLIENTDLL );
	}
	if ( !GetParent() || ( GetParent()->GetVPanel() != parent ) )
	{
		SetParent( parent );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Compute the size of the panel based upon the commander's zoom level
//-----------------------------------------------------------------------------
unsigned char CEntityPanel::ComputeSizeAndFade( int targW, int targH, bool scale ) {
	if ( !m_pBaseEntity || !ShouldDraw() ) {
		return 0;
	}

	/* original tactical impl. cut for now since we don't use it
	ClientModeTFNormal *commander = ( ClientModeTFNormal * ) g_pClientMode;
	Assert( commander );
	float flZoom = commander->GetCommanderOverlayPanel()->GetZoom();

	// Scale our size
	m_flScale = 0.75 + ( 0.25 * ( 1.0 - flZoom ) ); // 1/2 size at max zoomed out, full size by half zoomed in
	*/

	// Scale the image

	// Get distance to entity
	float flDistance = ( m_pBaseEntity->GetRenderOrigin() - MainViewOrigin() ).Length();
	flDistance *= 2.0f;
	m_flScale = 0.25f + MAX( 0, 2.0f - ( flDistance / 2048.0f ) );
	m_flScale = clamp( m_flScale, 0.0f, 1.0f );

	// Fade based on distance from center of screen along the x coord
	int centerScreenWidth = ScreenWidth() / 2;
	int thisX, thisY;
	ipanel()->GetPos( GetVPanel(), thisX, thisY );
	thisX += m_iOrgOffsetX;
	float diffPos = fabsf( centerScreenWidth - thisX ) / ( centerScreenWidth - 300 );
	float fade = clamp( ( flDistance / 1024.0f ) * diffPos, 0.0f, 1.0f );
	// todo: fade by distance from viewer as well...

	// Update the size
	int w, h;
	if ( scale ) {
		w = ( targW - m_iOrgWidth ) + m_iOrgWidth * m_flScale;
		h = ( targH - m_iOrgHeight ) + m_iOrgHeight * m_flScale;

		// Update the offsets too
		m_OffsetX = m_iOrgOffsetX * m_flScale;
		m_OffsetY = m_iOrgOffsetY * m_flScale;
	} else {
		w = targW;
		h = targH;
	}

	SetSize( w, h );
	
	return ( unsigned char ) roundf( ( 1.0f - fade ) * 255.f );
}

//-----------------------------------------------------------------------------
// initialization
//-----------------------------------------------------------------------------
bool CEntityPanel::Init( ::KeyValues* pInitData, C_BaseEntity* pEntity )
{
	Assert( pInitData && pEntity );
	m_pBaseEntity = pEntity;

	if ( pInitData->GetInt( "showinnormalmode", 0 ) )
	{
		m_bShowInNormal = true;
	}

	// get the size...
	int w, h;
	if (!ParseCoord( pInitData, "offset", m_OffsetX, m_OffsetY ))
		return false;

	if (!ParseCoord( pInitData, "size", w, h ))
		return false;

	const char *mouseover = pInitData->GetString( "mousehint", "" );
	if ( mouseover && mouseover[ 0 ] )
	{
		Q_strncpy( m_szMouseOverText, mouseover, sizeof( m_szMouseOverText ) );
	}

	SetSize( w, h );

	m_iOrgWidth = w;
	m_iOrgHeight = h;
	m_iOrgOffsetX = m_OffsetX;
	m_iOrgOffsetY = m_OffsetY;

	// we need updating
	vgui::ivgui()->AddTickSignal( GetVPanel() );
	return true;
}



//-----------------------------------------------------------------------------
// Purpose: Determine where our entity is in screen space.
//-----------------------------------------------------------------------------
void CEntityPanel::GetEntityPosition( int& sx, int& sy )
{
	if (!m_pBaseEntity)
	{
		sx = sy = -1.0f;
		return;
	}

	GetTargetInScreenSpace( m_pBaseEntity, sx, sy );
}


//-----------------------------------------------------------------------------
// Should we draw?.
//-----------------------------------------------------------------------------
bool CEntityPanel::ShouldDraw()
{
	return ( ( IsLocalPlayerInTactical() && GetClientModeNormal()->ShouldDrawEntity( m_pBaseEntity ) ) || 
			 ( !IsLocalPlayerInTactical() && m_bShowInNormal) );
}


//-----------------------------------------------------------------------------
// called when we're ticked...
//-----------------------------------------------------------------------------
void CEntityPanel::OnTick()
{
	// Determine if panel parent should switch
	ComputeParent();

	// If we should shouldn't draw, don't bother with any of this
	if ( !ShouldDraw() )
		return;

	// Update our current position
	int sx, sy;
	GetEntityPosition( sx, sy );

	// Recalculate our size
	ComputeSizeAndFade( m_iOrgWidth, m_iOrgHeight );

	// Set the position
	SetPos( (int)(sx + m_OffsetX + 0.5f), (int)(sy + m_OffsetY + 0.5f));
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEntityPanel::OnCursorEntered()
{
	if ( m_szMouseOverText[ 0 ] )
		CCommanderStatusPanel::StatusPanel()->SetText(TYPE_HINT, "%s", m_szMouseOverText);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEntityPanel::OnCursorExited()
{
	if ( m_szMouseOverText[ 0 ] )
		CCommanderStatusPanel::StatusPanel()->Clear();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
const char *CEntityPanel::GetMouseOverText( void )
{
	return m_szMouseOverText;
}
