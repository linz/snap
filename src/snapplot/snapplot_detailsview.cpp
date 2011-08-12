#include "snapconfig.h"
// Module supports a window that can be used to display information about the
// stations, connections, or individual observations...

#include "snapplot_detailsview.hpp"

//extern "C" {
#include "snap/snapglob.h"
#include "plotstns.h"
#include "plotconn.h"
#include "plotscal.h"
#include "snap/stnadj.h"
#include "util/dms.h"
#include "util/pi.h"
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
    if( sscanf( string.c_str(),"PTI:%d:%d:%d:%d",&itype,&ifrom,&ito,&iobs_id ) != EOF )
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
    newText.Append(_T("<html><body><pre>"));
}

void SnapplotDetailsView::AddString( const char *text, bool newLine )
{
    newText.Append(_T(text) );
    if( newLine ) AddNewLine();
}

void SnapplotDetailsView::AddInfoText( PutTextInfo *jump, const char *text, bool addNewLine )
{
    wxString s(_T(text));
    s.Replace(_T("&"),_T("&amp;"));
    s.Replace(_T("<"),_T("&lt;"));
    s.Replace(_T(">"),_T("&gt;"));
    if( jump && jump->type != ptfNone )
    {
        PutTextInfoWriter pti(*jump);
        newText.Append(_T("<a href=\""));
        newText.Append(pti.ToString());
        newText.Append(_T("\">"));
        newText.Append(s);
        newText.Append(_T("</a>"));
    }
    else
    {
        newText.Append(s);
    }
    if( addNewLine ) AddNewLine();
}

void SnapplotDetailsView::AddNewLine()
{
    newText.Append(_T("\n"));
}


void SnapplotDetailsView::DisplayText()
{
    newText.Append(_T("</pre></body></html>"));
    SetPage( newText );
    newText.Empty();
}


