#ifndef WXSPLASHIMAGE_HPP
#define WXSPLASHIMAGE_HPP

#include "wx_includes.hpp"

class wxSplashImage : public wxWindow
{
public:
    wxSplashImage(wxWindow *parent, const wxBitmap& bitmap);
    void OnPaint(wxPaintEvent& WXUNUSED(event));

private:
    wxBitmap m_bitmap;
    DECLARE_EVENT_TABLE()
};

#endif
