#ifndef SNAPPLOT_APP_HPP
#define SNAPPLOT_APP_HPP

#include "wx_includes.hpp"

class SnapplotApp : public wxApp {
 public:
    SnapplotApp() = default;

    bool OnInit() override;
    int OnExit() override;

    DECLARE_NO_COPY_CLASS(SnapplotApp)
};

#endif