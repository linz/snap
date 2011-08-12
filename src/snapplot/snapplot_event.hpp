#ifndef SNAPPLOT_EVENT_HPP
#define SNAPPLOT_EVENT_HPP

#include "wx_includes.hpp"

#include "snapplot_event.hpp"
#include "snapplot_eventids.hpp"

//extern "C" {
#include "infowin.h"
//}


// Declares an custom event type for SNAPPLOT events, which require defining
// the PutTextInfo structure to define what is to be displayed ...

class wxSnapplotEvent: public wxEvent
{
public:
    wxSnapplotEvent( wxEventType commandType = wxEVT_NULL, int id = 0 );
    wxSnapplotEvent( const wxSnapplotEvent &event );

    void SetInfo( const PutTextInfo &info );
    void SetShowTitle();
    void SetShowStation( int idfrom );
    void SetShowLine( int idfrom, int idto );
    void SetShowObs( int idrom, int idto, int idobs );

    // accessors
    const PutTextInfo &GetInfo() const { return info; }

    // required for sending with wxPostEvent()
    virtual wxEvent* Clone() const;

private:
    PutTextInfo info;

    DECLARE_NO_ASSIGN_CLASS( wxSnapplotEvent )
};

DECLARE_EVENT_TYPE( wxEVT_SET_DETAILVIEW, -1 )
DECLARE_EVENT_TYPE( wxEVT_SHOW_DETAILVIEW, -1 )
DECLARE_EVENT_TYPE( wxEVT_ZOOM_MAPVIEW, -1 )
DECLARE_EVENT_TYPE( wxEVT_LOCATE_MAPVIEW, -1 )

typedef void (wxEvtHandler::*wxSnapplotEventFunction)(wxSnapplotEvent&);

#define EVT_SNAPPLOT_EVENT( eventtype, fn) \
    DECLARE_EVENT_TABLE_ENTRY( eventtype, wxID_ANY, -1, \
    (wxObjectEventFunction) (wxEventFunction) \
    wxStaticCastEvent( wxSnapplotEventFunction, & fn ), (wxObject *) NULL ),


#endif
