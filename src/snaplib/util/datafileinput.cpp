#include "snapconfig.hpp"

#include "util/datafileinput.hpp"
#include "util/errdef.h"

using namespace LINZ;

/////////////////////////////////////////////////////////////////////////////
//
// DatafileInput

DatafileInput::DatafileInput( const std::string &filename, const std::string &description ) :
    RecordInputBase( filename),
    _df(df_open_data_file( filename.c_str(), description.c_str() )),
    _check_progress(0),
    _aborted(false)
{
    if( ! _df )
    {
        throw RecordError(std::string("Cannot open ") + description + " " + filename );
    }
    _owner = true;
    df_set_data_file_comment( _df, 0 );
    df_set_data_file_quote( _df, 0 );
    df_set_data_file_continuation( _df, 0 );
}

DatafileInput::DatafileInput( DATAFILE *df, int (*check_progress)( DATAFILE *df ) ) :
    RecordInputBase( df_file_name( df )),
    _df(df),
    _check_progress(check_progress),
    _aborted(false)
{
    _owner = false;
    df_set_data_file_comment( df, 0 );
    df_set_data_file_quote( df, 0 );
    df_set_data_file_continuation( df, 0 );
}

DatafileInput::~DatafileInput()
{
    if( _owner && _df )
    {
        df_close_data_file( _df );
        _df = 0;
    }
}

bool DatafileInput::getNextLine( std::string &line )
{
    if( df_read_data_file(_df ) != OK )
    {
        return false;
    }
    line = df_rest_of_line( _df );
    return true;
}

bool DatafileInput::handleError( const RecordError &error )
{
    int status = INVALID_DATA;
    if( error.type() == Warning )
    {
        status = WARNING_ERROR;
    }
    df_data_file_error( _df, status, error.message().c_str());
    return true;
}

int DatafileInput::errorCount()
{
    return df_data_file_errcount( _df );
}
