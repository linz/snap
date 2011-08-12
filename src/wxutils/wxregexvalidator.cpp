#include "wxregexvalidator.hpp"
#include <wx/tokenzr.h>

#include <stdarg.h>

BEGIN_EVENT_TABLE(wxRegexpStringValidator,wxTextValidator)
    EVT_CHAR(OnChar)
END_EVENT_TABLE()

wxRegexpStringValidator::wxRegexpStringValidator( wxString *value, const wxString &rules  )
    : wxTextValidator( wxFILTER_NONE, value )
{
    setCase = 0;
    haveRegex = false;

    wxString validChars;
    if( rules.Len() > 1 )
    {
        wxStringTokenizer tok( rules, rules.Mid(0,1), wxTOKEN_RET_EMPTY );
        tok.GetNextToken();  // Skip initial blank
        if( tok.HasMoreTokens() ) validChars = tok.GetNextToken();
        if( tok.HasMoreTokens() )
        {
            regexstr = tok.GetNextToken();
            if( ! regexstr.IsEmpty())
            {
                haveRegex = true;
                regex.Compile(regexstr,wxRE_ADVANCED );
            }
        }
        if( tok.HasMoreTokens() ) message = tok.GetNextToken();
    }
    if( message.IsEmpty() && haveRegex ) message = wxString("Invalid text entered");
    showMessage = ! message.IsSameAs(wxT("silent"));
    if( ! validChars.IsEmpty() )
    {
        wxArrayString includes;
        for( size_t i = 0; i < validChars.Len(); i++ )
        {
            wxChar ch = validChars.GetChar(i);
            if( ch == '-' && i > 0 && i < validChars.Len()-1)
            {
                wxChar c0 = validChars.GetChar(i-1);
                wxChar c1 = validChars.GetChar(i+1);
                while(c0 < c1)
                {
                    c0++;
                    includes.Add(wxString(c0));
                }
                i++;
                continue;
            }
            includes.Add( wxString(ch) );
        }
        SetIncludes( includes );
        SetStyle( wxFILTER_INCLUDE_CHAR_LIST );
    }
}

wxRegexpStringValidator::wxRegexpStringValidator( const wxRegexpStringValidator &validator )
    : wxTextValidator( validator )
{
    message = validator.message;
    regexstr = validator.regexstr;
    regex.Compile(regexstr,wxRE_ADVANCED );
    setCase = validator.setCase;
    haveRegex = validator.haveRegex;
    showMessage = validator.showMessage;
}

bool wxRegexpStringValidator::Validate( wxWindow * WXUNUSED(parent) )
{
    if( haveRegex )
    {
        wxTextCtrl *txt = static_cast<wxTextCtrl *>( GetWindow() );
        wxString value = txt->GetValue();
        if( ! regex.Matches(value) )
        {
            if( showMessage )
            {
                ::wxMessageBox( message, "Input error", wxICON_EXCLAMATION | wxOK );
            }
            return false;
        }
    }
    return true;
}

void wxRegexpStringValidator::OnChar(wxKeyEvent &event)
{
    event.Skip();
}
