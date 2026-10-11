#include "snapconfig.h"
// Module supports a window that can be used to display information about the
// stations, connections, or individual observations...

#include <string>

#include "snapplot_detailsview.hpp"

//extern "C" {
#include "snap/snapglob.h"
#include "plotstns.h"
#include "plotconn.h"
#include "plotscal.h"
#include "snap/stnadj.h"
#include "util/dms.h"
#include "util/pi.h"
#include "util/textformat.hpp"
//};

const int HistorySize = 20;

//////////////////////////////////////////////////////////
// Class to convert PutTextInfo address to and from string

PutTextInfoWriter::PutTextInfoWriter( char type, int from, int to, int obs_id )
{
    this->type = type;
    this->from = from;
    this->to = to;
    this->obs_id = obs_id;
}

PutTextInfoWriter::PutTextInfoWriter( const PutTextInfo &info ) :
    PutTextInfo( info )
{
}

PutTextInfoWriter::PutTextInfoWriter( const wxString &string )
{
    int itype, ifrom, ito, iobs_id;
    type = ptfNone;
    if( sscanf( string.mb_str(),"PTI:%d:%d:%d:%d",&itype,&ifrom,&ito,&iobs_id ) != EOF )
    {
        type = itype;
        from = ifrom;
        to = ito;
        obs_id = iobs_id;
    }
}

wxString PutTextInfoWriter::ToString()
{
    wxString s;
    s.Printf("PTI:%d:%d:%d:%d",((int) type),((int) from),((int) to),((int) obs_id));
    return s;
}

BEGIN_EVENT_TABLE(SnapplotDetailsView, wxHtmlWindow)
    EVT_HTML_LINK_CLICKED(wxID_ANY, SnapplotDetailsView::OnHtmlLinkClicked )
    EVT_RIGHT_DOWN(SnapplotDetailsView::OnRightMouse )
END_EVENT_TABLE()

SnapplotDetailsView::SnapplotDetailsView( wxWindow *parent ) :
    wxHtmlWindow(parent, -1, wxDefaultPosition, wxDefaultSize, wxHW_NO_SELECTION)
{
    newText.Alloc(2048);
    history = new PutTextInfoWriter[HistorySize];
    historyCount = 0;
    historyNext = 0;
}

SnapplotDetailsView::~SnapplotDetailsView()
{
    delete [] history;
}

void SnapplotDetailsView::ClearText()
{
    newText.Empty();
    newText.Append("<html><body><pre>");
}

void SnapplotDetailsView::AddString( const std::string &text, const bool newLine )
{
    newText.Append(text );
    if( newLine ) AddNewLine();
}

void SnapplotDetailsView::AddInfoText( PutTextInfo *jump, const std::string &text, const bool addNewLine )
{
    wxString s(text);
    s.Replace("&","&amp;");
    s.Replace("<","&lt;");
    s.Replace(">","&gt;");
    if( jump && jump->type != ptfNone )
    {
        PutTextInfoWriter pti(*jump);
        newText.Append("<a href=\"");
        newText.Append(pti.ToString());
        newText.Append("\">");
        newText.Append(s);
        newText.Append("</a>");
    }
    else
    {
        newText.Append(s);
    }
    if( addNewLine ) AddNewLine();
}

void SnapplotDetailsView::AddNewLine()
{
    newText.Append("\n");
}


void SnapplotDetailsView::DisplayText()
{
    newText.Append("</pre></body></html>");
    SetPage( newText );
    newText.Empty();
}


void SnapplotDetailsView::AddInfoText( void *win, PutTextInfo *jump, const std::string &text )
{
    SnapplotDetailsView *dv = static_cast<SnapplotDetailsView *>(win);
    dv->AddInfoText( jump, text, true );
}


void SnapplotDetailsView::OnHtmlLinkClicked( wxHtmlLinkEvent &event )
{
    PutTextInfoWriter pti( event.GetLinkInfo().GetHref() );
    if( pti.type != ptfNone )
    {
        const wxMouseEvent *me = event.GetLinkInfo().GetEvent();
        if( me->LeftUp())
        {
            Show( pti );

            bool zoom = me && me->ShiftDown();
            JumpTo( pti, zoom );
        }
    }
}

void SnapplotDetailsView::JumpTo( const PutTextInfo &jump, bool zoomMap )
{

    // Zoom to the selected observation if shift was pressed

    if( zoomMap )
    {
        wxSnapplotEvent e2( wxEVT_ZOOM_MAPVIEW, GetId() );
        e2.SetEventObject( this );
        e2.SetInfo( jump );
        AddPendingEvent( e2 );
    }

    // Highlight on the map

    wxSnapplotEvent e3( wxEVT_LOCATE_MAPVIEW, GetId() );
    e3.SetEventObject( this );
    e3.SetInfo( jump );
    AddPendingEvent( e3 );
}

void SnapplotDetailsView::OnRightMouse( wxMouseEvent &event )
{
    PutTextInfo jump;
    if( RevertHistory() )
    {
        GetCurrentView( jump );
        JumpTo( jump, event.ShiftDown() );
    }
}

void SnapplotDetailsView::Show( const PutTextInfo &info )
{
    AddToHistory( info );
}

void SnapplotDetailsView::AddToHistory( const PutTextInfo &jump )
{
    history[historyNext] = jump;
    historyNext++;
    if( historyNext >= HistorySize ) historyNext = 0;
    if( historyCount < HistorySize ) historyCount++;
    RefreshText();
}

