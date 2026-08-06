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
#include <wx/popupwin.h>

#include <vector>

#include "snapplot_frame.hpp"
#include "snapplot_eventids.hpp"
#include "snapplot_layers.hpp"
#include "snapplot_map.hpp"
#include "snapplot_mapprinter.hpp"
#include "snapplot_help.hpp"

#include "wxhelpabout.hpp"

#ifdef __WXGTK__
#include "resources/snap16_icon.xpm"
#include "resources/snap32_icon.xpm"
#endif

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
#include "util/getversion.h"
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
    EVT_MENU(CMD_STATION_COLOURBY_RESETALLON, SnapplotFrame::OnCmdStationColourResetAllOn )
    EVT_MENU(CMD_STATION_COLOURBY_RESETALLOFF, SnapplotFrame::OnCmdStationColourResetAllOff )
    EVT_MENU(CMD_STATION_OPTIONS, SnapplotFrame::FunctionNotImplemented )
    EVT_MENU(CMD_STATION_HIGHLIGHT, SnapplotFrame::OnCmdStationHighlight )
    EVT_MENU(CMD_STATION_HIDESHOW, SnapplotFrame::OnCmdStationHideShow )
    EVT_MENU(CMD_STATION_IGNOREOFFSETS, SnapplotFrame::OnCmdStationIgnoreOffsets )
    EVT_MENU(CMD_STATION_FINDSTATION, SnapplotFrame::OnCmdStationFindStation )

    EVT_MENU(CMD_DATA_OPTIONS, SnapplotFrame::OnCmdDataOptions )
    EVT_MENU(CMD_DATA_HIGHLIGHT, SnapplotFrame::OnCmdDataHighlight )
    EVT_MENU(CMD_DATA_LISTOPTIONS, SnapplotFrame::OnCmdDataListOptions )

    EVT_MENU_RANGE( CMD_COLOURBY_FIRST, CMD_COLOURBY_LAST, SnapplotFrame::OnCmdColourBy )
    EVT_MENU(CMD_DATA_COLOURBY_RESETALLON, SnapplotFrame::OnCmdDataColourResetAllOn )
    EVT_MENU(CMD_DATA_COLOURBY_RESETALLOFF, SnapplotFrame::OnCmdDataColourResetAllOff )

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
    : wxFrame(NULL, wxID_ANY, "Snapplot",
              wxDefaultPosition, wxDefaultSize,
              wxDEFAULT_FRAME_STYLE)
{

    map = 0;
    dataColourMenu = 0;
    stationColourMenuItem = 0;
    stationColourMenu = 0;
    displayMenu = 0;
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
#ifdef __WXMSW__
    // snap.ico embeds 16/32/48/256px frames (for a crisp Explorer/shortcut icon), so an
    // explicit size is needed here to still get a distinct small and large icon -
    // wxICON()'s default load just returns whichever frame matches the system's
    // standard large-icon size for both calls.
    icons.AddIcon( wxIcon(wxT("ICO_SNAP16"), wxBITMAP_TYPE_ICO_RESOURCE, 16, 16) );
    icons.AddIcon( wxIcon(wxT("ICO_SNAP32"), wxBITMAP_TYPE_ICO_RESOURCE, 32, 32) );
#else
    icons.AddIcon( wxIcon(wxICON(ICO_SNAP16)) );
    icons.AddIcon( wxIcon(wxICON(ICO_SNAP32)) );
#endif
    SetIcons( icons );
}

void SnapplotFrame::SetupHelp()
{

    wxFileName helpFile(wxStandardPaths::Get().GetExecutablePath());
    helpFile.AppendDir("help");
    helpFile.SetName("snapplot_help");
    helpFile.SetExt("html");
    helpUrl=helpFile.GetFullPath();
    // help = new wxHelpController( this );
    // help->Initialize( helpFile.GetFullPath() );
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
    //detailsWindow = new wxTextCtrl( dataView, -1, "SNAPPLOT", wxDefaultPosition, wxDefaultSize,
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
    mapInfoWindow = new wxStaticText( infoDivider, -1, "",wxDefaultPosition, wxDefaultSize, wxST_NO_AUTORESIZE );

    int mapHeight = GetClientSize().GetHeight()/5;
    textDivider->SetMinimumPaneSize( 50 );
    textDivider->SetSashGravity(0.8);
    textDivider->SplitHorizontally( keyDivider, dataView, -mapHeight );

    int twidth, theight;
    keyWindow->GetTextExtent("MMMMM",&twidth,&theight);
    keyDivider->SetMinimumPaneSize( 50 );
    keyDivider->SetSashGravity(1.0);
    keyDivider->SplitVertically( mapWindow, infoDivider, -twidth*3 );

    mapInfoWindow->GetTextExtent("MMMMM",&twidth,&theight);
    infoDivider->SetMinimumPaneSize( 20 );
    infoDivider->SetSashGravity(1.0);
    infoDivider->SplitHorizontally(keyWindow,mapInfoWindow,-theight*6);
}


