#ifndef SNAPPLOT_MAPPRINTER_HPP
#define SNAPPLOT_MAPPRINTER_HPP

#include "wxmap.hpp"
#include "wxmapscale.hpp"
#include "wxsymbology.hpp"

class SnapplotMapPrinter : public wxPrintout {
 public:
    SnapplotMapPrinter(const wxString& title, wxMap* map, wxMapScale* scale, Symbology* symbology);
    ~SnapplotMapPrinter() override;
    //
    bool OnPrintPage(int page) override;
    bool HasPage(int page) override;
    bool OnBeginDocument(int startPage, int endPage) override;
    void GetPageInfo(int* minPage, int* maxPage, int* selPageFrom, int* selPageTo) override;

 private:
    wxMap* sourceMap;
    wxMapScale* sourceScale;
    Symbology* sourceSymbology;
};

#endif
