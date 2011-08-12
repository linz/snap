#ifndef SNAPPLOT_LOADER_HPP
#define SNAPPLOT_LOADER_HPP
class SnapplotLoadLog : public wxFrame
{
    friend class SnapplotLoadLogger;
public:
    SnapplotLoadLog();
    ~SnapplotLoadLog();
    void StartLogging();
    void EndLogging();

private:
    void OnCloseButton( wxCommandEvent &event );
    void OnClose( wxCloseEvent &event );

    wxTextCtrl *logText;
    wxButton *closeButton;
    bool logging;
    wxLog *textLog;
    wxLog *oldLog;

    DECLARE_EVENT_TABLE()
};

#endif