void SnapplotFrame::CreateMenu()
{

    wxMenuBar *menuBar = new wxMenuBar;

    wxMenu *fileMenu = new wxMenu;
    fileMenu->Append(CMD_FILE_SAVECONFIG,
                     "&Save configuration ...",
                     "Saves snapplot configuration settings to a file");
    fileMenu->Append(CMD_FILE_RESTORECONFIG,
                     "&Load configuration ...",
                     "Reload a previously saved configuration");

    fileMenu->AppendSeparator();
    fileMenu->Append(CMD_FILE_EXPORTIMAGE,
                     "Export &Image ...",
                     "Export the current map to an image file");

    fileMenu->Append(CMD_FILE_EXPORTDXF,
                     "Export &DXF file ...",
                     "Export the current map view to a DXF file");

    fileMenu->AppendSeparator();
    fileMenu->Append(CMD_FILE_PRINTMAP,
                     "&Print map ...",
                     "Print the current map view ");

    fileMenu->AppendSeparator();
    fileMenu->Append(CMD_FILE_CLOSE,
                     "&Exit\tAlt-F4",
                     "Exit snapplot");

    menuBar->Append( fileMenu, "&File" );

    wxMenu *copyMenu = new wxMenu;
    copyMenu->Append( CMD_COPY_MAP,
                      "&Map",
                      "Copy the map to the clipboard");
    copyMenu->Append( CMD_COPY_DETAILS,
                      "&Details",
                      "Copy the details information to the clipboard");
    copyMenu->Append( CMD_COPY_STATIONS,
                      "&Stations",
                      "Copy the stations list to the clipboard");
    copyMenu->Append( CMD_COPY_OBSLIST,
                      "&Observations",
                      "Copy the observations list to the clipboard");
    menuBar->Append( copyMenu, "&Copy" );

    wxMenu *viewMenu = new wxMenu;
    viewMenu->Append( CMD_VIEW_ALL,
                      "&All\tAlt-A",
                      "View entire network");
    viewMenu->Append( CMD_VIEW_ZOOMOUT,
                      "&Zoom out\tAlt-Z",
                      "Zoom out around current view");
    viewMenu->Append( CMD_VIEW_ZOOMPREV,
                      "&Previous view\tAlt-P",
                      "Restore the previous view of the map");
    viewMenu->AppendSeparator();
    viewMenu->Append( CMD_VIEW_STATIONLIST,
                      "View s&tation list\tAlt-T",
                      "View the list of stations in the text window");
    viewMenu->Append( CMD_VIEW_OBSLIST,
                      "View o&bservation list\tAlt-B",
                      "View the list of observations in the text window");
    viewMenu->AppendSeparator();
    viewMenu->Append( CMD_VIEW_ADD_COLOUR,
                      "A&dd colour to palette ...",
                      "Add a colour to the palette of colours available in the key");
    viewMenu->Append( CMD_VIEW_MAPFONT,
                      "Select station &font ...",
                      "Choose the font used for station codes and names on the map");

    menuBar->Append( viewMenu, "&View");

    wxMenu *stationMenu = new wxMenu;
    /*
    stationMenu->Append( CMD_STATION_OPTIONS,
    	              "Display &options ...",
    				  "Options for how stations are shown");
    */
    stationColourMenu = new wxMenu;

    stationColourMenu->Append(CMD_STATION_COLOURBY_USAGE,
                              "&Usage",
                              "Colour stations according to their usage in the adjustment");

    stationColourMenuItem = stationMenu->Append( CMD_STATION_COLOURBY,
                            "&Colour coding",
                            stationColourMenu,
                            "Choose how stations are coloured on the map");
    stationMenu->Append( CMD_STATION_HIGHLIGHT,
                         "&Highlight stations ..",
                         "Choose stations to highlight on the map");
    stationMenu->Append( CMD_STATION_HIDESHOW,
                         "Hi&de stations ..",
                         "Select which stations to hide and which to display on the map");

    ignoreOffsetsItem =
        stationMenu->AppendCheckItem( CMD_STATION_IGNOREOFFSETS,
                                      "&Ignore station offsets",
                                      "Show all stations in their true positions ignoring applied offsets");

    stationMenu->Append( CMD_STATION_FINDSTATION,
                         "&Find station\tCtrl-F",
                         "Find a station");

    menuBar->Append( stationMenu, "&Stations" );

    wxMenu *dataMenu = new wxMenu;
    dataColourMenu = new wxMenu;
    dataColourMenu->AppendCheckItem( CMD_COLOURBY_DATATYPE,
                            "Data &type",
                            "Colour observations according to data type");
    dataColourMenu->AppendCheckItem( CMD_COLOURBY_DATAFILE,
                            "Data &file",
                            "Colour observations according to data type");
    dataColourMenu->AppendCheckItem( CMD_COLOURBY_RESIDUAL,
                            "&Residual ...",
                            "Colour observations according to standardised residual in adjustment");
    dataColourMenu->AppendCheckItem( CMD_COLOURBY_REDUNDANCY,
                            "Re&dundancy ...",
                            "Colour observations according to redundancy factor in adjustment");

    dataMenu->Append( CMD_DATA_COLOURBY,
                      "&Colour coding ...",
                      dataColourMenu,
                      "Choose how observations are coloured coded on the map");
    dataMenu->Append( CMD_DATA_HIGHLIGHT,
                      "&Highlight observations ...",
                      "Choose which observations are highlighted");
    dataMenu->Append( CMD_DATA_OPTIONS,
                      "Display &options ...",
                      "Choose how observations are displayed");

    dataMenu->Append( CMD_DATA_LISTOPTIONS,
                      "L&isting options",
                      "Choose what information is listed for each observation in the grid");

    menuBar->Append( dataMenu, "&Observations");

    wxMenu *errorMenu = new wxMenu;

    if( have_binary_data() )
    {
        errorMenu->Append( CMD_ERROR_OPTIONS,
                           "&Error options ...",
                           "Choose how errors are displayed");
    }

    menuBar->Append( errorMenu, "&Errors");

    configMenu = new wxMenu;
    menuBar->Append( configMenu, "Co&nfiguration");

    displayMenu = new wxMenu;
    wxMenuItem *displaySelectOptionsItem = displayMenu->Append( wxID_ANY, "&Select options..." );
    menuBar->Append( displayMenu, "&Display" );

    Bind( wxEVT_MENU, [this]( wxCommandEvent & )
    {
        wxPopupTransientWindow *popup = new wxPopupTransientWindow( this, wxBORDER_SIMPLE );
        wxBoxSizer *sizer = new wxBoxSizer( wxVERTICAL );

        // One row per Display-by dimension: the fixed trio, then every
        // classification the loaded dataset defines - not hardcoded, same
        // classification_count/name() accessors AddColourByClassifications()
        // already uses for the Colour-by menu.
        struct DisplayByDimension { int id; wxString label; };
        std::vector<DisplayByDimension> dimensions = {
            { DISPLAYBY_DATATYPE, "Data type" },
            { DISPLAYBY_DATAFILE, "Data file" },
            { DISPLAYBY_OBSSTATUS, "Obs status" },
        };
        for( int i = 1; i <= classification_count( &obs_classes ); i++ )
        {
            dimensions.push_back( { i, wxString( classification_name( &obs_classes, i ) ) } );
        }

        std::vector<wxCheckBox *> checkboxes;
        for( const DisplayByDimension &dimension : dimensions )
        {
            const int id = dimension.id;
            wxCheckBox *checkbox = new wxCheckBox( popup, wxID_ANY, dimension.label );
            checkbox->SetValue( is_displayby_enabled( id ) );
            checkbox->Bind( wxEVT_CHECKBOX, [this, id]( wxCommandEvent &event )
            {
                set_displayby_enabled( id, event.IsChecked() );
                rebuild_displayby_symbology();
                SetupSymbology();
            } );
            sizer->Add( checkbox, 0, wxALL, 4 );
            checkboxes.push_back( checkbox );
        }

        wxBoxSizer *buttons = new wxBoxSizer( wxHORIZONTAL );
        wxButton *allButton = new wxButton( popup, wxID_ANY, "All" );
        wxButton *noneButton = new wxButton( popup, wxID_ANY, "None" );
        allButton->Bind( wxEVT_BUTTON, [this, checkboxes, dimensions]( wxCommandEvent & )
        {
            for( size_t i = 0; i < checkboxes.size(); i++ )
            {
                checkboxes[i]->SetValue( true );
                set_displayby_enabled( dimensions[i].id, true );
            }
            rebuild_displayby_symbology();
            SetupSymbology();
        } );
        noneButton->Bind( wxEVT_BUTTON, [this, checkboxes, dimensions]( wxCommandEvent & )
        {
            for( size_t i = 0; i < checkboxes.size(); i++ )
            {
                checkboxes[i]->SetValue( false );
                set_displayby_enabled( dimensions[i].id, false );
            }
            rebuild_displayby_symbology();
            SetupSymbology();
        } );
        buttons->Add( allButton, 0, wxALL, 4 );
        buttons->Add( noneButton, 0, wxALL, 4 );
        sizer->Add( buttons, 0, wxALIGN_CENTRE );
        popup->SetSizerAndFit( sizer );

        // Fixed, deterministic position rather than mouse-based: top at the
        // frame's client area (starts right below the menu bar), left
        // horizontally centred over the map window.
        wxPoint clientTopLeft = ClientToScreen( wxPoint( 0, 0 ) );
        wxRect frameRect = GetScreenRect();
        wxRect mapRect = mapWindow->GetScreenRect();
        wxSize popupSize = sizer->GetMinSize();
        int x = mapRect.GetLeft() + ( mapRect.GetWidth() - popupSize.GetWidth() ) / 2;
        if( x > frameRect.GetRight() - popupSize.GetWidth() ) x = frameRect.GetRight() - popupSize.GetWidth();
        if( x < frameRect.GetLeft() ) x = frameRect.GetLeft();
        popup->Position( wxPoint( x, clientTopLeft.y ), wxSize( 0, 0 ) );
        popup->Popup();
    }, displaySelectOptionsItem->GetId() );

    wxMenu *helpMenu = new wxMenu;
    helpMenu->Append( CMD_HELP_HELP,
                      "&Help\tF1",
                      "Get help about snapplot");
    helpMenu->AppendSeparator();
    helpMenu->Append( CMD_HELP_ABOUT,
                      "&About",
                      "Information about this version snaplot program");

    menuBar->Append( helpMenu, "&Help" );

    SetMenuBar(menuBar);
}

