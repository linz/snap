#ifndef SNAPPLOT_DETAILSVIEW_HPP
#define SNAPPLOT_DETAILSVIEW_HPP

#include "wx_includes.hpp"

#include "snapplot_event.hpp"

// Class to convert PutTextInfo address to and from string,

class PutTextInfoWriter : public PutTextInfo
{
public:
    PutTextInfoWriter( char type=ptfNone, int from=0, int to=0, int obs_id=0 );
    PutTextInfoWriter( const PutTextInfo &info );
    PutTextInfoWriter( const wxString &string );
    wxString ToString();
};


class SnapplotDetailsView : public wxHtmlWindow
{
public:
    SnapplotDetailsView( wxWindow *parent );
    ~SnapplotDetailsView();
    void Show( const PutTextInfo &info );
    void RefreshText();

private:

    void ShowTitleInfo();
    void ShowStationInfo( int stn );
    void ShowLineInfo( int from, int to);
    void ShowObsInfo( int from, int to, int id );

    void AddToHistory( const PutTextInfo &jump );
    bool RevertHistory( );
    void GetCurrentView( PutTextInfo &jump );
    void JumpTo( const PutTextInfo &writer, bool zoomMap );

    // Routines to build content

    void ClearText();
    void AddString( const char *string, bool newLine = true );
    void AddNewLine();
    void DisplayText();
    void AddInfoText( PutTextInfo *jump, const char *text, bool addNewLine = true );
    static void AddInfoText( void *win, PutTextInfo *jump, const char *text );

    // Event handlers

    void OnHtmlLinkClicked( wxHtmlLinkEvent &event );
    void OnRightMouse( wxMouseEvent &event );

    char buf[256];
    PutTextInfoWriter *history;
    int historyNext;
    int historyCount;

    wxString newText;

    DECLARE_EVENT_TABLE();
};

#endif
