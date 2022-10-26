#include "wx_includes.hpp"
#include "wxmapwindow.hpp"
#include "wxsymbology.hpp"
#include "wxsimpledialog.hpp"
#include "wxtabbedtextgrid.hpp"
#include "testmap.hpp"

class MyDialog : public wxSimpleDialog
{
public:
	MyDialog();
protected:
	virtual void ReadData();
	virtual void Apply();
private:
	void ButtonFunc( wxCommandEvent &event );
	int i;

};

MyDialog::MyDialog() : wxSimpleDialog("My test dialog",0)
{
	AddRadioBox( "Options",i,"~Option 1~Option 2~Last option");
	wxBoxSizer *sizer = new wxBoxSizer( wxHORIZONTAL );
	wxButton *highlightButton = Button(_T("Highlight"),wxCommandEventHandler( MyDialog::ButtonFunc ));
	sizer->Add(highlightButton);
	sizer->Add( new wxButton( this, wxID_CANCEL, "Done") );
	sizer->Add( new wxButton( this, wxID_HELP ) );\
	AddSizer( sizer );
	highlightButton->SetDefault();
	AddButtonsAndSize();
}

void MyDialog::Apply()
{
	::wxMessageBox(_T("Apply called"),_T("Apply"));
}

void MyDialog::ReadData()
{
}

void MyDialog::ButtonFunc( wxCommandEvent &event )
{
	int id = event.GetId();
	::wxMessageBox(wxString::Format(_T("Button %d pressed"),id),_T("Button func"));
}

class MyFrame;

class MyProcess : public wxProcess
{
public:
	MyProcess( MyFrame *myFrame );
	virtual void OnTerminate( int pid, int status );
private:
	MyFrame *frame;

};

class MyApp: public wxApp
{
    virtual bool OnInit();
};

class TestSource : public wxTabbedTextSource
{
	const char *header="row\tname\tcount";
	const char *data[3]={
		"row1\tvalue1\tFred",
		"row2\tvalue2\tFrodo Baggins",
		"row3\tvalue3\tKermit the frog"
	};
	int const rowcount=3;

public:
	TestSource(){};
	virtual ~TestSource(){};
	virtual char *GetHeader(){ return (char *) TestSource::header;}
	virtual int GetRowCount(){ return rowcount; }
	virtual char *GetRow( int i ){ return (char *) (TestSource::data[i]); }
};

class MyFrame: public wxFrame
{
public:
    MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size);

    void OnQuit(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);
    void OnTest(wxCommandEvent& event);
	void OnTestDialog( wxCommandEvent &event );
    void OnZoomAll(wxCommandEvent& event);
	void GridRowSelected( wxCommandEvent &event );

	void BringToFront();

private:
    DECLARE_EVENT_TABLE()
	TestMap map;
	Symbology sym;
	wxMapScaleDragger dragger;
	wxMapWindow *main;
	wxLogWindow *logger;
	wxMenuItem *iconize;
	wxMenuItem *show;
	wxMenuItem *raise;
	wxMenuItem *synchronous;
	wxTabbedTextGrid *grid;
	TestSource *gridSource;

};

enum
{
	ID_Test = 1,
	ID_TestDialog,
	ID_ZoomAll,
	ID_DeIconize,
	ID_Show,
	ID_Raise,
	ID_Synchronous,
    ID_Quit,
    ID_About,
};

BEGIN_EVENT_TABLE(MyFrame, wxFrame)
    EVT_MENU(ID_Quit,  MyFrame::OnQuit)
    EVT_MENU(ID_About, MyFrame::OnAbout)
    EVT_MENU(ID_Test, MyFrame::OnTest)
    EVT_MENU(ID_TestDialog, MyFrame::OnTestDialog)
	EVT_MENU(ID_ZoomAll, MyFrame::OnZoomAll )
    EVT_COMMAND( wxID_ANY, WX_TTGRID_ROW_SELECTED, MyFrame::GridRowSelected )
END_EVENT_TABLE()

IMPLEMENT_APP(MyApp)