void SnapplotFrame::SetupData()
{
    // if( map ) delete map;
    // if( symbologyKey ) delete symbologyKey;

    if( command_file ) SetLabel(wxString("Snapplot - " )+ wxString(command_file));
    map = new SnapplotMap();
    mapWindow->SetMap( map );
    SetupSymbology();
    AddStationColourOptions();
    AddColourByClassifications();
    UpdateColourByMenuCheck();
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
                                   wxString::Format("Colour stations according to %s",
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

    stationColourMenu->AppendSeparator();
    stationColourMenu->Append( CMD_STATION_COLOURBY_RESETALLON,
                                "Reset all o&n",
                                "Reset every item's checkbox in this list to on" );
    stationColourMenu->Append( CMD_STATION_COLOURBY_RESETALLOFF,
                                "Reset all &off",
                                "Reset every item's checkbox in this list to off" );
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
            dataColourMenu->AppendCheckItem( nextCommandId,
                                    menuText,
                                    wxString::Format("Colour observations according to %s classification",
                                            classification_name( &obs_classes,i) )
                                  );
            Connect( nextCommandId, wxEVT_COMMAND_MENU_SELECTED,
                     wxCommandEventHandler(SnapplotFrame::OnCmdColourBy));

            classifyCommandLast = nextCommandId;
            nextCommandId++;
        }
    }

    dataColourMenu->AppendSeparator();
    dataColourMenu->Append( CMD_DATA_COLOURBY_RESETALLON,
                            "Reset all o&n",
                            "Reset every item's checkbox in this list to on" );
    dataColourMenu->Append( CMD_DATA_COLOURBY_RESETALLOFF,
                            "Reset all &off",
                            "Reset every item's checkbox in this list to off" );
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
                                wxString::Format("Load configuration file for %s",
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

void SnapplotFrame::ReadConfiguration( const char *filename )
{
    process_configuration_file( filename );
    UpdateColourByMenuCheck();
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
    if( cmd_dir ) { cmdDir.Append(cmd_dir); }
    if( cmdDir.IsEmpty() ) { cmdDir.Append("."); }
    wxFileDialog dlgFile(
        this,
        "Select configuration filename for save",
        cmdDir,
        "*" SNAPPLOT_CONFIG_EXT,
        "Snapplot config files (*" SNAPPLOT_CONFIG_EXT ")|*" SNAPPLOT_CONFIG_EXT "|All files (*.*)|*.*",
        wxFD_SAVE | wxFD_OVERWRITE_PROMPT
    );

    if( dlgFile.ShowModal() == wxID_OK &&
            ! save_configuration( dlgFile.GetPath().mb_str()) )
    {
        wxMessageBox(
            wxString::Format( "Unable to save configuration to %s", dlgFile.GetFilename()),
            "Configuration save error",
            wxOK,
            this
        );
    }
}

void SnapplotFrame::OnCmdRestoreConfig( wxCommandEvent & WXUNUSED(event) )
{
    wxString cmdDir;
    if( cmd_dir ) { cmdDir.Append(cmd_dir); }
    if( cmdDir.IsEmpty() ) { cmdDir.Append("."); }
    wxString configFile = wxFileSelector(
                              "Select configuration file to restore",
                              cmdDir,
                              "*" SNAPPLOT_CONFIG_EXT,
                              SNAPPLOT_CONFIG_EXT,
                              "Snapplot config files (*" SNAPPLOT_CONFIG_EXT ")|*" SNAPPLOT_CONFIG_EXT "|All files (*.*)|*.*",
                              wxFD_OPEN | wxFD_FILE_MUST_EXIST,
                              this
                          );

    if( ! configFile.IsEmpty() )
    {
        ReadConfiguration( configFile.mb_str() );
    }
}

void SnapplotFrame::OnCmdExportImage( wxCommandEvent & WXUNUSED(event) )
{

    wxString cmdDir;
    if( cmd_dir ) { cmdDir.Append(cmd_dir); }
    if( cmdDir.IsEmpty() ) { cmdDir.Append("."); }
    wxFileDialog dlgFile(
        this,
        "Image file name",
        cmdDir,
        "*.png",
        "PNG file (*.png)|*.png|Windows bitmap file (*.bmp)|*.bmp",
        wxFD_SAVE | wxFD_OVERWRITE_PROMPT
    );

    if( dlgFile.ShowModal() != wxID_OK ) return;

    wxBitmap mapImage = mapWindow->CreateImage();
    wxImage img = mapImage.ConvertToImage();

    wxBitmapType type;

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
            wxString::Format( "Unable to save image to %s", dlgFile.GetFilename()),
            "Image export error",
            wxOK,
            this
        );
    }
}