void SnapplotDetailsView::AddInfoText( void *win, PutTextInfo *jump, const char *text )
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
    int nch;
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
        sprintf(buf,"Displaying %s errors", aposteriori_errors ? "aposteriori" : "apriori");
        AddString( buf );
        if( use_confidence_limit )
        {
            sprintf(buf,"Error ellipses and height errors are %.2lf%% confidence limits",confidence_limit );
            AddString( buf );
        }

        nch = 3;
        if( dimension != 1 )
        {
            sprintf(buf,"Ellipses exaggerated %.0lf times",errell_scale);
            AddString( buf );
            nch++;
        }
        if( dimension != 2 )
        {
            sprintf(buf,"Height errors exaggerated %.0lf times",hgterr_scale);
            AddString( buf );
        }
    }

    buf[0] = 0;
    get_obs_highlight_option( &opt, &threshold );
    switch(opt)
    {
    case PCONN_HIGHLIGHT_REJECTED:
        sprintf(buf,"Rejected observations are highlighted"); break;
    case PCONN_HIGHLIGHT_SRES:
        sprintf(buf,"Apriori standardised residuals greater than %.2lf are highlighted",
                threshold); break;
    case PCONN_HIGHLIGHT_APOST_SRES:
        sprintf(buf,"Aposteriori standardised residuals greater than %.2lf are highlighted",
                threshold); break;
    case PCONN_HIGHLIGHT_RFAC:
        sprintf(buf,"Observations with redundancy less than %.2lf are highlighted",
                threshold); break;
    }
    if( buf[0] ) { AddString( buf ); }

    if( have_binary_data() )
    {
        AddNewLine();
        AddString("Adjustment statistics");

        sprintf(buf,"Number of observations:           %5ld",(long)(nobs + nschp));
        AddString(buf);
        sprintf(buf,"Number of parameters:             %5ld",(long)(nprm));
        AddString(buf);
        if(nschp)
        {
            sprintf(buf,"Number of implicit parameters:    %5ld",(long)(nschp));
            AddString(buf);
        }
        if(ncon)
        {
            sprintf(buf,"Number of arbitrary constraints:  %5ld",(long)(ncon));
            AddString(buf);
        }
        sprintf(buf,"Degrees of freedom:               %5ld",(long)(dof));
        AddString(buf);
        sprintf(buf,"Sum of squared residuals:         %11.5lf",ssr);
        AddString(buf);
        sprintf(buf,"Standard error of unit weight:    %11.5lf",seu);
        AddString(buf);
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


// Old code ..
#if 0

#include <stdio.h>
#include <string.h>

#include "snapplot_textview.hpp"

//extern "C" {
#include "snap/snapglob.h"
#include "plotstns.h"
#include "plotconn.h"
#include "plotscal.h"
#include "snap/stnadj.h"
#include "util/dms.h"
#include "util/pi.h"
//};

static const int maxBackup = 20;

static ccMessageId msgShowInfoStns("ShowInfoStns");
static ccMessageId msgCentreInfoStns("CentreInfoStns");

class JumpDef : public PutTextInfo
{
public:
    void
    void Nowhere() { type = ptfNone; }
    void ToTitle() { type = ptfTitleBlock; }
    void ToStation( int id ) { type = ptfStation; from = id; }
    void ToLine( int sfrom, int sto )
    {
        type = ptfLine; from = sfrom; to = sto;
    }
    void ToObs( int sfrom, int sto, int sid )
    {
        type = ptfObs; from = sfrom; to = sto; obs_id = sid;
    }
    void ToSresList( int mode ) { type = ptfSres; from = mode; }
    void ToStnList( int mode ) { type = ptfStnList; from = mode; }
    void ToString();
};


InfoWin::InfoWin( int nrows, int ncols ) :
    ccScrollingTextWindow( nrows, ncols, 4, sizeof( PutTextInfo ) )
{
    selectOption = lbNoSelect;
    TrackDragging();
    JumpDef jmp;
    jmp.Nowhere();
    currentItem = jmp;
    mode = DefaultMode;
    oldLocations = new InfoWinLoc[maxBackup];
    nOldLocations = 0;
    maxOldLocations = maxBackup;
    nextOldLocation = 0;
    saveLocation = 1;
    maxRows = ccDisplay::Height()/(2*CalcItemHeight());
    minRows = 4;
}

InfoWin::~InfoWin()
{
    if( oldLocations ) delete [] oldLocations;
}


void InfoWin::AddInfoText( void *win, PutTextInfo *jump, const char *text )
{
    ((InfoWin *) win ) -> AddString( text, jump );
}

void InfoWin::ItemDraw( long id, char flags )
{
    int col = jumpColour;
    if( mode == DefaultMode )
    {
        PutTextInfo *info = (PutTextInfo *) Data(id);
        if( !info || info->type == ptfNone ) col = textColour;
    }
    SetTextColours( col, bkgColour );
    ccScrollingTextWindow::ItemDraw( id, flags );
}

void InfoWin::CursorButton( ccCursorEvent &event )
{
    if( event.type == ceRButtonDown )
    {
        BackupInfoLocation();
        return;
    }
    ccScrollingTextWindow::CursorButton( event );
}

void InfoWin::ItemCursorButton( long id, ccCursorEvent &event )
{
    if( event.type == ceLButtonDown )
    {
        PutTextInfo *info;
        PutTextInfo jmpInfo;
        if( mode == SresMode )
        {
            info = &jmpInfo;
            sres_item_info( id, info );
        }
        else if( mode == StnListMode )
        {
            sort_stations_by_name( showStationNames );
            jmpInfo.type = ptfStation;
            jmpInfo.from = sorted_station_number( (int) id );
            info = &jmpInfo;
        }
        else
        {
            info = (PutTextInfo *) Data(id);
        }

        if( info && info->type != ptfNone )
        {
            if( (info->type == ptfStation ||
                    info->type == ptfLine ||
                    info->type == ptfObs ) && ccKeyboard::ShiftPressed() )
            {
                int to = info->to;
                if( info->type == ptfStation ) to = 0;
                ccMessage::PostMessage(
                    ccMessage( msgCentreInfoStns, info->from, to ));
            }

            JumpToItem( info );
            return;
        }
    }
    ccScrollingTextWindow::ItemCursorButton( id, event );
}

void InfoWin::JumpToItem( PutTextInfo * item, long topRow )
{
    if( !item ) return;
    switch( item->type )
    {
    case ptfNone:
    case ptfStation: ShowStationInfo( item->from, topRow ); break;
    case ptfTitleBlock:   ShowTitleInfo( topRow ); break;
    case ptfLine:    ShowLineInfo( item->from, item->to, topRow  ); break;
    case ptfObs:     ShowObsInfo( item->from, item->to, item->obs_id, topRow ); break;
    case ptfSres:    ShowSresList( item->from, topRow ); break;
    case ptfStnList: ShowStationList( item->from, topRow ); break;
    }
    return;
}

void InfoWin::SaveInfoLocation()
{
    if( !saveLocation ) return;
    if( currentItem.type == ptfNone ) return;
    InfoWinLoc &loc = oldLocations[nextOldLocation];
    * (PutTextInfo *) &loc = currentItem;
    loc.topRow = TopItem();
    nextOldLocation++;
    if( nextOldLocation >= maxOldLocations ) nextOldLocation = 0;
    if( nOldLocations < maxOldLocations ) nOldLocations++;
}

void InfoWin::BackupInfoLocation()
{
    if( nOldLocations <= 0 ) return;
    nextOldLocation--;
    if( nextOldLocation < 0 ) nextOldLocation += maxOldLocations;
    nOldLocations--;
    saveLocation = 0;
    JumpToItem( &oldLocations[nextOldLocation], oldLocations[nextOldLocation].topRow );
    saveLocation = 1;
}

///////////////////////////////////////////////////////////////////////////////
// Specific outputs...


static char buf[120];

void InfoWin::ShowSresList(  int listmode, long topRow )
{

    SaveInfoLocation();

    JumpDef jmp;
    jmp.ToSresList( listmode );
    currentItem = jmp;

    mode = SresMode;

    SuspendUpdates();
    ClearText();
    set_sres_display_option( listmode );
    SetCount( sres_index_count() );
    SetTopItem( topRow );
    SuspendUpdates(0);
}


void InfoWin::ShowStationList(  int listmode, long topRow )
{

    SaveInfoLocation();

    JumpDef jmp;
    jmp.ToStnList( listmode );
    currentItem = jmp;

    mode = StnListMode;
    showStationNames = listmode;

    SuspendUpdates();
    ClearText();
    sort_stations_by_name( listmode );
    SetCount( used_station_count() );
    SetTopItem( topRow );
    SuspendUpdates(0);
}

const char *InfoWin::ItemText( long id )
{
    if( mode == SresMode ) return sres_item_description( id );
    if( mode == StnListMode )
    {
        sort_stations_by_name( showStationNames );
        int s = sorted_station_number( (int) id );
        sprintf( buf, "%-10s %.50s",stnptr(s)->Code, stnptr(s)->Name );
        return buf;
    }
    return ccScrollingTextWindow::ItemText(id);
}

char InfoWin::GetSelect( long id )
{
    if( mode != DefaultMode ) return 0;
    return ccScrollingTextWindow::GetSelect(id);
}

char InfoWin::SetSelect( long id, char on )
{
    if( mode != DefaultMode ) return 0;
    return ccScrollingTextWindow::SetSelect(id,on);
}
#endif
