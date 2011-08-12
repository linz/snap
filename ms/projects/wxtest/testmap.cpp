#include "testmap.hpp"

TestMap::TestMap(){ started = false; }
TestMap::~TestMap(){}
void TestMap::GetMapExtents( MapRect &extents )
{
	extents = MapRect( -10, 10, 10, 30 );
}

void TestMap::GetLayer( Symbology &symbology )
{
	layerId = symbology.AddLayer( _T("l0"), LayerSymbology::hasColour, wxColour(_T("BLUE")), true );
}

void TestMap::DrawMap( wxMapDrawer &drawer )
{
}

void *TestMap::StartDrawing()
{
	started = true;
	return 0;
}

bool TestMap::DrawSome( wxMapDrawer &drawer, void *state )
{
	if( started )
	{
		drawer.SetLayer( layerId );
		drawer.MoveTo( MapPoint(-10, 11) );
		drawer.LineTo( MapPoint(-8, 30) );
		drawer.LineTo( MapPoint(10, 10 ) );
		drawer.LineTo( MapPoint(10,33) );
		drawer.LineTo( MapPoint(-10,11) );
		drawer.EndLine();
	}
	started = false;
	return false;
}

void TestMap::EndDrawing( void *state )
{
}
