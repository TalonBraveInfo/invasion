/*
"Invasion" project by TalonBrave.info, Copyright (C) 2014-2022 (see README.md for list of contributors)
*/

#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Panel.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/Controls.h"
#include "clientmode.h"
#include "filesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CHudVersion : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudVersion, vgui::Panel );

public:
	CHudVersion( const char *elementName );
	~CHudVersion();

private:
	void Init() override;
	void Paint() override;
	void OnThink() override;
	void ApplySchemeSettings( vgui::IScheme *scheme ) override;

	char        versionLabel[ 128 ];
	vgui::HFont labelFont;

	static const char *GetVersionPath() { return "version.txt"; }
};

static ConVar showVersion( "show_version", "1" );

DECLARE_HUDELEMENT( CHudVersion );

CHudVersion::CHudVersion( const char *elementName ) : CHudElement( elementName ), vgui::Panel( nullptr, "HudVersion" )
{
	Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	SetActive( true );
	SetPaintBorderEnabled( true );

	SetHiddenBits( HIDEHUD_ALL );

	versionLabel[ 0 ] = '\0';
}

CHudVersion::~CHudVersion()
{
}

void CHudVersion::Init()
{
	CHudElement::Init();

	CUtlBuffer buffer;
	if ( !filesystem->ReadFile( GetVersionPath(), "GAME", buffer ) )
	{
		Msg( "Failed to fetch \"%s\", will not display version.\n", GetVersionPath() );
		return;
	}

	const char *str = ( char * ) buffer.Base();
	assert( *str != '\0' );
	if ( *str == '\0' )
		Error( "Invalid version provided under \"%s\"!\n", GetVersionPath() );

	char        *end;
	unsigned int versionMajor = strtoul( str, &end, 10 );
	unsigned int versionMinor = strtoul( end, &end, 10 );
	unsigned int versionPatch = strtoul( end, NULL, 10 );

	V_snprintf( versionLabel, sizeof( versionLabel ), "v%u.%u.%u (Pre-Alpha)", versionMajor, versionMinor, versionPatch );

	SetVisible( true );
}

void CHudVersion::Paint()
{
	if ( *versionLabel == '\0' )
		return;

	vgui::HScheme scheme = vgui::scheme()->GetScheme( "ClientScheme" );
	vgui::HFont   font   = vgui::scheme()->GetIScheme( scheme )->GetFont( "CommentaryDefault" );
	if ( !font )
		font = vgui::scheme()->GetIScheme( scheme )->GetFont( "Default" );

	vgui::surface()->DrawSetTextFont( font );
	vgui::surface()->DrawSetTextColor( Color( 100, 255, 100, GetAlpha() ) );

	int pixels = UTIL_ComputeStringWidth( font, versionLabel );
	vgui::surface()->DrawSetTextPos( ( GetWide() - pixels ) / 2, 22 );

	char *p = versionLabel;
	while ( *p )
	{
		vgui::surface()->DrawUnicodeChar( *p );
		p++;
	}
}

void CHudVersion::OnThink()
{
	SetVisible( showVersion.GetBool() );
}

void CHudVersion::ApplySchemeSettings( vgui::IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );

	SetPaintBackgroundEnabled( true );
	SetPaintBackgroundType( 2 );

	SetBgColor( Color( 0, 0, 0, 255 ) );
}
