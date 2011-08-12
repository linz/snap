#ifndef WXMAINPROGWINDOW_HPP
#define WXMAINPROGWINDOW_HPP

#include "wx_includes.hpp"

#include <time.h>

class wxMainProgWindow : public wxDialog
{
public:
    wxMainProgWindow( const wxString &title, bool hasProgress = true, bool reportTimes = false );
    ~wxMainProgWindow();
    void RunMainProg( int (*mainfunc)( int argc, char *argv[]) );
private:
    void OnCloseButton( wxCommandEvent &event );
    void OnClose( wxCloseEvent &event );

    void AppendMessage( char *message );
    void AppendString( const wxString &string );
    int DoPrintArgs( const char *format, va_list args );
    int DoErrorHandler( int sts, const char *msg1, const char *msg2 );
    void DoInitMeter( long total_size );
    void DoUpdateMeter( long progress );
    void DoEndMeter();

    static int PrintArgs( const char *format, va_list args );
    static int ErrorHandler( int sts, const char *msg1, const char *msg2 );
    static void InitMeter( long total_size );
    static void UpdateMeter( long progress );
    static void EndMeter();

    wxTextCtrl *logText;
    wxStaticText *progressText;
    wxGauge *progressGauge;
    wxButton *closeButton;

    wxString lastLine;
    char *buffer;
    int buflen;
    bool running;
    bool hasProgress;
    bool reportTimes;
    clock_t lastUpdate;
    clock_t startTime;
    long nextProgressUpdate;
    long maxUnreportedProgress;

    static wxMainProgWindow *instance;

    DECLARE_EVENT_TABLE()

};



#endif