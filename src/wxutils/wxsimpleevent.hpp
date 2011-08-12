#ifndef WX_SIMPLE_EVENT_HPP
#define WX_SIMPLE_EVENT_HPP

// Declare a simple event .. just has an event id and a propogation level

class wxSimpleEvent : public wxEvent
{
public:
    wxSimpleEvent( wxEventType commandType, bool propogate = false ) :
        wxEvent( wxID_ANY, commandType )
    { m_propagationLevel =  propogate ? wxEVENT_PROPAGATE_MAX : wxEVENT_PROPAGATE_NONE; }
    wxSimpleEvent( const wxSimpleEvent &event ) : wxEvent( event ) {}
    virtual wxEvent* Clone() const { return new wxSimpleEvent( *this );}
};


typedef void (wxEvtHandler::*wxSimpleEventFunction)(wxSimpleEvent&);

#define EVT_SIMPLE_EVENT( eventtype, fn) \
    DECLARE_EVENT_TABLE_ENTRY( eventtype, wxID_ANY, -1, \
    (wxObjectEventFunction) (wxEventFunction) \
    wxStaticCastEvent( wxSimpleEventFunction, & fn ), (wxObject *) NULL ),

#endif