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
    chars.Add(_T("0"));
    chars.Add(_T("1"));
    chars.Add(_T("2"));
    chars.Add(_T("3"));
    chars.Add(_T("4"));
    chars.Add(_T("5"));
    chars.Add(_T("6"));
    chars.Add(_T("7"));
    chars.Add(_T("8"));
    chars.Add(_T("9"));
    if( ! isInteger ) chars.Add(_T("."));
    if( ! isPositive ) chars.Add(_T("-"));
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
            _T("A number is not formatted correctly - please re-enter"),
            _T("Invalid number"),
            wxOK | wxICON_HAND
        );
    }
    return result;
}
