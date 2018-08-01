#include "snapconfig.h"
#include "wxsplashimage.hpp"

wxSplashImage::wxSplashImage(wxWindow *parent, const wxBitmap& bitmap)
    : wxWindow(parent, wxID_ANY,
               wxDefaultPosition, wxDefaultSize,
               wxNO_BORDER),
    m_bitmap(bitmap)
{
    wxSize bitmapSize( bitmap.GetWidth(), bitmap.GetHeight() );
    SetMinSize( bitmapSize );
    SetMaxSize( bitmapSize );
}

void wxSplashImage::OnPaint(wxPaintEvent& WXUNUSED(event))
{
    wxPaintDC dc( this );
    dc.DrawBitmap( m_bitmap, 0, 0, false );
}


BEGIN_EVENT_TABLE(wxSplashImage, wxWindow)
    EVT_PAINT(wxSplashImage::OnPaint)
END_EVENT_TABLE()