bool MyApp::OnInit()
{
    MyFrame *frame = new MyFrame( _T("Hello World"), wxPoint(50,50), wxSize(450,340) );
    frame->Show( true );
    SetTopWindow( frame );
    return true;
}


MyFrame::MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size)
       : wxFrame((wxFrame *)NULL, -1, title, pos, size)
{
	logger = new wxLogWindow( this, "logger", true, false );

    wxMenu *menuFile = new wxMenu;

    menuFile->Append( ID_About, _T("&About...") );
    menuFile->Append( ID_Test, _T("&Test...") );
    menuFile->Append( ID_TestDialog, _T("&Dialog...") );
    menuFile->Append( ID_ZoomAll, _T("&Zoom all...") );
    menuFile->AppendSeparator();
	iconize = menuFile->AppendCheckItem( ID_DeIconize,"DeIconize");
	iconize->Check();
	show = menuFile->AppendCheckItem( ID_Show,"Show");
	show->Check();
	raise = menuFile->AppendCheckItem( ID_Raise, "Raise" );
	raise->Check();
	synchronous = menuFile->AppendCheckItem( ID_Synchronous, "Synchronous" );
	synchronous->Check(false);

    menuFile->AppendSeparator();
    menuFile->Append( ID_Quit, _T("E&xit") );

    wxMenuBar *menuBar = new wxMenuBar;
    menuBar->Append( menuFile, _T("&File") );

    SetMenuBar( menuBar );

    CreateStatusBar();
    SetStatusText( _T("Welcome to wxWidgets!") );

	wxSplitterWindow *divider=new wxSplitterWindow(this);

	main = new wxMapWindow( divider, wxID_ANY );
	main->SetMap( &map );
	map.GetLayer( sym );
	main->SetSymbology( &sym );
	// main->SetDragger( &dragger );

	grid=new wxTabbedTextGrid(divider,wxID_ANY);
	gridSource=new TestSource();
	grid->SetTabbedTextSource(gridSource);

	divider->SplitHorizontally(main,grid);
	
	


	wxLogMessage("Construction complete");


}

void MyFrame::OnQuit(wxCommandEvent& WXUNUSED(event))
{
    Close( true );
}

void MyFrame::OnAbout(wxCommandEvent& WXUNUSED(event))
{
    wxMessageBox( _T("This is a wxWidgets' Hello world sample"),
                  _T("About Hello World"), wxOK | wxICON_INFORMATION );
}

void MyFrame::OnZoomAll(wxCommandEvent& WXUNUSED(event))
{
	main->GetScale().ZoomAll();
}

void MyFrame::OnTest(wxCommandEvent& WXUNUSED(event))
{
	if( synchronous->IsChecked() )
	{
		wxExecute( "c:\\windows\\notepad.exe", wxEXEC_SYNC );
		BringToFront();
	}
	else
	{
		wxProcess *p = new MyProcess( this );
		int status = wxExecute( "c:\\windows\\notepad.exe", wxEXEC_ASYNC, p );
		if( ! status ) delete p;
	}

}

void MyFrame::OnTestDialog( wxCommandEvent & WXUNUSED(event))
{
	MyDialog dlg;
	dlg.RunDialog();
}

void MyFrame::BringToFront( void )
{
	if( iconize->IsChecked()) 
	{
		wxLogMessage("Restore Icon");
		if( IsIconized() ) Iconize( false ); 
	}

	if( show->IsChecked()) 
	{
		wxLogMessage("Showing");
		Show();
	}

	if( raise->IsChecked()) 
	{
		wxLogMessage("Raising");
		Raise();
	}
}

void MyFrame::GridRowSelected( wxCommandEvent &event )
{
	int row = event.GetInt();
	wxLogMessage("Rows %d selected",row);
}


MyProcess::MyProcess( MyFrame *myFrame )
{
	frame = myFrame;
}

void MyProcess::OnTerminate( int pid, int status )
{
	wxLogMessage(_T("On terminate called: pid %d status %d"),pid,status);

	frame->BringToFront();
	
	delete this;
}