void SnapplotFrame::OnCmdExportDxf( wxCommandEvent & WXUNUSED(event) )
{

    wxString cmdDir;
    if( cmd_dir ) { cmdDir.Append(cmd_dir); }
    if( cmdDir.IsEmpty() ) { cmdDir.Append("."); }
    wxFileDialog dlgFile(
        this,
        "DXF file name",
        cmdDir,
        "*.dxf",
        "DXF file (*.dxf)|*.dxf",
        wxFD_SAVE | wxFD_OVERWRITE_PROMPT
    );

    if( dlgFile.ShowModal() != wxID_OK ) return;

    if( open_dxf_file( dlgFile.GetPath().mb_str() ) != OK )
    {
        wxMessageBox(
            wxString::Format( "Unable to save DXF file to %s", dlgFile.GetFilename()),
            "DXF export error",
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
            wxMessageBox("There was a problem printing.\nPerhaps your current printer is not set correctly?", "Printing", wxOK);
        else
            wxMessageBox("Printing cancelled", "Printing", wxOK);
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
    wxColour colour = wxGetColourFromUser( this, none, "New palette colour");
    if( colour.IsOk() )
    {
        Symbology *symbology = GetSnapplotSymbology();
        symbology->GetPalette()->AddColour( colour );
    }
}

void SnapplotFrame::OnCmdViewMapFont( wxCommandEvent & WXUNUSED(event) )
{
    wxFont stationFont;
    stationFont.SetNativeFontInfoUserDesc( get_station_font() );
    wxFont newFont = wxGetFontFromUser( this, stationFont, "New map font style");
    if( newFont.IsOk() )
    {
        set_station_font( newFont.GetNativeFontInfoUserDesc().mb_str() );
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
            // wx already auto-checked this item on click; if the dialog is
            // cancelled, data_pen_type hasn't changed, so the checkmark needs
            // restoring to whichever mode is still actually active.
            if( SetupStandardisedResidualPens( help ) ) SetupDataPens(DPEN_BY_SRES);
            else UpdateColourByMenuCheck();
            break;
        case CMD_COLOURBY_REDUNDANCY:
            // See CMD_COLOURBY_RESIDUAL above.
            if( SetupRedundancyFactorPens( help ) ) SetupDataPens(DPEN_BY_RFAC);
            else UpdateColourByMenuCheck();
            break;
        }
    }
}

// Rebuilds the active list from scratch (defaults, including colours) before
// applying the requested status, so "reset all" also restores default colours.
void SnapplotFrame::ResetDataColourAll( const bool is_on )
{
    invalidate_active_data_user_layer_cache();
    SetupDataPens( get_data_pen_type() );
    reset_data_user_layers( is_on );
    reset_data_type_layer_colours();
    SetupSymbology();
}

void SnapplotFrame::OnCmdDataColourResetAllOn( wxCommandEvent & WXUNUSED(event) )
{
    ResetDataColourAll( true );
}

void SnapplotFrame::OnCmdDataColourResetAllOff( wxCommandEvent & WXUNUSED(event) )
{
    ResetDataColourAll( false );
}

// See ResetDataColourAll - the station equivalent. setup_station_layers() is
// called directly rather than through SetupStationPens()/setup_station_pens(),
// since that guards against redundant rebuilds when the class_id is
// unchanged, which would make it a no-op here.
void SnapplotFrame::ResetStationColourAll( const bool is_on )
{
    invalidate_active_station_class_layer_cache();
    setup_station_layers( get_station_colourby_class() );
    reset_station_user_layers( is_on );
    SetupSymbology();
}

void SnapplotFrame::OnCmdStationColourResetAllOn( wxCommandEvent & WXUNUSED(event) )
{
    ResetStationColourAll( true );
}

void SnapplotFrame::OnCmdStationColourResetAllOff( wxCommandEvent & WXUNUSED(event) )
{
    ResetStationColourAll( false );
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
    if( help ) help->DisplaySection( HELPBASE "index.html");
    wxLaunchDefaultBrowser(helpUrl);
}

void SnapplotFrame::OnCmdHelpAbout( wxCommandEvent & WXUNUSED(event) )
{
    ShowHelpAbout(PROGRAM_NAME,PROGRAM_VERSION,PROGRAM_DATE);
}

void SnapplotFrame::FunctionNotImplemented( wxCommandEvent & WXUNUSED(event) )
{
    wxMessageDialog msgbox( this, "Sorry - this function is not yet implemented",
                            "Warning", wxOK );
    msgbox.ShowModal();
}


void SnapplotFrame::SetupDataPens( int dataPenType )
{
    setup_data_pens( dataPenType );
    UpdateColourByMenuCheck();
    SetupSymbology();
}

void SnapplotFrame::UpdateColourByMenuCheck()
{
    const int type = get_data_pen_type();
    int id;
    switch( type ) {
    case DPEN_BY_TYPE:
        id = CMD_COLOURBY_DATATYPE;
        break;
    case DPEN_BY_FILE:
        id = CMD_COLOURBY_DATAFILE;
        break;
    case DPEN_BY_SRES:
        id = CMD_COLOURBY_RESIDUAL;
        break;
    case DPEN_BY_RFAC:
        id = CMD_COLOURBY_REDUNDANCY;
        break;
    default:
        id = classifyCommandFirst + type - 1;
        break;
    }
    dataColourMenu->Check( CMD_COLOURBY_DATATYPE, id == CMD_COLOURBY_DATATYPE );
    dataColourMenu->Check( CMD_COLOURBY_DATAFILE, id == CMD_COLOURBY_DATAFILE );
    dataColourMenu->Check( CMD_COLOURBY_RESIDUAL, id == CMD_COLOURBY_RESIDUAL );
    dataColourMenu->Check( CMD_COLOURBY_REDUNDANCY, id == CMD_COLOURBY_REDUNDANCY );
    if( classifyCommandLast > 0 ) {
        for( int classifyId = classifyCommandFirst; classifyId <= classifyCommandLast; classifyId++ ) {
            dataColourMenu->Check( classifyId, classifyId == id );
        }
    }
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
