#include "cbase.h"

#include "vgui_classmenu.h"

#include <vgui/IVGui.h>

#include <vgui_controls/RichText.h>

#include "c_basetfplayer.h"
#include "c_team.h"

using namespace vgui;

CFortressClassMenu::CFortressClassMenu(IViewPort *pViewPort) : Frame(NULL,PANEL_CLASS)
{
	iLastPreview = 0;

	m_pViewPort = pViewPort;

	SetScheme("ClientScheme");
	SetSizeable(false);
	SetProportional(true);
	SetMoveable(false);
	SetTitleBarVisible(true);
	SetCloseButtonVisible(false);
	
	bReconButton = new Button(this, "ReconButton", "Recon");
	bCommandoButton = new Button(this, "CommandoButton", "Commando");
	bMedicButton = new Button(this, "MedicButton", "Medic");
	bDefenderButton = new Button(this, "DefenderButton", "Defender");
	bSniperButton = new Button(this, "SniperButton", "Sniper");
	bSupportButton = new Button(this, "SupportButton", "Support");
	bEscortButton = new Button(this, "EscortButton", "Escort");
	bSapperButton = new Button(this, "SapperButton", "Sapper");
	bInfiltratorButton = new Button(this, "InfiltratorButton", "Infiltrator");
	bPyroButton = new Button(this, "PyroButton", "Pyro");
	bRandomButton = new Button(this, "RandomButton", "Random");

	bCancelButton = new Button(this, "CancelButton", "#VGui_Cancel");
	bCancelButton->SetCommand("Cancel");

	rtClassDescription = new RichText(this,"ClassInfo");

	// Don't show model preview until we hover over something...
	mClassPreview = new CModelPanel(this,"ClassPreview");
	mClassPreview->DeleteModelData();

	ivgui()->AddTickSignal(GetVPanel());

	LoadControlSettings("resource/ui/classmenu.res");

	InvalidateLayout();
}

CFortressClassMenu::~CFortressClassMenu()
{
}

void CFortressClassMenu::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	Update();
}

void CFortressClassMenu::ShowPanel(bool bShow)
{
	if(BaseClass::IsVisible() == bShow)
		return;

	if(bShow)
	{
		Activate();
		SetMouseInputEnabled(true);
	}
	else
	{
		SetVisible(false);
		SetMouseInputEnabled(false);
	}
}

void CFortressClassMenu::Update(void)
{
	C_BaseTFPlayer *pPlayer = C_BaseTFPlayer::GetLocalPlayer();
	if(pPlayer && (pPlayer->GetClass() != TFCLASS_UNDECIDED))
		bCancelButton->SetVisible(true);
	else
		bCancelButton->SetVisible(false);
}

void CFortressClassMenu::Activate(void)
{
	BaseClass::Activate();

	// Ensure that there's no model when initially opened.
	mClassPreview->DeleteModelData();
}

