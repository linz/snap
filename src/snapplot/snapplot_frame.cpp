#include "snapconfig.h"
/////////////////////////////////////////////////////////////////////////////
// Name:        snapplot_frame
// Purpose:     Main window for snapplot application
// Author:      Chris Crook
//
// This version built with the wxWidgets (version 2.8.0 - patched) windows
// development toolkit.
//
// Built with MS Visual Studio.
/////////////////////////////////////////////////////////////////////////////

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx_includes.hpp"

#include "snapplot_frame.hpp"
#include "snapplot_eventids.hpp"
#include "snapplot_layers.hpp"
#include "snapplot_map.hpp"
#include "snapplot_mapprinter.hpp"
#include "snapplot_help.hpp"

#include "wxhelpabout.hpp"

#include "dlg_obs_options.hpp"
#include "dlg_pen_options.hpp"
#include "dlg_obs_highlight.hpp"
#include "dlg_obs_list_options.hpp"
#include "dlg_error_options.hpp"
#include "dlg_station_highlight.hpp"
#include "dlg_findstation.hpp"

//extern "C"
//{
#include "util/errdef.h"
#include "util/classify.h"
#include "snap/snapglob.h"
#include "snap/stnadj.h"
#include "plotscal.h"
#include "plotstns.h"
#include "plotpens.h"
#include "plotconn.h"
#include "plotcmd.h"
#include "dxfplot.h"
//}

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// SnapplotFrame
// ----------------------------------------------------------------------------

BEGIN_EVENT_TABLE(SnapplotFrame, wxFrame)
    EVT_MENU(CMD_FILE_SAVECONFIG, SnapplotFrame::OnCmdSaveConfig )
    EVT_MENU(CMD_FILE_RESTORECONFIG, SnapplotFrame::OnCmdRestoreConfig )
    EVT_MENU(CMD_FILE_EXPORTDXF, SnapplotFrame::OnCmdExportDxf )
    EVT_MENU(CMD_FILE_EXPORTIMAGE, SnapplotFrame::OnCmdExportImage )
    EVT_MENU(CMD_FILE_PRINTMAP, SnapplotFrame::OnCmdPrintMap )
    EVT_MENU(CMD_FILE_CLOSE, SnapplotFrame::Quit)

    EVT_MENU(CMD_COPY_MAP, SnapplotFrame::OnCmdCopyMap )
    EVT_MENU(CMD_COPY_DETAILS, SnapplotFrame::OnCmdCopyDetails )
    EVT_MENU(CMD_COPY_STATIONS, SnapplotFrame::OnCmdCopyStations )
    EVT_MENU(CMD_COPY_OBSLIST, SnapplotFrame::OnCmdCopyObsList )

    EVT_MENU(CMD_VIEW_ALL, SnapplotFrame::OnCmdViewAll )
    EVT_MENU(CMD_VIEW_ZOOMOUT, SnapplotFrame::OnCmdViewZoomOut )
    EVT_MENU(CMD_VIEW_ZOOMPREV, SnapplotFrame::OnCmdViewZoomPrev )
    EVT_MENU(CMD_VIEW_STATIONLIST, SnapplotFrame::OnCmdViewStationList )
    EVT_MENU(CMD_VIEW_OBSLIST, SnapplotFrame::OnCmdViewObsList )
    EVT_MENU(CMD_VIEW_DETAILS,SnapplotFrame::OnCmdViewDetails )
    EVT_MENU(CMD_VIEW_ADD_COLOUR, SnapplotFrame::OnCmdViewAddColour )
    EVT_MENU(CMD_VIEW_MAPFONT, SnapplotFrame::OnCmdViewMapFont )

    EVT_MENU(CMD_STATION_COLOURBY_USAGE, SnapplotFrame::OnCmdStationColourBy )
    EVT_MENU(CMD_STATION_OPTIONS, SnapplotFrame::FunctionNotImplemented )
    EVT_MENU(CMD_STATION_HIGHLIGHT, SnapplotFrame::OnCmdStationHighlight )
    EVT_MENU(CMD_STATION_HIDESHOW, SnapplotFrame::OnCmdStationHideShow )
    EVT_MENU(CMD_STATION_IGNOREOFFSETS, SnapplotFrame::OnCmdStationIgnoreOffsets )
    EVT_MENU(CMD_STATION_FINDSTATION, SnapplotFrame::OnCmdStationFindStation )

    EVT_MENU(CMD_DATA_OPTIONS, SnapplotFrame::OnCmdDataOptions )
    EVT_MENU(CMD_DATA_HIGHLIGHT, SnapplotFrame::OnCmdDataHighlight )
    EVT_MENU(CMD_DATA_LISTOPTIONS, SnapplotFrame::OnCmdDataListOptions )

    EVT_MENU_RANGE( CMD_COLOURBY_FIRST, CMD_COLOURBY_LAST, SnapplotFrame::OnCmdColourBy )

    EVT_MENU(CMD_ERROR_OPTIONS, SnapplotFrame::OnCmdErrorOptions )

    EVT_MENU(CMD_HELP_HELP, SnapplotFrame::OnCmdHelpHelp )
    EVT_MENU(CMD_HELP_ABOUT, SnapplotFrame::OnCmdHelpAbout)

    EVT_COMMAND(wxID_ANY, WX_MAPVIEW_DISPLAY_COORDS, SnapplotFrame::OnDisplayCoords )

    EVT_MENU_OPEN( SnapplotFrame::OnMenuOpen )
    EVT_SIMPLE_EVENT(WX_SYMBOLOGY_CHANGED, SnapplotFrame::OnSymbologyChanged )

    EVT_SNAPPLOT_EVENT( wxEVT_SET_DETAILVIEW, SnapplotFrame::OnSetDetails )
    EVT_SNAPPLOT_EVENT( wxEVT_SHOW_DETAILVIEW, SnapplotFrame::OnShowDetails )
    EVT_SNAPPLOT_EVENT( wxEVT_ZOOM_MAPVIEW, SnapplotFrame::OnZoomMapview )
    EVT_SNAPPLOT_EVENT( wxEVT_LOCATE_MAPVIEW, SnapplotFrame::OnLocateMapview )
