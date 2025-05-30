#include "snapconfig.h"
#include "snapjob.hpp"
#include "util/fileutil.h"

// SnapJobFile type

SnapJobFile::SnapJobFile( const wxString &fileName, JobFileType type, SnapJobFile *sourceFile ) :
    filename(fileName ),
    type(type),
    next(0)
{
    filename.MakeAbsolute( sourceFile ? sourceFile->GetPath() : "");
    if( filename.FileExists() )
    {
        lastModified = filename.GetModificationTime();
        size = filename.GetSize().ToULong();
    }
    else
    {
        filename=fileName;
        size = -1;
    }
}

SnapJobFile::~SnapJobFile()
{
}

void SnapJobFile::SetNext( SnapJobFile *next )
{
    this->next = next;
}

bool SnapJobFile::Exists()
{
    return filename.FileExists();
}

bool SnapJobFile::Updated()
{
    bool updated = false;
    if( Exists() )
    {
        updated = lastModified != filename.GetModificationTime() ||
                  size != filename.GetSize();
    }
    else
    {
        updated = size >= 0;
    }
    return updated;
}


bool SnapJobFile::Update()
{
    bool updated = Updated();
    if( updated )
    {
        if( Exists() )
        {

            lastModified = filename.GetModificationTime();
            size = filename.GetSize().ToULong();
        }
        else
        {
            size = -1;
        }
    }
    return updated;
}

wxString SnapJobFile::GetFilename()
{
    return filename.GetFullName();
}

wxString SnapJobFile::GetFullFilename()
{
    return filename.GetFullPath();
}

wxString SnapJobFile::GetPath()
{
    return filename.GetPath( wxPATH_GET_VOLUME );
}

////////////////////////////////////////////////////////////////////////
// SnapJob class

SnapJob::SnapJob( wxString fileName ) :
    SnapJobFile( fileName, JFCommandFile )
{
    // For testing...
    // saved = false;
    saved = true;
    jobFiles = this;
    lastFile = this;
    crdfile = 0;

    Load();

}

SnapJob::~SnapJob()
{
    DeleteJobFiles();
}

wxString SnapJob::SnapUser()
{
    wxString result;
    wxGetEnv("SNAPUSER",&result);
    if( result.IsEmpty() ) wxGetEnv("USERNAME",&result);
    if( result.IsEmpty() ) wxGetEnv("USER",&result);
    return result;
}

void SnapJob::DeleteJobFiles()
{
    SnapJobFile *jf;

    // Dont try and delete the first job file, as it is this file!
    jf = jobFiles->Next();
    while( jf )
    {
        SnapJobFile *next = jf->Next();
        delete jf;
        jf = next;
    }
    lastFile = jobFiles;
    crdfile = 0;
}

void SnapJob::AddJobFile( SnapJobFile *file )
{
    if( file )
    {
        lastFile->SetNext(file);
        lastFile = file;
    }
}

wxString SnapJob::Title()
{
    return title;
}

wxString SnapJob::RelativeFilename( const wxString &sourceFile )
{
    wxFileName f( sourceFile );
    f.MakeRelativeTo( GetPath() );
    return f.GetFullPath();
}

wxString SnapJob::CoordinateFilename()
{
    wxString crdfilestr;
    if( crdfile )
    {
        crdfilestr = RelativeFilename( crdfile->GetFullFilename() );
    }
    return crdfilestr;
}

wxString SnapJob::FileWithExtension( wxString ext )
{
    wxFileName file( FileName() );
    file.SetExt( ext );
    return file.GetFullName();
}

wxString SnapJob::ListingFilename()
{
    return FileWithExtension("lst");
}

wxString SnapJob::ErrorFilename()
{
    return FileWithExtension("err");
}

wxString SnapJob::BinFilename()
{
    return FileWithExtension("bin");
}