void CFortressClassMenu::OnTick(void)
{
	C_BaseTFPlayer *pPlayer = C_BaseTFPlayer::GetLocalPlayer();
	if(!pPlayer)
		return;

	if ( bReconButton->IsCursorOver() ) {
		if ( iLastPreview != 0 ) {
			if ( pPlayer->GetTeamNumber() == TEAM_HUMANS )
				mClassPreview->SwapModel( "models/player/human_recon.mdl" );
			else
				mClassPreview->SwapModel( "models/player/alien_recon.mdl" );

			rtClassDescription->SetText( "#FORTRESS_RECONDESCRIPTION" );
		}

		iLastPreview = 0;
	} else if ( bCommandoButton->IsCursorOver() ) {
		if ( iLastPreview != 1 ) {
			if ( pPlayer->GetTeamNumber() == TEAM_HUMANS )
				mClassPreview->SwapModel( "models/player/human_commando.mdl" );
			else
				mClassPreview->SwapModel( "models/player/alien_commando.mdl" );

			rtClassDescription->SetText( "#FORTRESS_COMMANDODESCRIPTION" );
		}

		iLastPreview = 1;
	} else if ( bMedicButton->IsCursorOver() ) {
		if ( iLastPreview != 2 ) {
			if ( pPlayer->GetTeamNumber() == TEAM_HUMANS )
				mClassPreview->SwapModel( "models/player/human_medic.mdl" );
			else
				mClassPreview->SwapModel( "models/player/alien_medic.mdl" );

			rtClassDescription->SetText( "#FORTRESS_MEDICDESCRIPTION" );
		}

		iLastPreview = 2;
	} else if ( bDefenderButton->IsCursorOver() ) {
		if ( iLastPreview != 3 ) {
			if ( pPlayer->GetTeamNumber() == TEAM_HUMANS )
				mClassPreview->SwapModel( "models/player/human_defender.mdl" );
			else
				mClassPreview->SwapModel( "models/player/alien_defender.mdl" );

			rtClassDescription->SetText( "#FORTRESS_DEFENDERDESCRIPTION" );
		}

		iLastPreview = 3;
	} else if ( bSniperButton->IsCursorOver() ) {
		if ( iLastPreview != 4 ) {
			if ( pPlayer->GetTeamNumber() == TEAM_HUMANS )
				mClassPreview->SwapModel( "models/player/human_sniper.mdl" );
			else
				mClassPreview->SwapModel( "models/player/alien_sniper.mdl" );

			rtClassDescription->SetText( "#FORTRESS_SNIPERDESCRIPTION" );
		}

		iLastPreview = 4;
	} else if ( bSupportButton->IsCursorOver() ) {
		if ( iLastPreview != 5 ) {
			if ( pPlayer->GetTeamNumber() == TEAM_HUMANS )
				mClassPreview->SwapModel( "models/player/human_support.mdl" );
			else
				mClassPreview->SwapModel( "models/player/alien_support.mdl" );

			rtClassDescription->SetText( "#FORTRESS_SUPPORTDESCRIPTION" );
		}

		iLastPreview = 5;
	} else if ( bEscortButton->IsCursorOver() ) {
		if ( iLastPreview != 6 ) {
			if ( pPlayer->GetTeamNumber() == TEAM_HUMANS )
				mClassPreview->SwapModel( "models/player/human_escort.mdl" );
			else
				mClassPreview->SwapModel( "models/player/alien_escort.mdl" );

			rtClassDescription->SetText( "#FORTRESS_ESCORTDESCRIPTION" );
		}

		iLastPreview = 6;
	} else if ( bSapperButton->IsCursorOver() ) {
		if ( iLastPreview != 7 ) {
			if ( pPlayer->GetTeamNumber() == TEAM_HUMANS )
				mClassPreview->SwapModel( "models/player/human_sapper.mdl" );
			else
				mClassPreview->SwapModel( "models/player/alien_sapper.mdl" );

			rtClassDescription->SetText( "#FORTRESS_SAPPERDESCRIPTION" );
		}

		iLastPreview = 7;
	} else if ( bInfiltratorButton->IsCursorOver() ) {
		if ( iLastPreview != 8 ) {
			if ( pPlayer->GetTeamNumber() == TEAM_HUMANS )
				mClassPreview->SwapModel( "models/player/human_infiltrator.mdl" );
			else
				mClassPreview->SwapModel( "models/player/alien_infiltrator.mdl" );

			rtClassDescription->SetText( "#FORTRESS_INFILTRATORDESCRIPTION" );
		}

		iLastPreview = 8;
	} else if ( bPyroButton->IsCursorOver() ) {
		if ( iLastPreview != 10 ) {
			if ( pPlayer->GetTeamNumber() == TEAM_HUMANS )
				mClassPreview->SwapModel( "models/player/human_pyro.mdl" );
			else
				mClassPreview->SwapModel( "models/player/alien_pyro.mdl" );

			rtClassDescription->SetText( "#FORTRESS_CLASSPYROINFO" );
		}

		iLastPreview = 10;
	} else if ( bRandomButton->IsCursorOver() ) {
		if ( iLastPreview != 11 ) {
			mClassPreview->DeleteModelData();
			rtClassDescription->SetText( "#FORTRESS_CLASSRANDOMINFO" );
		}

		iLastPreview = 11;
	}
}

void CFortressClassMenu::OnCommand(const char *command)
{
	if ( V_strstr( command, "changeclass " ) ) {
		// TODO: Check if this is valid before sending... ~hogsy
		engine->ClientCmd( command );
	}

	BaseClass::OnCommand(command);

	ShowPanel(false);
	OnClose();
}