END_EVENT_TABLE()

// SnapPlot frame constructor
SnapplotFrame::SnapplotFrame()
    : wxFrame(NULL, wxID_ANY, _T("Snapplot"),
              wxDefaultPosition, wxDefaultSize,
              wxDEFAULT_FRAME_STYLE)
{

    map = 0;
    dataColourMenu = 0;
    stationColourMenuItem = 0;
    stationColourMenu = 0;
    configMenu = 0;
    ignoreOffsetsItem = 0;
    help = 0;
    nextCommandId = CMD_USER_CONFIG_FIRST;
    classifyCommandFirst = 0;
    classifyCommandLast = 0;
    stationColourCommandFirst = 0;
    stationColourCommandLast = 0;
    configMenuCommandFirst = 0;
    configMenuCommandLast = 0;
    printData = 0;
    pageData = 0;
    zoomOnFind = true;

    // Set the size to the maximum available on the display

    wxRect windowRect = wxGetClientDisplayRect();
    SetPosition( windowRect.GetPosition() );
    SetSize( windowRect.GetSize() );

    // Create the status bar

    CreateStatusBar(3);
    int statusWidths[] = { -2, -1, -1 };
    GetStatusBar()->SetStatusWidths( 3, statusWidths );
    SetErrorTypeStatus();

    // Make a menubar

    CreateMenu();

    // Set up the main windows

    CreateMainWindows();

    // Setup icons

    SetupIcons();

    // Setup  help controller

    SetupHelp();

    // Setup printing

    SetupPrinter();

    // Install the data

    SetupData();

}

SnapplotFrame::~SnapplotFrame()
{
    if( map ) delete map;
    if( help ) delete help;
    if( printData ) delete printData;
    if( pageData ) delete pageData;
}


void SnapplotFrame::SetupIcons()
{
    wxIconBundle icons;
    icons.AddIcon( wxIcon(wxICON(ICO_SNAP16)) );
    icons.AddIcon( wxIcon(wxICON(ICO_SNAP32)) );
    SetIcons( icons );
}

void SnapplotFrame::SetupHelp()
{
    help = new wxHelpController( this );

    wxFileName helpFile(wxStandardPaths::Get().GetExecutablePath());
    helpFile.SetName(_T("snaphelp"));
    help->Initialize( helpFile.GetFullPath() );
}

