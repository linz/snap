#include "snapconfig.h"
#include "snapplot_util.hpp"

// print_log captures output intended for command line user ..

#include "snapplot_util.hpp"
#include <stdio.h>
#include <stdarg.h>

//extern "C" {
#include "snapplot_util.h"
#include "util/errdef.h"
//}

class wxStringLog : public wxLog
{
public:
    wxStringLog();
    ~wxStringLog() {}
    const wxString &GetMessages() { return logString; }
    void ClearMessages() { logString.Empty(); }

protected:
    virtual void DoLogText( const wxString &msg );

private:
    wxString logString;
};

static wxStringLog *logger = 0;

wxStringLog::wxStringLog()
{
    logString.Alloc(2048);
}

void wxStringLog::DoLogText( const wxString &msg )
{
    logString.Append( msg );
}

int print_log(const char *format, ... )
{
    va_list args;
    va_start( args, format );
    return print_log_args( format, args );
}

// NOTE: Not threadsafe ... designed for efficiency ...

int print_log_args( const char *format, va_list args )
{
    int len;
    static char * buffer = 0;
    static int buflen = 0;
    va_list copy;

    va_copy(copy,args);

    if( ! buffer )
    {
        buflen=1024;
        buffer=new char[buflen];
    }
    len = vsnprintf( buffer, buflen, format, args );
    if( buflen < len+1 )
    {
        if( buffer ) { delete [] buffer; buffer = 0; }
        buflen = 1024;
        while( buflen < len+1 ) buflen += 1024;
        buffer = new char[buflen];
    }

    if( buffer )
    {
        len=vsnprintf( buffer, buflen, format, copy );
        wxLogMessage("%s",buffer);
    }
    va_end(copy);
    return len;
}

static int error_handler( const int sts, const std::string_view msg1, const error_message msg2 )
{
    const wxString text1{ std::string( msg1 ) };
    const wxString text2( msg2 ? std::string( *msg2 ) : std::string() );
    if( FATAL_ERROR_CONDITION(sts) )
    {
        // For fatal errors run wxMessageBox directly so that user sees it before process
        // is aborted.  Don't use wxLogError, as this calls abort(), which fires up Windows
        // error handler ... too severe for what I'm calling a fatal error!

        wxMessageBox( wxString::Format("%s %s\nAborting.", text1, text2 ),
                      "Error", wxOK | wxICON_EXCLAMATION );
        // wxLogError( "%s %s\n", text1, text2 );
    }
    else if ( WARNING_ERROR_CONDITION(sts) )
    {
        wxLogWarning( "%s %s\n", text1, text2 );
        // wxLogWarning's target (see ImplementErrorHandler below) only buffers
        // messages in memory, with nothing in the UI that ever displays them -
        // so a caller that wants this specific warning seen must ask for it
        // explicitly via SHOW_DIALOG, rather than every warning popping up
        // unprompted.
        if( SHOW_DIALOG_CONDITION(sts) )
        {
            wxMessageBox( wxString::Format("%s %s", text1, text2 ),
                          "Warning", wxOK | wxICON_EXCLAMATION );
        }
    }
    else
    {
        wxLogMessage("%s %s\n", text1, text2 );
    }
    return sts;
}

void ImplementErrorHandler()
{
    if( ! logger ) logger = new wxStringLog();
    wxLog::SetActiveTarget( logger );
    set_error_handler( error_handler );
}

wxString GetLogMessages()
{
    wxString messages;
    if( logger )
    {
        messages = logger->GetMessages();
        logger->ClearMessages();
    }
    return messages;
}
