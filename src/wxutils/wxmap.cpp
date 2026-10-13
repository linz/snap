#include "snapconfig.h"
#include "wxmap.hpp"

// Class wxMap ... Abstract Base Class for a map.

wxMap::wxMap() = default;

wxMap::~wxMap() = default;

void wxMap::DrawMap(wxMapDrawer& drawer) {
    void* state = StartDrawing();
    while (DrawSome(drawer, state)) {
    }
    EndDrawing(state);
}