void SnapplotFrame::SetupPrinter()
{
    printData = new wxPrintData();
    printData->SetOrientation( wxLANDSCAPE );
    pageData = new wxPageSetupDialogData();
}
void SnapplotFrame::CreateMainWindows()
{

    wxSplitterWindow *textDivider = new wxSplitterWindow( this );
    wxSplitterWindow *keyDivider = new wxSplitterWindow( (wxWindow *) textDivider );
    dataView = new wxNotebook( textDivider, -1, wxDefaultPosition, wxDefaultSize, wxNB_TOP );
    stationListWindow = new SnapplotStationList( dataView, -1 );
    obsListWindow = new SnapplotObsList( dataView, -1 );
    //detailsWindow = new wxTextCtrl( dataView, -1, _T("SNAPPLOT"), wxDefaultPosition, wxDefaultSize,
    //	wxTE_READONLY | wxTE_MULTILINE | wxHSCROLL );
    detailsWindow = new SnapplotDetailsView( dataView );

    detailsPage = dataView->GetPageCount();
    dataView->AddPage( detailsWindow, "Details", true );

    stationListPage = dataView->GetPageCount();
    dataView->AddPage( stationListWindow, "Stations", false );

    obsListPage = dataView->GetPageCount();
    dataView->AddPage( obsListWindow, "Observations", false );

    mapWindow = new SnapplotMapView( keyDivider );
    mapWindow->GetScale().SetZoomStackSize(20);
    wxSplitterWindow *infoDivider = new wxSplitterWindow( keyDivider, -1 );
    keyWindow = new wxLayerKey( infoDivider, -1 );
    mapInfoWindow = new wxStaticText( infoDivider, -1, _T(""),wxDefaultPosition, wxDefaultSize, wxST_NO_AUTORESIZE );

    int mapHeight = (GetClientSize().GetHeight()*4)/5;
    textDivider->SetMinimumPaneSize( 50 );
    textDivider->SetSashGravity(0.8);
    textDivider->SplitHorizontally( keyDivider, dataView, mapHeight);

    int twidth, theight;
    keyWindow->GetTextExtent(_T("MMMMM"),&twidth,&theight);
    keyDivider->SetMinimumPaneSize( 50 );
    keyDivider->SetSashGravity(1.0);
    keyDivider->SplitVertically( mapWindow, infoDivider, GetClientSize().GetWidth() - twidth*5 );

    mapInfoWindow->GetTextExtent(_T("MMMMM"),&twidth,&theight);
    infoDivider->SetMinimumPaneSize( 20 );
    infoDivider->SetSashGravity(1.0);
    infoDivider->SplitHorizontally(keyWindow,mapInfoWindow, mapHeight - theight * 6);
}


