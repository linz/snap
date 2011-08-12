#include "snapconfig.h"
#include "wxmap.hpp"

// Class wxMap ... Abstract Base Class for a map.

wxMap::wxMap()
{
}

wxMap::~wxMap()
{
}

void wxMap::DrawMap( wxMapDrawer &drawer )
{
    void *state = StartDrawing();
    while( DrawSome( drawer, state ) ) {}
    EndDrawing(state);
}
