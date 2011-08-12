#include "wxmainprogwindow.hpp"

extern "C" {
#include "util/progress.h"
#include "util/xprintf.h"
#include "util/errdef.h"
#include "util/xprintf.h"
}


BEGIN_EVENT_TABLE( wxMainProgWindow, wxDialog )
EVT_BUTTON( wxID_CLOSE, wxMainProgWindow::OnCloseButton )
EVT_TEXT_ENTER(wxID_ANY, wxMainProgWindow::OnCloseButton)
EVT_CLOSE( wxMainProgWindow::OnClose )
END_EVENT_TABLE()

wxMainProgWindow::wxMainProgWindow( const wxString &title, bool hasProgress, bool reportTimes ) :
wxDialog( 0, wxID_ANY, title, wxDefaultPosition, wxDefaultSize, 
		 wxCAPTION | wxRESIZE_BORDER | wxSYSTEM_MENU | wxCLOSE_BOX ),
hasProgress( hasProgress ),
reportTimes ( reportTimes )
{
	
	// TODO: Fail somehow if already an instance running

	if( instance ) { return; } 

	wxIconBundle icons;
	icons.AddIcon( wxIcon(wxICON(ICO_SNAP16)) );
	icons.AddIcon( wxIcon(wxICON(ICO_SNAP32)) );
	SetIcons( icons );

    wxSizer *sizer = new wxBoxSizer( wxVERTICAL );
	wxSizerFlags flags;
	flags.Expand().Border();

	logText = new wxTextCtrl( this, wxID_ANY, _T(""), 
		    wxDefaultPosition, 
			wxSize(GetCharWidth()*85, GetCharHeight()*25),
            wxTE_MULTILINE  |
            wxHSCROLL       |
            // needed for Win32 to avoid 65Kb limit but it doesn't work well
            // when using RichEdit 2.0 which we always do in the Unicode build
#if !wxUSE_UNICODE
            wxTE_RICH       |
#endif // !wxUSE_UNICODE
            wxTE_READONLY |
			wxTE_PROCESS_ENTER );

	wxSizerFlags flags2;
	flags2.Expand().Border().Proportion(1);
	sizer->Add( logText, flags2 );

	progressText = 0;
	progressGauge = 0;

	if( hasProgress )
	{
		sizer->AddSpacer( GetCharHeight() );

		progressText = 	new wxStaticText( this, wxID_ANY, _T(""), wxDefaultPosition, wxDefaultSize );
		sizer->Add( progressText, flags );

		progressGauge = new wxGauge( this, wxID_ANY, 100, wxDefaultPosition, wxDefaultSize, wxGA_HORIZONTAL | wxGA_SMOOTH );
		sizer->Add( progressGauge, flags);
	}

	sizer->AddSpacer( GetCharHeight() );
 	wxSizerFlags flags3;
	flags3.Border().Right();
	closeButton = new wxButton( this, wxID_CLOSE );
	closeButton->SetDefault();
	sizer->Add( closeButton, flags3 );

	SetSizerAndFit( sizer );


	running = false;
	buffer = 0;
	buflen = 0;
	instance = this;

	lastUpdate = clock();

	set_printf_target( PrintArgs );

	progress_meter_def meter;
	meter.init_meter = InitMeter;
	meter.update_meter = UpdateMeter;
	meter.end_meter = EndMeter;
	install_progress_meter( &meter );

	set_error_handler( ErrorHandler );

}

wxMainProgWindow::~wxMainProgWindow()
{
	set_error_handler( default_error_handler );
	
	uninstall_progress_meter();

	set_printf_target( 0 );

	if( buffer ) delete [] buffer;
	buffer = 0;
	buflen = 0;
	instance = 0;
}

void wxMainProgWindow::OnCloseButton( wxCommandEvent & WXUNUSED(event) )
{
	if( ! running ) Close();
}