void SnapplotFrame::CreateMenu()
{

    wxMenuBar *menuBar = new wxMenuBar;

    wxMenu *fileMenu = new wxMenu;
    fileMenu->Append(CMD_FILE_SAVECONFIG,
                     _T("&Save configuration ..."),
                     _T("Saves snapplot configuration settings to a file"));
    fileMenu->Append(CMD_FILE_RESTORECONFIG,
                     _T("&Load configuration ..."),
                     _T("Reload a previously saved configuration"));

    fileMenu->AppendSeparator();
    fileMenu->Append(CMD_FILE_EXPORTIMAGE,
                     _T("Export &Image ..."),
                     _T("Export the current map to an image file"));

    fileMenu->Append(CMD_FILE_EXPORTDXF,
                     _T("Export &DXF file ..."),
                     _T("Export the current map view to a DXF file"));

    fileMenu->AppendSeparator();
    fileMenu->Append(CMD_FILE_PRINTMAP,
                     _T("&Print map ..."),
                     _T("Print the current map view "));

    fileMenu->AppendSeparator();
    fileMenu->Append(CMD_FILE_CLOSE,
                     _T("&Exit\tAlt-F4"),
                     _T("Exit snapplot"));

    menuBar->Append( fileMenu, _T("&File") );

    wxMenu *copyMenu = new wxMenu;
    copyMenu->Append( CMD_COPY_MAP,
                      _T("&Map"),
                      _T("Copy the map to the clipboard"));
    copyMenu->Append( CMD_COPY_DETAILS,
                      _T("&Details"),
                      _T("Copy the details information to the clipboard"));
    copyMenu->Append( CMD_COPY_STATIONS,
                      _T("&Stations"),
                      _T("Copy the stations list to the clipboard"));
    copyMenu->Append( CMD_COPY_OBSLIST,
                      _T("&Observations"),
                      _T("Copy the observations list to the clipboard"));
    menuBar->Append( copyMenu, _T("&Copy") );

    wxMenu *viewMenu = new wxMenu;
    viewMenu->Append( CMD_VIEW_ALL,
                      _T("&All\tAlt-A"),
                      _T("View entire network"));
    viewMenu->Append( CMD_VIEW_ZOOMOUT,
                      _T("&Zoom out\tAlt-Z"),
                      _T("Zoom out around current view"));
    viewMenu->Append( CMD_VIEW_ZOOMPREV,
                      _T("&Previous view\tAlt-P"),
                      _T("Restore the previous view of the map"));
    viewMenu->AppendSeparator();
    viewMenu->Append( CMD_VIEW_STATIONLIST,
                      _T("View s&tation list\tAlt-T"),
                      _T("View the list of stations in the text window"));
    viewMenu->Append( CMD_VIEW_OBSLIST,
                      _T("View o&bservation list\tAlt-B"),
                      _T("View the list of observations in the text window"));
    viewMenu->AppendSeparator();
    viewMenu->Append( CMD_VIEW_ADD_COLOUR,
                      _T("A&dd colour to palette ..."),
                      _T("Add a colour to the palette of colours available in the key"));
    viewMenu->Append( CMD_VIEW_MAPFONT,
                      _T("Select station &font ..."),
                      _T("Choose the font used for station codes and names on the map"));

    menuBar->Append( viewMenu, _T("&View"));

    wxMenu *stationMenu = new wxMenu;
    /*
    stationMenu->Append( CMD_STATION_OPTIONS,
    	              _T("Display &options ..."),
    				  _T("Options for how stations are shown"));
    */
    stationColourMenu = new wxMenu;

    stationColourMenu->Append(CMD_STATION_COLOURBY_USAGE,
                              _T("&Usage"),
                              _T("Colour stations according to their usage in the adjustment"));

    stationColourMenuItem = stationMenu->Append( CMD_STATION_COLOURBY,
                            _T("&Colour coding"),
                            stationColourMenu,
                            _T("Choose how stations are coloured on the map"));
    stationMenu->Append( CMD_STATION_HIGHLIGHT,
                         _T("&Highlight stations .."),
                         _T("Choose stations to highlight on the map"));
    stationMenu->Append( CMD_STATION_HIDESHOW,
                         _T("Hi&de stations .."),
                         _T("Select which stations to hide and which to display on the map"));

    ignoreOffsetsItem =
        stationMenu->AppendCheckItem( CMD_STATION_IGNOREOFFSETS,
                                      _T("&Ignore station offsets"),
                                      _T("Show all stations in their true positions ignoring applied offsets"));

    stationMenu->Append( CMD_STATION_FINDSTATION,
                         _T("&Find station\tCtrl-F"),
                         _T("Find a station"));

    menuBar->Append( stationMenu, _T("&Stations") );

    wxMenu *dataMenu = new wxMenu;
    dataColourMenu = new wxMenu;
    dataColourMenu->Append( CMD_COLOURBY_DATATYPE,
                            _T("Data &type"),
                            _T("Colour observations according to data type"));
    dataColourMenu->Append( CMD_COLOURBY_DATAFILE,
                            _T("Data &file"),
                            _T("Colour observations according to data type"));
    dataColourMenu->Append( CMD_COLOURBY_RESIDUAL,
                            _T("&Residual ..."),
                            _T("Colour observations according to standardised residual in adjustment"));
    dataColourMenu->Append( CMD_COLOURBY_REDUNDANCY,
                            _T("Re&dundancy ..."),
                            _T("Colour observations according to redundancy factor in adjustment"));

    dataMenu->Append( CMD_DATA_COLOURBY,
                      _T("&Colour coding ..."),
                      dataColourMenu,
                      _T("Choose how observations are coloured coded on the map"));
    dataMenu->Append( CMD_DATA_HIGHLIGHT,
                      _T("&Highlight observations ..."),
                      _T("Choose which observations are highlighted"));
    dataMenu->Append( CMD_DATA_OPTIONS,
                      _T("Display &options ..."),
                      _T("Choose how observations are displayed"));

    dataMenu->Append( CMD_DATA_LISTOPTIONS,
                      _T("L&isting options"),
                      _T("Choose what information is listed for each observation in the grid"));

    menuBar->Append( dataMenu, _T("&Observations"));

    wxMenu *errorMenu = new wxMenu;

    if( have_binary_data() )
    {
        errorMenu->Append( CMD_ERROR_OPTIONS,
                           _T("&Error options ..."),
                           _T("Choose how errors are displayed"));
    }

    menuBar->Append( errorMenu, _T("&Errors"));

    configMenu = new wxMenu;
    menuBar->Append( configMenu, _T("Co&nfiguration"));

    wxMenu *helpMenu = new wxMenu;
    helpMenu->Append( CMD_HELP_HELP,
                      _T("&Help\tF1"),
                      _T("Get help about snapplot"));
    helpMenu->AppendSeparator();
    helpMenu->Append( CMD_HELP_ABOUT,
                      _T("&About"),
                      _T("Information about this version snaplot program"));

#if TEST_FUNCTION
    helpMenu->Append( CMD_TEST1, _T("&Test1"), _T("Test function ... used to test things!!!") );
    helpMenu->Append( CMD_TEST2, _T("&Test1"), _T("Test function ... used to test things!!!") );
#endif

    menuBar->Append( helpMenu, _T("&Help") );

    SetMenuBar(menuBar);
}

void SnapplotFrame::SetupData()
{
    // if( map ) delete map;
    // if( symbologyKey ) delete symbologyKey;

    if( command_file ) SetLabel(wxString("Snapplot - " )+ wxString(_T(command_file)));
    map = new SnapplotMap();
    mapWindow->SetMap( map );
    SetupSymbology();
    AddStationColourOptions();
    AddColourByClassifications();
    AddConfigMenuItems();
    detailsWindow->Show( PutTextInfoWriter(ptfTitleBlock));
    stationListWindow->Reload();
    obsListWindow->Reload();
}

