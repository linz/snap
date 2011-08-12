#include "wx_includes.hpp"
#include "wxmap.hpp"

class TestMap : public wxMap
{
public:
	TestMap();
	virtual ~TestMap();
	virtual void GetMapExtents( MapRect &extents );
	virtual void GetLayer( Symbology &symbology );
	void DrawMap( wxMapDrawer &drawer );
	virtual void *StartDrawing();
	virtual bool DrawSome( wxMapDrawer &drawer, void *state );
	virtual void EndDrawing( void *state );
private:
	bool started;
	int layerId;
};
