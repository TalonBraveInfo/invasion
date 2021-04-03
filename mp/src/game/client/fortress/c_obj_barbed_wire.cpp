//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "c_obj_barbed_wire.h"


IMPLEMENT_CLIENTCLASS_DT( C_ObjectBarbedWire, DT_ObjectBarbedWire, CObjectBarbedWire )
	RecvPropEHandle( RECVINFO( m_hConnectedTo ) )
END_RECV_TABLE()


ConVar obj_barbed_wire_hang_dist( "obj_barbed_wire_hang_dist", "20" );


C_ObjectBarbedWire::C_ObjectBarbedWire()
{
	m_vLastConnectedOrigin.Init( -999999999, -999999999, -999999999 );
	m_vLastOrigin = m_vLastConnectedOrigin;
}


C_ObjectBarbedWire::~C_ObjectBarbedWire()
{
	// Get rid of our rope if necessary.
	if ( m_hRope )
	{
		m_hRope->Release();
	}
}


void C_ObjectBarbedWire::OnDataChanged( DataUpdateType_t type )
{
	if ( m_hConnectedTo != m_hLastConnectedTo )
	{
		m_hLastConnectedTo = m_hConnectedTo;

		// Get rid of any old rope we had.
		int iAttachment = LookupAttachment( "wire_attachment" );
		
		// Create or delete our rope?
		if ( m_hConnectedTo )
		{
			if ( !m_hRope )
			{
				const char *material;
				if( GetTeamNumber() == TEAM_HUMANS ) {
					material = "sprites/barbed_wire";
				} else {
					material = "sprites/physbeam";
				}
				m_hRope = C_RopeKeyframe::Create(
					this,
					m_hConnectedTo,
					iAttachment,
					iAttachment,
					8,
					material
					);
			}
		}
		else
		{
			if ( m_hRope )
			{
				m_hRope->Release();
				m_hRope = NULL;
			}
		}

		// Update rope parameters.
		if ( m_hRope )
		{
			m_hRope->SetEndEntity( m_hConnectedTo );
			// Originally used the BARBED flag here, but that just looks like a fucking mess.
			m_hRope->SetRopeFlags( ROPE_SIMULATE );
		}
	}

	BaseClass::OnDataChanged( type );
}


void C_ObjectBarbedWire::Spawn()
{
}


void C_ObjectBarbedWire::ClientThink()
{
	if ( m_hConnectedTo && m_hRope )
	{
		if ( m_vLastOrigin != GetAbsOrigin() || m_vLastConnectedOrigin != m_hConnectedTo->GetAbsOrigin() )
		{
			m_hRope->SetupHangDistance( obj_barbed_wire_hang_dist.GetFloat() );
			
			m_vLastOrigin = GetAbsOrigin();
			m_vLastConnectedOrigin = m_hConnectedTo->GetAbsOrigin();
		}
	}
	
	SetNextClientThink( gpGlobals->curtime + 0.1f );
}