void SnapplotFrame::SetErrorTypeStatus()
{
    wxString msg = wxString::Format("Error type: %s",
                                    aposteriori_errors ? "A posteriori" : "A priori" );
    GetStatusBar()->SetStatusText(msg,1);
    if( use_confidence_limit )
    {
        msg = wxString::Format("Coord errors: %.2lf%% conf limit",confidence_limit);
    }
    else if( confidence_limit != 1.0 )
    {
        msg = wxString::Format("Coord errors: %.1lf times standard error",confidence_limit);
    }
    else
    {
        msg = "Coord errors: standard error";
    }
    GetStatusBar()->SetStatusText(msg,2);
}

void SnapplotFrame::AddStationColourOptions()
{
    stationColourCommandFirst = nextCommandId;
    int nitems = 0;
    for( int i = 0; i++ < network_classification_count( net ); )
    {
        wxString menuText = wxString::Format("&%d %.40s",i,network_class_name( net,i) );
        stationColourMenu->Append( nextCommandId,
                                   menuText,
                                   wxString::Format(_T("Colour stations according to %s"),
                                           network_class_name( net,i) )
                                 );
        Connect( nextCommandId, wxEVT_COMMAND_MENU_SELECTED,
                 wxCommandEventHandler(SnapplotFrame::OnCmdStationColourBy));

        stationColourCommandLast = nextCommandId;
        nextCommandId++;
        nitems++;
    }
    if( nitems == 0 )
    {
        stationColourMenuItem->Enable(false);
    }
}

void SnapplotFrame::AddColourByClassifications()
{
    // Add classifications to the colour by menu ...

    if( classification_count( &obs_classes) > 0 )
    {
        classifyCommandFirst = nextCommandId;
        for( int i = 0; i++ < classification_count( &obs_classes); )
        {
            wxString menuText = wxString::Format("&%d %.40s",i,classification_name( &obs_classes,i) );
            dataColourMenu->Append( nextCommandId,
                                    menuText,
                                    wxString::Format(_T("Colour observations according to %s classification"),
                                            classification_name( &obs_classes,i) )
                                  );
            Connect( nextCommandId, wxEVT_COMMAND_MENU_SELECTED,
                     wxCommandEventHandler(SnapplotFrame::OnCmdColourBy));

            classifyCommandLast = nextCommandId;
            nextCommandId++;
        }
    }
}

void SnapplotFrame::AddConfigMenuItems()
{
    // Add classifications to the colour by menu ...

    if( config_menu_item_count() > 0 )
    {
        configMenuCommandFirst = nextCommandId;
        for( int i = 0; i < config_menu_item_count(); i++ )
        {
            wxString menuText = wxString::Format("&%d %.40s",i+1,config_menu_text(i) );
            configMenu->Append( nextCommandId,
                                menuText,
                                wxString::Format(_T("Load configuration file for %s"),
                                                 config_menu_text(i) )
                              );
            Connect( nextCommandId, wxEVT_COMMAND_MENU_SELECTED,
                     wxCommandEventHandler(SnapplotFrame::OnCmdReadConfig));

            configMenuCommandLast = nextCommandId;
            nextCommandId++;
        }
    }
}


void SnapplotFrame::SetupSymbology()
{
    Symbology *symbology = GetSnapplotSymbology();
    keyWindow->SetSymbology( symbology );
    mapWindow->SetSymbology( symbology );
}

void SnapplotFrame::ShowStationList()
{
    dataView->ChangeSelection( stationListPage );
}

void SnapplotFrame::ShowObsList()
{
    dataView->ChangeSelection( obsListPage );
}

void SnapplotFrame::ReadConfiguration( char *filename )
{
    process_configuration_file( filename );
    SetupSymbology();
    mapWindow->RedrawMap();
    dataView->Refresh();
}

// menu command handlers

void SnapplotFrame::Quit(wxCommandEvent& WXUNUSED(event) )
{
    Close(true);
}

void SnapplotFrame::OnCmdSaveConfig( wxCommandEvent & WXUNUSED(event) )
{
    // TODO: Want to add options here for which bits of configuration to save ...


    wxString cmdDir;
    if( cmd_dir ) { cmdDir.Append(_T(cmd_dir)); }
    if( cmdDir.IsEmpty() ) { cmdDir.Append(_T(".")); }
    wxFileDialog dlgFile(
        this,
        _T("Select configuration filename for save"),
        cmdDir,
        _T("*" SNAPPLOT_CONFIG_EXT),
        _T("Snapplot config files (" SNAPPLOT_CONFIG_EXT ")|" SNAPPLOT_CONFIG_EXT "|All files (*.*)|*.*"),
        wxSAVE | wxOVERWRITE_PROMPT
    );

    if( dlgFile.ShowModal() == wxID_OK &&
            ! save_configuration( const_cast<char *>(dlgFile.GetPath().c_str()) ) )
    {
        wxMessageBox(
            wxString::Format( _T("Unable to save configuration to %s"), dlgFile.GetFilename().c_str()),
            _T("Configuration save error"),
            wxOK,
            this
        );
    }
}

