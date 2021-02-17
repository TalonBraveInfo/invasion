#include "cbase.h"

#include "vgui_teammenu.h"

#include <vgui/IVGui.h>

#include <vgui_controls/RichText.h>

#include "c_basetfplayer.h"
#include "c_team.h"

using namespace vgui;

CFortressTeamMenu::CFortressTeamMenu(IViewPort *pViewPort) : CTeamMenu(pViewPort)
{
	iLastPreview = 0;	// Initially preview human team information.
	
	bAlienButton	= new Button(this,"AlienButton","Aliens");	// Alien team button.
	bHumanButton	= new Button(this,"HumanButton","Humans");	// Human team button.
	bAutoButton		= new Button(this,"AutoButton","Auto");		// Automatic team selection button.

	bCancelButton = new Button(this, "CancelButton", "#VGui_Cancel");	// Cancel, duh.
	bCancelButton->SetCommand("Cancel");

	vTeamDescription	= new RichText(this,"TeamInfo");	// Team description.

	mTeamPreviewPanel	= new CModelPanel(this,"TeamPreview");
	mTeamLogoPreview	= new CModelPanel(this,"TeamLogoPreview");

	ivgui()->AddTickSignal(GetVPanel());

	LoadControlSettings("resource/ui/teammenu.res");
}

CFortressTeamMenu::~CFortressTeamMenu()
{
}

void CFortressTeamMenu::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	Update();
}

void CFortressTeamMenu::Update(void)
{
	C_BaseTFPlayer *pPlayer = C_BaseTFPlayer::GetLocalPlayer();
	if(pPlayer && (pPlayer->GetTeamNumber() != TEAM_UNASSIGNED))
		bCancelButton->SetVisible(true);
	else
		bCancelButton->SetVisible(false);

	BaseClass::Update();
}

void CFortressTeamMenu::OnTick(void)
{
	if(bHumanButton->IsCursorOver() && (iLastPreview != 0))
	{
		mTeamLogoPreview->SwapModel("models/interface/red_team.mdl");
		mTeamPreviewPanel->SwapModel("models/player/human_commando.mdl");

		vTeamDescription->SetText("#FORTRESS_TEAMHUMANINFO");

		iLastPreview = 0;
	}
	else if(bAlienButton->IsCursorOver() && (iLastPreview != 1))
	{
		mTeamLogoPreview->SwapModel("models/interface/blue_team.mdl");
		mTeamPreviewPanel->SwapModel("models/player/alien_commando.mdl");

		vTeamDescription->SetText("#FORTRESS_TEAMALIENINFO");

		iLastPreview = 1;
	}
	else if(bAutoButton->IsCursorOver() && (iLastPreview != 2))
	{
		mTeamLogoPreview->SwapModel("models/interface/random.mdl");
		mTeamPreviewPanel->DeleteModelData();

		vTeamDescription->SetText("#FORTRESS_TEAMAUTOINFO");

		iLastPreview = 2;
	}
}

void CFortressTeamMenu::OnCommand(const char *command)
{
	if ( V_strstr( command, "changeteam " ) ) {
		engine->ClientCmd( command );
	}

	BaseClass::OnCommand(command);

	ShowPanel(false);
	OnClose();
}