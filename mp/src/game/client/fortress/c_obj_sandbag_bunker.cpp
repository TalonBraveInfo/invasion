#include "cbase.h"
#include "hud.h"
#include "CommanderOverlay.h"
#include "c_baseobject.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class C_ObjectSandbagBunker : public C_BaseObject
{
	DECLARE_CLASS( C_ObjectSandbagBunker, C_BaseObject );
public:
	DECLARE_CLIENTCLASS();

	C_ObjectSandbagBunker();

private:
	C_ObjectSandbagBunker( const C_ObjectSandbagBunker & ); // not defined, not accessible
};

IMPLEMENT_CLIENTCLASS_DT(C_ObjectSandbagBunker, DT_ObjectSandbagBunker, CObjectSandbagBunker)
END_RECV_TABLE()

C_ObjectSandbagBunker::C_ObjectSandbagBunker()
{
}
