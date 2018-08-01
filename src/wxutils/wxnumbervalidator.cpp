#include "snapconfig.h"
#include "wxnumbervalidator.hpp"

wxNumberValidator::wxNumberValidator( int *value, bool positive )
    : wxTextValidator( wxFILTER_INCLUDE_CHAR_LIST, NULL ),
      value(value)
{
    isInteger = true;
    isPositive = positive;

    Setup();
}

wxNumberValidator::wxNumberValidator( double *value, int nDecPlaces, bool positive )
    : wxTextValidator( wxFILTER_INCLUDE_CHAR_LIST, NULL ),
      value(value),
      nDecPlaces(nDecPlaces)
{
    isInteger = false;
    isPositive = positive;
    Setup();
}

void wxNumberValidator::Setup()
{
    wxArrayString chars;
    chars.Add("0");
    chars.Add("1");
    chars.Add("2");
    chars.Add("3");
    chars.Add("4");
    chars.Add("5");
    chars.Add("6");
    chars.Add("7");
    chars.Add("8");
    chars.Add("9");
    if( ! isInteger ) chars.Add(".");
    if( ! isPositive ) chars.Add("-");
    SetIncludes(chars);
}


wxNumberValidator::wxNumberValidator( const wxNumberValidator &validator ) :
    wxTextValidator( validator ),
    value( validator.value ),
    isInteger( validator.isInteger ),
    isPositive( validator.isPositive ),
    nDecPlaces( validator.nDecPlaces )
{
}

bool wxNumberValidator::TransferFromWindow()
{
    wxTextCtrl *ctrl = static_cast<wxTextCtrl *>(GetWindow());
    wxString strval = ctrl->GetValue();
    bool result;
    if( isInteger )
    {
        long lvalue;
        result = strval.ToLong( &lvalue );
        if( result && isPositive && lvalue < 0 ) result = false;
        if( result ) * (int *) value = (int) lvalue;
    }
    else
    {
        double dvalue;
        result = strval.ToDouble( &dvalue );
        if( result && isPositive && dvalue < 0 ) result = false;
        if( result ) * (double *) value = dvalue;
    }
    return result;
}

bool wxNumberValidator::TransferToWindow()
{
    wxTextCtrl *ctrl = static_cast<wxTextCtrl *>(GetWindow());
    if( isInteger )
    {
        ctrl->SetValue( wxString::Format("%d",* (int *) value ));
    }
    else
    {
        ctrl->SetValue( wxString::Format("%.*lf",nDecPlaces,* (double *) value ));
    }
    return true;
}

bool wxNumberValidator::Validate( wxWindow * WXUNUSED(parent) )
{
    double tempval;
    void *saveptr = value;
    value = &tempval;
    bool result = TransferFromWindow();
    value = saveptr;
    if( ! result )
    {
        wxMessageBox(
            "A number is not formatted correctly - please re-enter",
            "Invalid number",
            wxOK | wxICON_HAND
        );
    }
    return result;
}