void SnapplotFrame::OnCmdRestoreConfig( wxCommandEvent & WXUNUSED(event) )
{
    wxString cmdDir;
    if( cmd_dir ) { cmdDir.Append(_T(cmd_dir)); }
    if( cmdDir.IsEmpty() ) { cmdDir.Append(_T(".")); }
    wxString configFile = wxFileSelector(
                              _T("Select configuration file to restore"),
                              cmdDir,
                              _T("*" SNAPPLOT_CONFIG_EXT),
                              _T(SNAPPLOT_CONFIG_EXT),
                              _T("Snapplot config files (" SNAPPLOT_CONFIG_EXT ")|" SNAPPLOT_CONFIG_EXT "|All files (*.*)|*.*" ),
                              wxOPEN | wxFILE_MUST_EXIST,
                              this
                          );

    if( ! configFile.IsEmpty() )
    {
        ReadConfiguration( const_cast<char *>(configFile.c_str()) );
    }
}

void SnapplotFrame::OnCmdExportImage( wxCommandEvent & WXUNUSED(event) )
{

    wxString cmdDir;
    if( cmd_dir ) { cmdDir.Append(_T(cmd_dir)); }
    if( cmdDir.IsEmpty() ) { cmdDir.Append(_T(".")); }
    wxFileDialog dlgFile(
        this,
        _T("Image file name"),
        cmdDir,
        _T("*.png"),
        _T("PNG file (*.png)|*.png|Windows bitmap file (*.bmp)|*.bmp"),
        wxSAVE | wxOVERWRITE_PROMPT
    );

    if( dlgFile.ShowModal() != wxID_OK ) return;

    wxBitmap mapImage = mapWindow->CreateImage();
    wxImage img = mapImage.ConvertToImage();

    int type;

    switch ( dlgFile.GetFilterIndex() )
    {
    case 1:
        type = wxBITMAP_TYPE_BMP;
        break;

    default:
        type = wxBITMAP_TYPE_PNG;
        break;
    }

    bool ok = img.SaveFile( dlgFile.GetPath(), type );

    if( ! ok )
    {
        wxMessageBox(
            wxString::Format( _T("Unable to save image to %s"), dlgFile.GetFilename().c_str()),
            _T("Image export error"),
            wxOK,
            this
        );
    }
}

void SnapplotFrame::OnCmdExportDxf( wxCommandEvent & WXUNUSED(event) )
{

    wxString cmdDir;
    if( cmd_dir ) { cmdDir.Append(_T(cmd_dir)); }
    if( cmdDir.IsEmpty() ) { cmdDir.Append(_T(".")); }
    wxFileDialog dlgFile(
        this,
        _T("DXF file name"),
        cmdDir,
        _T("*.dxf"),
        _T("DXF file (*.dxf)|*.dxf"),
        wxSAVE | wxOVERWRITE_PROMPT
    );

    if( dlgFile.ShowModal() != wxID_OK ) return;

    if( open_dxf_file( dlgFile.GetPath().c_str() ) != OK )
    {
        wxMessageBox(
            wxString::Format( _T("Unable to save DXF file to %s"), dlgFile.GetFilename().c_str()),
            _T("DXF export error"),
            wxOK,
            this
        );
        return;
    }
    plot_dxf();
    close_dxf_file();

}

void SnapplotFrame::OnCmdPrintMap( wxCommandEvent & WXUNUSED(event) )
{
    SnapplotMapPrinter printout( GetLabel(), mapWindow->GetMap(), &(mapWindow->GetScale()), mapWindow->GetSymbology() );

    wxString cwd = wxGetCwd();

    wxPrintDialogData printDialogData( *printData);
    wxPrinter printer(& printDialogData);

    if (!printer.Print(this, &printout, true ))
    {
        if (wxPrinter::GetLastError() == wxPRINTER_ERROR)
            wxMessageBox(_T("There was a problem printing.\nPerhaps your current printer is not set correctly?"), _T("Printing"), wxOK);
        else
            wxMessageBox(_T("Printing cancelled"), _T("Printing"), wxOK);
    }
    else
    {
        (*printData) = printer.GetPrintDialogData().GetPrintData();
    }

    // As print dialogs may end up changing directory ...
    wxSetWorkingDirectory( cwd );

}

