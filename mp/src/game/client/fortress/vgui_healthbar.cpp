#include "cbase.h"
#include "vgui_healthbar.h"
#include "CommanderOverlay.h"
#include <vgui_controls/Controls.h>
#include <vgui/ISurface.h>

CHealthBarPanel::CHealthBarPanel( vgui::Panel *pParent ) : vgui::Panel(pParent, "CHealthBarPanel" )
{
	SetHealth( 0.0f );
	SetPaintBackgroundEnabled( false );
	SetVertical( false );
}

CHealthBarPanel::~CHealthBarPanel( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Parse values from the file
//-----------------------------------------------------------------------------
bool CHealthBarPanel::Init( KeyValues* pInitData )
{
	if (!pInitData)
		return false;

	if (!ParseRGBA(pInitData, "okcolor", m_Ok ))
		return false;

	if (!ParseRGBA(pInitData, "badcolor", m_Bad ))
		return false;

	int x, y, w, h;
	if (!ParseRect(pInitData, "position", x, y, w, h ))
		return false;
	SetPos( x, y );
	SetSize( w, h );
	SetCursor( NULL );

	return true;
}

void CHealthBarPanel::SetGoodColor( int r, int g, int b, int a )
{
	m_Ok.SetColor( r,g,b,a );
}

void CHealthBarPanel::SetBadColor( int r, int g, int b, int a )
{
	m_Bad.SetColor( r,g,b,a );
}

void CHealthBarPanel::SetHealth( float health )
{
	m_Health = health;
}

void CHealthBarPanel::SetVertical( bool bVertical )
{
	m_bVertical = bVertical;
}

void CHealthBarPanel::Paint( void )
{
	int w, h;
	GetSize( w, h );
	
	float frac;
	frac = min( 1.0, m_Health );
	frac = max( 0.0, m_Health );

	vgui::surface()->DrawSetColor( 255,255,255,255 );
	vgui::surface()->DrawOutlinedRect( 0,0, w,h );

	int drawwidth = frac * w;
	int drawheight = frac * h;

	// Draw the Ok section
	vgui::surface()->DrawSetColor( m_Ok[0], m_Ok[1], m_Ok[2], m_Ok[3] );
	if ( m_bVertical )
	{
		vgui::surface()->DrawFilledRect( 0, h - drawheight, w, h );
	}
	else
	{
		vgui::surface()->DrawFilledRect( 0, 0, drawwidth, h );
	}

	vgui::surface()->DrawSetColor( m_Bad[0], m_Bad[1], m_Bad[2], m_Bad[3] );
	// Draw the Bad section
	if ( m_bVertical )
	{
		if (h != drawheight)
		{
			vgui::surface()->DrawFilledRect( 0, 0, w, h - drawheight );
		}
	}
	else
	{
		if (w != drawwidth)
		{
			vgui::surface()->DrawFilledRect( drawwidth, 0, w, h );
		}
	}
}