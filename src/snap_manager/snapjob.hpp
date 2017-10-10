#ifndef SNAPJOB_HPP
#define SNAPJOB_HPP

#include "wx_includes.hpp"


enum JobFileType
{
    JFCommandFile,
    JFConfigFile,
    JFCoordinateFile,
    JFDataFile,
};

// Class defines a file referenced in the SNAP job.  Main function is to track files which have changed.
// Includes simplistic support for buildin into a list.

class SnapJobFile
{
public:
    SnapJobFile( const wxString &fileName, JobFileType type, SnapJobFile *sourceFile=nullptr );
    virtual ~SnapJobFile();
    const wxFileName &FileName() { return filename; }

    wxString GetFilename();
    wxString GetFullFilename();
    wxString GetPath();
    JobFileType Type() { return type; }

    bool Exists();
    bool Updated();
    virtual bool Update();

    // Create linked list of files ..
    SnapJobFile *Next() { return next; }
    void SetNext( SnapJobFile *next );
private:
    wxFileName filename;
    wxDateTime lastModified;
    long size;
    JobFileType type;

    SnapJobFile *next;
};

class SnapJob : public SnapJobFile
{
public:
    static wxString SnapUser();
    SnapJob( wxString fileName );
    ~SnapJob();

    wxString Title();
    wxString CoordinateFilename();
    wxString ListingFilename();
    wxString ErrorFilename();
    wxString BinFilename();
    wxString DataFiles();
    wxString RelativeFilename( const wxString &sourceFile );
    wxString LoadErrors();
    wxArrayString &Errors() { return errors; }

    void AddJobFile( SnapJobFile *file );
    void DeleteJobFiles();

    bool IsOk();
    bool Save();
    bool IsSaved();

    virtual bool Update();

private:
    // Function loads commands from the command file, noting
    // command file, data files, etc...

    bool LoadCommands( SnapJobFile *sourceFile, wxArrayString &errors );
    bool Load();

    wxString FileWithExtension( wxString ext );

    wxString title;
    wxArrayString errors;
    bool saved;

    SnapJobFile *crdfile;
    SnapJobFile *jobFiles;
    SnapJobFile *lastFile;

};

#endif