void SnapplotFrame::OnCmdCopyMap( wxCommandEvent & WXUNUSED(event) )
{
    if( wxTheClipboard->Open())
    {
        wxBitmap mapImage = mapWindow->CreateImage();
        wxTheClipboard->SetData(new wxBitmapDataObject( mapImage ) );
        wxTheClipboard->Close();
    }
}

void SnapplotFrame::OnCmdCopyDetails( wxCommandEvent & WXUNUSED(event) )
{
    if( wxTheClipboard->Open())
    {
        wxString text = detailsWindow->ToText();
        wxTheClipboard->SetData( new wxTextDataObject( text ) );
        wxTheClipboard->Close();
    }
}

void SnapplotFrame::OnCmdCopyStations( wxCommandEvent & WXUNUSED(event) )
{
    if( wxTheClipboard->Open())
    {
        wxString text = stationListWindow->GetText();
        wxTheClipboard->SetData( new wxTextDataObject( text ) );
        wxTheClipboard->Close();
    }
}

void SnapplotFrame::OnCmdCopyObsList( wxCommandEvent & WXUNUSED(event) )
{
    if( wxTheClipboard->Open())
    {
        wxString text = obsListWindow->GetText();
        wxTheClipboard->SetData( new wxTextDataObject( text ) );
        wxTheClipboard->Close();
    }
}


void SnapplotFrame::OnCmdViewAll(wxCommandEvent & WXUNUSED(event))
{
    mapWindow->GetScale().ZoomAll();
}

void SnapplotFrame::OnCmdViewZoomOut(wxCommandEvent & WXUNUSED(event))
{
    mapWindow->GetScale().ZoomOut();
}

void SnapplotFrame::OnCmdViewZoomPrev(wxCommandEvent & WXUNUSED(event))
{
    mapWindow->GetScale().ZoomPrev();
}

void SnapplotFrame::OnCmdViewStationList( wxCommandEvent & WXUNUSED(event) )
{
    dataView->ChangeSelection( stationListPage );
    stationListWindow->SetFocus();
}

void SnapplotFrame::OnCmdViewObsList( wxCommandEvent & WXUNUSED(event) )
{
    dataView->ChangeSelection( obsListPage );
    obsListWindow->SetFocus();
}

void SnapplotFrame::OnCmdViewDetails( wxCommandEvent & WXUNUSED(event) )
{
    dataView->ChangeSelection( detailsPage );
    detailsWindow->SetFocus();
}

void SnapplotFrame::OnCmdViewAddColour( wxCommandEvent & WXUNUSED(event) )
{
    wxColour none;
    wxColour colour = wxGetColourFromUser( this, none, _T("New palette colour"));
    if( colour.IsOk() )
    {
        Symbology *symbology = GetSnapplotSymbology();
        symbology->GetPalette()->AddColour( colour );
    }
}

void SnapplotFrame::OnCmdViewMapFont( wxCommandEvent & WXUNUSED(event) )
{
    wxFont stationFont;
    stationFont.SetNativeFontInfoUserDesc( _T(get_station_font()) );
    wxFont newFont = wxGetFontFromUser( this, stationFont, _T("New map font style"));
    if( newFont.IsOk() )
    {
        set_station_font( newFont.GetNativeFontInfoUserDesc().c_str() );
        mapWindow->RedrawMap();
    }
}

void SnapplotFrame::OnCmdDataOptions( wxCommandEvent & WXUNUSED(event) )
{
    if( RunObsOptsDialog( help )) mapWindow->RedrawMap();
}

void SnapplotFrame::OnCmdDataHighlight( wxCommandEvent & WXUNUSED(event) )
{
    if( SetupObsHighlights( help ) ) mapWindow->RedrawMap();
}

void SnapplotFrame::OnCmdDataListOptions( wxCommandEvent & WXUNUSED(event) )
{
    if( SetupObservationListOptions( help ) )
    {
        obsListWindow->Reload();
        ShowObsList();
    }
}



void SnapplotFrame::OnCmdColourBy( wxCommandEvent &event )
{
    int id = event.GetId();

    if( id >= classifyCommandFirst && id <= classifyCommandLast )
    {
        SetupDataPens( id + 1 - classifyCommandFirst);
    }
    else
    {
        switch (id)
        {
        case CMD_COLOURBY_DATATYPE:
            SetupDataPens(DPEN_BY_TYPE);
            break;
        case CMD_COLOURBY_DATAFILE:
            SetupDataPens(DPEN_BY_FILE);
            break;
        case CMD_COLOURBY_RESIDUAL:
            if( SetupStandardisedResidualPens( help ) ) SetupDataPens(DPEN_BY_SRES);
            break;
        case CMD_COLOURBY_REDUNDANCY:
            if( SetupRedundancyFactorPens( help ) ) SetupDataPens(DPEN_BY_RFAC);
            break;
        }
    }
}


