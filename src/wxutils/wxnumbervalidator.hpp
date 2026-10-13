#ifndef WX_NUMBER_VALIDATOR_HPP
#define WX_NUMBER_VALIDATOR_HPP

#include "wx_includes.hpp"

class wxNumberValidator : public wxTextValidator {
 public:
    wxNumberValidator(int* value, bool positive = true);
    wxNumberValidator(double* value, int nDecPlaces = 0, bool positive = true);
    wxNumberValidator(const wxNumberValidator& validator);

    [[nodiscard]] wxValidator* Clone() const override {
        return new wxNumberValidator(*this);
    }

    bool TransferFromWindow() override;
    bool TransferToWindow() override;
    bool Validate(wxWindow* parent) override;

 private:
    void Setup();
    void* value;
    bool isInteger;
    bool isPositive;
    int nDecPlaces;
};

#endif
