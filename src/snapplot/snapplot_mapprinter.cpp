#include "snapconfig.h"
#include "wx_includes.hpp"
#include "snapplot_mapprinter.hpp"

//extern "C"
//{
#include "snap/snapglob.h"
//}

SnapplotMapPrinter::SnapplotMapPrinter( const wxString &title, wxMap *map, wxMapScale *scale, Symbology *symbology ) :
    wxPrintout(title), sourceMap(map), sourceScale(scale), sourceSymbology( symbology )
{
}

SnapplotMapPrinter::~SnapplotMapPrinter()
{
}

void SnapplotMapPrinter::GetPageInfo(int *minPage, int *maxPage, int *selPageFrom, int *selPageTo)
{
    *minPage = 1;
    *maxPage = 1;
    *selPageFrom = 1;
    *selPageTo = 1;
}

bool SnapplotMapPrinter::HasPage(int page)
{
    return page == 1;
}

bool SnapplotMapPrinter::OnBeginDocument(int startPage, int endPage)
{
    if (!wxPrintout::OnBeginDocument(startPage, endPage))
        return false;

    return true;
}

bool SnapplotMapPrinter::OnPrintPage(int WXUNUSED(page) )
{
    wxDC *dc = GetDC();
    if( ! dc ) return false;

    // Lots of queries here trying to make scaling of fonts work ..

    int w, h;
    wxRect printRect = GetLogicalPageRect();

    // Just setting a point size didn't work very well :-(
    // This seems consistent, though also seems illogical.  Gives something a
    // little bigger than 12 point though..

    int titleFontSize = (int) ((dc->GetPPI().GetWidth()*12)/72);
    wxFont titleFont( *wxSWISS_FONT );
    titleFont.SetPointSize( titleFontSize );
    dc->SetFont( titleFont );

    wxRect plotRect = sourceScale->GetWindowExtents();

    dc->GetTextExtent( job_title, &w, &h );
    dc->DrawText( job_title, (printRect.GetWidth() - w)/2, 0 );

    int topMargin = dc->GetCharHeight() * 2;
    printRect.SetHeight( printRect.GetHeight() - topMargin );
    printRect.SetTop( printRect.GetTop() + topMargin );

    // Calculate the scaling ..

    double scale = ((double) printRect.GetWidth())/((double) plotRect.GetWidth());
    double scale2 = ((double) printRect.GetHeight())/((double) plotRect.GetHeight());
    if( scale2 < scale )
    {
        scale2 /= scale;
        int width = (int) (printRect.GetWidth() * scale2 );
        printRect.SetLeft( printRect.GetLeft() + (printRect.GetWidth()-width)/2 );
        printRect.SetWidth( width );
    }
    else
    {
        scale /= scale2;
        int height = (int) (printRect.GetHeight() * scale );
        printRect.SetHeight( height );
    }

    dc->SetClippingRegion( printRect );

    wxMapScale printScale( *sourceScale );
    printScale.SetWindowExtents( printRect );
    wxMapDrawer drawer( *dc, printScale, *sourceSymbology );
    sourceMap->DrawMap( drawer );

    dc->DestroyClippingRegion();

    dc->SetPen( wxPen( *wxBLACK ) );
    dc->SetBrush( wxBrush( *wxBLACK, wxTRANSPARENT ));
    dc->DrawRectangle( printRect );

    return true;

}