void SnapplotFrame::OnCmdStationColourBy( wxCommandEvent &event )
{
    int id = event.GetId();

    if( id >= stationColourCommandFirst && id <= stationColourCommandLast )
    {
        SetupStationPens( id + 1 - stationColourCommandFirst);
    }
    else
    {
        SetupStationPens( 0 );
    }
}

void SnapplotFrame::OnCmdErrorOptions( wxCommandEvent & WXUNUSED(event) )
{
    if( SetupErrorOptions( help ) )
    {
        mapWindow->RedrawMap();
        detailsWindow->RefreshText();
        dataView->Refresh();
        SetErrorTypeStatus();
    }
}

void SnapplotFrame::OnCmdStationHighlight( wxCommandEvent & WXUNUSED(event) )
{
    if( SetupStationHighlights( false, help ) ) mapWindow->RedrawMap();
}

void SnapplotFrame::OnCmdStationHideShow( wxCommandEvent & WXUNUSED(event) )
{
    if( SetupStationHighlights( true, help ) ) mapWindow->RedrawMap();
}

void SnapplotFrame::OnCmdStationIgnoreOffsets( wxCommandEvent & WXUNUSED(event) )
{
    use_station_offsets( ! using_station_offsets());
    if( offset_station_count() > 0 ) mapWindow->RedrawMap();
}

void SnapplotFrame::OnCmdStationFindStation( wxCommandEvent & WXUNUSED(event) )
{
    int stationId = GetFindStationId( help, zoomOnFind );
    if( stationId > 0 )
    {
        wxSnapplotEvent spevent( wxEVT_SET_DETAILVIEW, GetId() );
        spevent.SetShowStation( stationId );
        spevent.SetEventObject(this);
        AddPendingEvent( spevent );

        if( zoomOnFind )
        {
            spevent.SetEventType( wxEVT_ZOOM_MAPVIEW );
            AddPendingEvent( spevent );
        }

        spevent.SetEventType( wxEVT_LOCATE_MAPVIEW );
        AddPendingEvent( spevent );
    }
}

void SnapplotFrame::OnCmdReadConfig( wxCommandEvent &event )
{
    int id = event.GetId();
    if( id >= configMenuCommandFirst && id <= configMenuCommandLast )
    {
        char *filename = config_menu_filename( id - configMenuCommandFirst );
        ReadConfiguration( filename );
    }
}

void SnapplotFrame::OnCmdHelpHelp( wxCommandEvent & WXUNUSED(event) )
{
    if( help ) help->DisplaySection( _T(HELPBASE "index.html"));
}

void SnapplotFrame::OnCmdHelpAbout( wxCommandEvent & WXUNUSED(event) )
{
    ShowHelpAbout();
}

void SnapplotFrame::FunctionNotImplemented( wxCommandEvent & WXUNUSED(event) )
{
    wxMessageDialog msgbox( this, _T("Sorry - this function is not yet implemented"),
                            _T("Warning"), wxOK );
    msgbox.ShowModal();
}


void SnapplotFrame::SetupDataPens( int dataPenType )
{
    setup_data_pens( dataPenType );
    SetupSymbology();
}

void SnapplotFrame::SetupStationPens( int stationPenType )
{
    setup_station_pens( stationPenType );
    SetupSymbology();
}


// Other events

void SnapplotFrame::OnMenuOpen( wxMenuEvent & WXUNUSED(event) )
{
    ignoreOffsetsItem->Check( ! using_station_offsets() );
}

void SnapplotFrame::OnSymbologyChanged(wxSimpleEvent& WXUNUSED(event) )
{
    mapWindow->RedrawMap();
}

void SnapplotFrame::OnDisplayCoords( wxCommandEvent &event )
{
    mapInfoWindow->SetLabel(event.GetString());
}

void SnapplotFrame::OnSetDetails( wxSnapplotEvent &event )
{
    detailsWindow->Show( event.GetInfo() );
}

void SnapplotFrame::OnShowDetails( wxSnapplotEvent &event )
{
    detailsWindow->Show( event.GetInfo() );
    dataView->ChangeSelection( detailsPage );
}

void SnapplotFrame::OnZoomMapview( wxSnapplotEvent &event )
{
    PutTextInfo info = event.GetInfo();
    switch( info.type )
    {
    case ptfStation:
        mapWindow->ZoomToStation( info.from );
        break;
    case ptfLine:
    case ptfObs:
        mapWindow->ZoomToLine( info.from, info.to );
        break;
    }
}

void SnapplotFrame::OnLocateMapview( wxSnapplotEvent &event )
{
    PutTextInfo info = event.GetInfo();
    switch( info.type )
    {
    case ptfStation:
        mapWindow->LocateStation( info.from );
        break;
    case ptfLine:
    case ptfObs:
        mapWindow->LocateLine( info.from, info.to );
        break;
    }
}
