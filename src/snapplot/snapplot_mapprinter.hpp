#ifndef SNAPPLOT_MAPPRINTER_HPP
#define SNAPPLOT_MAPPRINTER_HPP

#include "wxmap.hpp"
#include "wxmapscale.hpp"
#include "wxsymbology.hpp"

class SnapplotMapPrinter : public wxPrintout
{
public:
    SnapplotMapPrinter( const wxString &title, wxMap *map, wxMapScale *scale, Symbology *symbology );
    ~SnapplotMapPrinter();
    //
    bool OnPrintPage(int page);
    bool HasPage(int page);
    bool OnBeginDocument(int startPage, int endPage);
    void GetPageInfo(int *minPage, int *maxPage, int *selPageFrom, int *selPageTo);

private:
    wxMap *sourceMap;
    wxMapScale *sourceScale;
    Symbology *sourceSymbology;

};


#endif