void SnapplotDetailsView::GetCurrentView( PutTextInfo &jump )
{
    if( historyCount > 0 )
    {
        int current = historyNext-1;
        if( current < 0 ) current += HistorySize;
        jump = history[current];
    }
    else
    {
        jump.type = ptfTitleBlock;
    }
}

bool SnapplotDetailsView::RevertHistory(  )
{
    if( historyCount < 1 ) return false;
    historyNext -= 1;
    historyCount -= 1;
    if( historyNext < 0 ) historyNext += HistorySize;
    RefreshText();
    return true;
}

void SnapplotDetailsView::RefreshText()
{
    PutTextInfo info;
    GetCurrentView( info );
    switch( info.type )
    {
    case ptfTitleBlock:
        ShowTitleInfo();
        break;

    case ptfStation:
        ShowStationInfo( info.from );
        break;

    case ptfLine:
        ShowLineInfo( info.from, info.to );
        break;

    case ptfObs:
        ShowObsInfo( info.from, info.to, info.obs_id );
        break;
    }
    Refresh();
}


///////////////////////////////////////////////////////////////////////

void SnapplotDetailsView::ShowTitleInfo()
{
    int opt;
    double threshold;

    /*
    SaveInfoLocation();
    JumpDef jmp;
    jmp.ToTitle();
    currentItem = jmp;
    ccMessage::PostMessage( ccMessage( msgShowInfoStns, (int) 0, (int) 0 ) );

    mode = DefaultMode;
    */

    ClearText();
    AddString( job_title );
    AddNewLine();
    AddString( "Coordinate system: ",false);
    AddString( plot_crdsys_name() );

    if( got_covariances() )
    {
        AddString( std::string( "Displaying " ) + ( aposteriori_errors ? "aposteriori" : "apriori" ) + " errors" );
        if( use_confidence_limit )
        {
            AddString( "Error ellipses and height errors are " + format_fixed( confidence_limit, 2 ) + "% confidence limits" );
        }
        else if( confidence_limit != 1.0 )
        {
            AddString( "Error ellipses and height errors are " + format_fixed( confidence_limit, 1 ) + " times standard error" );
        }

        if( dimension != 1 )
        {
            AddString( "Ellipses exaggerated " + format_fixed( errell_scale, 0 ) + " times" );
        }
        if( dimension != 2 )
        {
            AddString( "Height errors exaggerated " + format_fixed( hgterr_scale, 0 ) + " times" );
        }
    }

    std::string highlightText;
    get_obs_highlight_option( &opt, &threshold );
    switch(opt)
    {
    case PCONN_HIGHLIGHT_REJECTED:
        highlightText = "Rejected observations are highlighted"; break;
    case PCONN_HIGHLIGHT_SRES:
        highlightText = "Apriori standardised residuals greater than " + format_fixed( threshold, 2 ) + " are highlighted"; break;
    case PCONN_HIGHLIGHT_APOST_SRES:
        highlightText = "Aposteriori standardised residuals greater than " + format_fixed( threshold, 2 ) + " are highlighted"; break;
    case PCONN_HIGHLIGHT_RFAC:
        highlightText = "Observations with redundancy less than " + format_fixed( threshold, 2 ) + " are highlighted"; break;
    }
    if( !highlightText.empty() ) { AddString( highlightText ); }

    if( have_binary_data() )
    {
        AddNewLine();
        AddString("Adjustment statistics");

        AddString( "Number of observations:           " + pad_left( std::to_string( nobs + nschp ), 5 ) );
        AddString( "Number of parameters:             " + pad_left( std::to_string( nprm ), 5 ) );
        if(nschp)
        {
            AddString( "Number of implicit parameters:    " + pad_left( std::to_string( nschp ), 5 ) );
        }
        if(ncon)
        {
            AddString( "Number of arbitrary constraints:  " + pad_left( std::to_string( ncon ), 5 ) );
        }
        AddString( "Degrees of freedom:               " + pad_left( std::to_string( dof ), 5 ) );
        AddString( "Sum of squared residuals:         " + format_fixed( ssr, 5, 11 ) );
        AddString( "Standard error of unit weight:    " + format_fixed( seu, 5, 11 ) );
    }

    AddNewLine();
    list_station_summary( this, AddInfoText );

    DisplayText();
}

void SnapplotDetailsView::ShowStationInfo( int istn )
{

    /*
    SaveInfoLocation();

    JumpDef jmp;
    jmp.ToStation( istn );
    currentItem = jmp;
    ccMessage::PostMessage( ccMessage( msgShowInfoStns, istn, (int) 0 ) );

    mode = DefaultMode;
    */

    if( ! istn ) return;
    ClearText();
    list_station_details( this, AddInfoText, istn );
    list_connections( this, AddInfoText, istn );
    DisplayText();


}


void SnapplotDetailsView::ShowLineInfo( int from, int to )
{

    /*
    SaveInfoLocation();

    JumpDef jmp;
    jmp.ToLine( from, to );
    currentItem = jmp;
    ccMessage::PostMessage( ccMessage( msgShowInfoStns, from, to ) );

    mode = DefaultMode;
    */

    if( ! from || ! to ) return;

    ClearText();

    list_observations( this, AddInfoText, from, to );

    DisplayText();
}

void SnapplotDetailsView::ShowObsInfo( int from, int to, int obs_id )
{

    /*
    SaveInfoLocation();

    JumpDef jmp;
    jmp.ToObs( from, to, obs_id );
    currentItem = jmp;
    ccMessage::PostMessage( ccMessage( msgShowInfoStns, from, to ) );

    mode = DefaultMode;

    */

    ClearText();
    list_single_observation( this, AddInfoText, from, to, obs_id );
    DisplayText();
}