wxString SnapJob::DataFiles()
{
    wxString datafiles;
    for( SnapJobFile *f = jobFiles; f; f=f->Next() )
    {
        if( f->Type() != JFDataFile ) continue;
        if( datafiles.Len() > 0 ) datafiles.Append("\n");
        datafiles.Append( RelativeFilename( f->GetFullFilename()));
    }
    return datafiles;
}

wxString SnapJob::LoadErrors()
{
    wxString errorString;
    for( size_t i = 0; i < errors.Count(); i++ )
    {
        if( i > 0 ) errorString.Append("\n");
        errorString.Append( errors.Item(i) );
    }
    return errorString;
}

bool SnapJob::Save()
{
    saved = true;
    return saved;
}

bool SnapJob::IsSaved()
{
    return saved;
}

bool SnapJob::IsOk()
{
    return errors.Count() == 0 ? true : false;
}

bool SnapJob::Load()
{
    DeleteJobFiles();
    errors.Clear();
    return LoadCommands( this, errors  );
}

bool SnapJob::LoadCommands( SnapJobFile *sourceFile, wxArrayString &errors )
{
    size_t errcount = errors.GetCount();
    wxTextFile cmds;
    wxString cmdfile=sourceFile->GetFullFilename();
    bool OpenOk;
    {
        wxLogNull noLog;
        OpenOk = cmds.Open( cmdfile );
    }
    if( OpenOk )
    {
        wxStringTokenizer tok;
        for( size_t i = 0; i < cmds.GetLineCount(); i++ )
        {
            tok.SetString( cmds.GetLine(i) );
            if( tok.HasMoreTokens() )
            {
                wxString cmd = tok.GetNextToken();
                if( cmd.IsSameAs("title",false) )
                {
                    title = tok.GetString().Trim();
                }
                else if( cmd.IsSameAs("coordinate_file",false) )
                {
                    if( crdfile )
                    {
                        // Only the first coordinate file is used for the moment.
                        // Extra files just add stations but are not the principle 
                        // coordinate file.
                        //
                        // This will need better handling
                    }
                    else if( ! tok.HasMoreTokens() )
                    {
                        errors.Add(wxString::Format( "Coordinate file name missing: line %d file %s",
                                                     i, cmdfile));
                    }
                    else
                    {
                        crdfile = new SnapJobFile( tok.GetNextToken(), JFCoordinateFile, sourceFile );
                        AddJobFile( crdfile );
                    }
                }
                else if( cmd.IsSameAs("data_file",false) )
                {
                    if( ! tok.HasMoreTokens() )
                    {
                        errors.Add(wxString::Format( "Data file name missing: line %d file %s",
                                                     i, cmdfile));
                    }
                    else
                    {
                        AddJobFile( new SnapJobFile( tok.GetNextToken(), JFDataFile, sourceFile ) );
                    }
                }
                else if( cmd.IsSameAs("include",false) )
                {
                    if( ! tok.HasMoreTokens() )
                    {
                        errors.Add(wxString::Format( "Included command file name missing: line %d file %s",
                                                     i, cmdfile));
                    }
                    else
                    {
                        SnapJobFile *commandfile = new SnapJobFile( tok.GetNextToken(), JFCommandFile, sourceFile );
                        AddJobFile( commandfile );
                        LoadCommands( commandfile, errors );
                    }
                }
            }

        }
        cmds.Close();
    }
    else
    {
        errors.Add( wxString::Format( "Cannot open command file: %s",cmdfile));
    }

    return errors.GetCount() == errcount;
}

bool SnapJob::Update()
{
    SnapJobFile *jf;
    bool jobUpdated = false;

    for( jf = jobFiles; jf; jf=jf->Next() )
    {
        if( jf->Type() == JFCommandFile && jf->Updated() )
        {
            jobUpdated = true;
            break;
        }
    }

    if( jobUpdated )
    {
        SnapJobFile::Update();
        Load();
    }
    else
    {
        /*

        Atthe moment don't do anything with other types of files, so
        no sense in updating them ...

        for( jf = jobFiles->Next(); jf; jf = jf->Next() )
        {
        	jf->Update();
        }
        */
    }
    return jobUpdated;
}
