/*
Copyright (C) Valve Corporation
Copyright (C) 2014-2016 TalonBrave.info
*/

#include <vgui/Cursor.h>
#include "clientmode_tfbase.h"
#include <vgui_controls/EditablePanel.h>
#include "hud_minimap.h"

class CCommanderOverlayPanel;
namespace vgui
{
	class AnimationController;
}

class ClientModeTFNormal : public ClientModeTFBase, public IMinimapClient
{
DECLARE_CLASS( ClientModeTFNormal, ClientModeTFBase );

// IClientMode overrides.
public:
	ClientModeTFNormal();

	class Viewport : public CBaseViewport
	{
		typedef CBaseViewport BaseClass;
		// Panel overrides.
	public:
		Viewport();
		virtual ~Viewport() override;

		virtual void CreateDefaultPanels() override;

		void SetTeamScheme( int teamId = TEAM_UNASSIGNED );

		virtual void ApplySchemeSettings(vgui::IScheme *pScheme) override
		{
			BaseClass::ApplySchemeSettings(pScheme);

			gHUD.InitColors(pScheme);

			SetPaintBackgroundEnabled(false);
		}

		IViewPortPanel *CreatePanelByName(const char *szPanelName);

		// hogsy start
		void Enable() {}
		void Disable() {}

		void EnableCommanderMode();
		void DisableCommanderMode();

		void MinimapClicked(const Vector& clickWorldPos);

		CCommanderOverlayPanel *GetCommanderOverlayPanel() { return m_pOverlayPanel; }
		// hogsy end

	protected:
		virtual void Paint() override;

	private:
		// hogsy start
		CCommanderOverlayPanel	*m_pOverlayPanel;

		vgui::HCursor m_CursorCommander;
		vgui::HCursor m_CursorRightMouseMove;
		// hogsy end
	};

	virtual void Update() override;
	virtual bool CreateMove( float flInputSampleTime, CUserCmd *cmd ) override;

	virtual bool ShouldDrawViewModel( void ) override;
// hogsy start
	virtual bool ShouldDrawEntity(C_BaseEntity *pEnt) override;
	virtual bool ShouldDrawDetailObjects() override;
	virtual bool ShouldDrawLocalPlayer( C_BasePlayer *pPlayer ) override;
	virtual bool ShouldDrawCrosshair() override;
	virtual bool ShouldDrawParticles() override;
	virtual bool ShouldDrawFog() override;

	virtual void Init() override;

	virtual void Enable() override;
	virtual void Disable() override;

	virtual void OverrideView( CViewSetup *pSetup ) override;

	virtual void LevelInit( const char *newmap ) override;
	virtual void LevelShutdown() override;

	virtual void Layout() override;

	bool IsCommanderMode() { return commander_mode; }
	void EnableCommanderMode();
	void DisableCommanderMode();

	CCommanderOverlayPanel *GetCommanderOverlayPanel();
// hogsy end
	
	virtual vgui::Panel *GetMinimapParent();

// hogsy start
	virtual int	KeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding ) override;

	virtual void MinimapClicked( const Vector& clickWorldPos ) override;

	// Makes the mouse sit over a particular world location
	void MoveMouse(Vector& worldPos);

	virtual void PreRender( CViewSetup *pSetup ) override;
	virtual void PostRender() override;

	Viewport *GetNormalViewport();

private:
	bool commander_mode;

	// Fills in ortho parameters (and near/far Z) in pSetup for how the commander mode renders the world.
	bool GetOrthoParameters(CViewSetup *pSetup);

	float GetScaledSlueSpeed() { return m_ScaledSlueSpeed; }	// Scale commander slue speed based on viewport zoom factor
	float GetHeightForMap(float zoom);

	float Commander_ResampleMove(float in);

	void ResetCommand(CUserCmd *cmd);
	void IsometricMove(CUserCmd *cmd);

	int	m_LastMouseX;
	int	m_LastMouseY;

	float m_ScaledMouseSpeed;
	float m_ScaledSlueSpeed;
	float m_Log_BaseEto2;		// scales logarithms from base E to base 2.	
// hogsy end
};

extern IClientMode *GetClientModeNormal();