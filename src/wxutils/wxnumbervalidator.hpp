#ifndef WX_NUMBER_VALIDATOR_HPP
#define WX_NUMBER_VALIDATOR_HPP

#include "wx_includes.hpp"

class wxNumberValidator : public wxTextValidator
{
public:
    wxNumberValidator( int *value, bool positive = true );
    wxNumberValidator( double *value, int nDecPlaces = 0, bool positive = true );
    wxNumberValidator( const wxNumberValidator &validator );

    virtual wxValidator *Clone() const { return new wxNumberValidator(*this); }

    virtual bool TransferFromWindow();
    virtual bool TransferToWindow();
    virtual bool Validate( wxWindow *parent );

private:
    void Setup();
    void *value;
    bool isInteger;
    bool isPositive;
    int nDecPlaces;
};

#endif
