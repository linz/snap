#ifndef WXREGEXVALIDATOR_HPP
#define REGEXVALIDATOR_HPP

#include "wx_includes.hpp"

// Constructor takes a string rules, which can define a filter list of characters
// regular expression for validation, and error message.
// These are defined in a delimited string, in which the first character is the
// delimiter, eg
//
// ~1234567890-~^\d\d\d-\d\d\d$~Input must be formated as ddd-ddd
//
// If the message is silent, then it fails silently

class wxRegexpStringValidator : public wxTextValidator
{
public:
    wxRegexpStringValidator( wxString *value, const wxString &rules = wxEmptyString  );
    wxRegexpStringValidator( const wxRegexpStringValidator &validator );

    virtual wxValidator *Clone() const { return new wxRegexpStringValidator( *this ); }
    virtual bool Validate( wxWindow *parent );

private:
    void OnChar(wxKeyEvent &event);
    wxString regexstr;
    wxString message;
    wxRegEx regex;
    bool haveRegex;
    bool showMessage;
    int setCase; // 0 = no, 1 = upper, 2 = lower - not working at present :-(

    DECLARE_EVENT_TABLE()
};

#endif
