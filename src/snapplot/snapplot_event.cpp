#include "snapconfig.h"
#include "snapplot_event.hpp"

DEFINE_EVENT_TYPE( wxEVT_SET_DETAILVIEW )
DEFINE_EVENT_TYPE( wxEVT_SHOW_DETAILVIEW )
DEFINE_EVENT_TYPE( wxEVT_ZOOM_MAPVIEW )
DEFINE_EVENT_TYPE( wxEVT_LOCATE_MAPVIEW )

wxSnapplotEvent::wxSnapplotEvent( wxEventType commandType, int id )
    :wxEvent( id, commandType )
{
    m_propagationLevel = wxEVENT_PROPAGATE_MAX;
    SetShowTitle();
}

wxSnapplotEvent::wxSnapplotEvent( const wxSnapplotEvent &event ) :
    wxEvent( event ),
    info( event.info )
{
}

void wxSnapplotEvent::SetInfo( const PutTextInfo &info )
{
    this->info = info;
}

void wxSnapplotEvent::SetShowTitle()
{
    info.type = ptfTitleBlock;
}

void wxSnapplotEvent::SetShowStation( int idfrom )
{
    info.type = ptfStation;
    info.from = idfrom;
}

void wxSnapplotEvent::SetShowLine( int idfrom, int idto )
{
    info.type = ptfLine;
    info.from = idfrom;
    info.to = idto;
}

void wxSnapplotEvent::SetShowObs( int idfrom, int idto, int idobs )
{
    info.type = ptfObs;
    info.from = idfrom;
    info.to = idto;
    info.obs_id = idobs;
}

wxEvent* wxSnapplotEvent::Clone() const
{
    return new wxSnapplotEvent( *this );
}
