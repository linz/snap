#ifndef SNAPPLOT_FRAME_HPP
#define SNAPPLOT_FRAME_HPP

#include "wx_includes.hpp"

#include "wxsymbology.hpp"
#include "wxlayerkey.hpp"
#include "wxmapwindow.hpp"
#include "wxmap.hpp"
#include "wxtabbedtextgrid.hpp"

#include "snapplot_detailsview.hpp"
#include "snapplot_stationlist.hpp"
#include "snapplot_obslist.hpp"
#include "snapplot_event.hpp"
#include "snapplot_mapview.hpp"

// Add miscellaneous functions for testing during building ..

class SnapplotFrame: public wxFrame
{
public:
    SnapplotFrame();
    virtual ~SnapplotFrame();

    // Menu commands

    void Quit(wxCommandEvent& event);
    void OnCmdSaveConfig( wxCommandEvent &event );
    void OnCmdRestoreConfig( wxCommandEvent &event );
    void OnCmdExportImage( wxCommandEvent &event );
    void OnCmdExportDxf( wxCommandEvent &event );
    void OnCmdPrintMap( wxCommandEvent &event );
    void OnCmdCopyMap( wxCommandEvent &event );
    void OnCmdCopyDetails( wxCommandEvent &event );
    void OnCmdCopyStations( wxCommandEvent &event );
    void OnCmdCopyObsList( wxCommandEvent &event );
    void OnCmdViewAll( wxCommandEvent &event );
    void OnCmdViewZoomOut( wxCommandEvent &event );
    void OnCmdViewZoomPrev( wxCommandEvent &event );
    void OnCmdViewStationList( wxCommandEvent &event );
    void OnCmdViewObsList( wxCommandEvent &event );
    void OnCmdViewDetails( wxCommandEvent &event );
    void OnCmdViewAddColour( wxCommandEvent &event );
    void OnCmdViewMapFont( wxCommandEvent &event );
    void OnCmdStationHighlight( wxCommandEvent &event );
    void OnCmdStationHideShow( wxCommandEvent &event );
    void OnCmdStationIgnoreOffsets( wxCommandEvent &event );
    void OnCmdStationFindStation( wxCommandEvent &event );
    void OnCmdColourBy( wxCommandEvent &event );
    void OnCmdStationColourBy( wxCommandEvent &event );
    void OnCmdDataOptions( wxCommandEvent &event );
    void OnCmdDataHighlight( wxCommandEvent &event );
    void OnCmdDataListOptions( wxCommandEvent &event );
    void OnCmdErrorOptions( wxCommandEvent &event );
    void OnCmdReadConfig( wxCommandEvent &event );
    void OnCmdHelpHelp( wxCommandEvent &event );
    void OnCmdHelpAbout( wxCommandEvent &event );

    void FunctionNotImplemented( wxCommandEvent &event );

    // Other application events

    void OnMenuOpen( wxMenuEvent &event );

    void OnSymbologyChanged(wxSimpleEvent &event);
    void OnDisplayCoords( wxCommandEvent &event );
    void OnSetDetails( wxSnapplotEvent &event );
    void OnShowDetails( wxSnapplotEvent &event );
    void OnZoomMapview( wxSnapplotEvent &event );
    void OnLocateMapview( wxSnapplotEvent &event );

private:

    void SetupIcons();
    void CreateMenu();
    void CreateMainWindows();
    void SetupHelp();
    void SetupPrinter();
    void SetupData();
    void SetupSymbology();
    void SetupDataPens( int dataPenType );
    void SetupStationPens( int stationPenType );
    void AddColourByClassifications();
    void AddStationColourOptions();
    void AddConfigMenuItems();
    void ReadConfiguration( const char *filename );
    void ShowObsList();
    void ShowStationList();
    void SetErrorTypeStatus();

    wxMap *map;

    // The map
    SnapplotMapView *mapWindow;
    // Window for displaying update information about the map (coordinates)
    wxStaticText *mapInfoWindow;
    // Map layers window
    wxLayerKey *keyWindow;

    // Textual information
    wxNotebook *dataView;
    int stationListPage;
    int obsListPage;
    int detailsPage;
    SnapplotStationList *stationListWindow;
    SnapplotObsList *obsListWindow;
    //wxTextCtrl *detailsWindow;
    SnapplotDetailsView *detailsWindow;

    int nextCommandId;
    int classifyCommandFirst;
    int classifyCommandLast;
    int stationColourCommandFirst;
    int stationColourCommandLast;
    int configMenuCommandFirst;
    int configMenuCommandLast;

    wxMenu *dataColourMenu;
    wxMenuItem *stationColourMenuItem;
    wxMenu *stationColourMenu;
    wxMenu *configMenu;
    wxMenuItem *ignoreOffsetsItem;
    wxHelpController *help;

    wxPrintData *printData;
    wxPageSetupDialogData *pageData;

    bool zoomOnFind;

    DECLARE_EVENT_TABLE()
    DECLARE_NO_COPY_CLASS(SnapplotFrame)
};

#endif