void wxMainProgWindow::OnClose( wxCloseEvent &event )
{
	if( running && event.CanVeto() )
	{
		event.Veto();
		return;
	}
	else
	{
		Destroy();
	}
}

void wxMainProgWindow::RunMainProg( int (*mainfunc)( int argc, char *argv[] ))
{
	closeButton->Enable( false );
	running = true;

	(*mainfunc)( wxTheApp->argc, wxTheApp->argv );

	running = false;
	closeButton->Enable( true );
	// closeButton->SetDefault();
}

void wxMainProgWindow::AppendMessage( char *message )
{
	AppendString( wxString(_T(message)));
}

void wxMainProgWindow::AppendString( const wxString &text )
{

    // remove selection (WriteText is in fact ReplaceSelection)
#ifdef __WXMSW__
    wxTextPos nLen = logText->GetLastPosition();
    logText->SetSelection(nLen, nLen);
#endif // Windows

    logText->AppendText(text);

	Update();
	lastUpdate = clock();

	// Keep track of the last non-blank line as the header for the progress meter
	wxStringTokenizer tokens(text,_T("\n"));
	while( tokens.HasMoreTokens())
	{
		wxString line = tokens.GetNextToken();
		line.Trim();
		if( ! line.IsEmpty() ) lastLine = line;
	}
}

int wxMainProgWindow::DoPrintArgs( const char *format, va_list args )
{
   int len;

   len = _vscprintf( format, args ) + 1;
   if( buflen < len )
   {
	   if( buffer ){ delete [] buffer; buffer = 0; }
	   buflen = 1024;
	   while( buflen < len ) buflen += 1024;
	   buffer = new char[buflen];
   }

   if( buffer ) 
   {
      vsprintf_s( buffer, len, format, args );
	  AppendMessage( buffer );
   }
   return len;

}

short wxMainProgWindow::DoErrorHandler( short sts, char *msg1, char *msg2 )
{
	char *blank = "";
	wxString text = wxString::Format("%s %s\n", msg1 ? msg1 : blank, msg2 ? msg2 : blank );
	AppendString( text );
	return sts;
}

int wxMainProgWindow::PrintArgs( const char *format, va_list args )
{
	if( instance ) return instance->DoPrintArgs( format, args );
	return 0;
}

short wxMainProgWindow::ErrorHandler( short sts, char *msg1, char *msg2 )
{
	if( instance ) return instance->DoErrorHandler( sts, msg1, msg2 );
	return 0;
}

void wxMainProgWindow::DoInitMeter( long total_size )
{
	if( hasProgress )
	{
		progressText->SetLabel( lastLine.Trim(false) );
		progressText->Update();
		progressGauge->SetRange( total_size );
		startTime = clock();
		maxUnreportedProgress = total_size/100;
		nextProgressUpdate = maxUnreportedProgress;
	}
}

void wxMainProgWindow::DoUpdateMeter( long progress )
{
	if( hasProgress && progress > nextProgressUpdate )
	{
		progressGauge->SetValue( progress );
		nextProgressUpdate = progress + maxUnreportedProgress;

	}
	if( clock() - lastUpdate > CLOCKS_PER_SEC )
	{
		Update();
		lastUpdate = clock();
	}
}

void wxMainProgWindow::DoEndMeter()
{
	if( hasProgress )
	{
		progressText->SetLabel(_T(""));
		progressGauge->SetValue(0);
		if( reportTimes )
		{
			double time = (double)(clock()-startTime) / CLOCKS_PER_SEC;
			AppendString( wxString::Format("     ... duration %.2lf seconds\n",time) );
		}
	}
}

void wxMainProgWindow::InitMeter( long total_size )
{
	if( instance ) instance->DoInitMeter( total_size );
}

void wxMainProgWindow::UpdateMeter( long progress )
{
	if( instance ) instance->DoUpdateMeter( progress );
}

void wxMainProgWindow::EndMeter()
{
	if( instance ) instance->DoEndMeter();
}

wxMainProgWindow *wxMainProgWindow::instance = 0;
