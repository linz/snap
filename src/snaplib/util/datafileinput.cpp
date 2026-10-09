#include "snapconfig.hpp"

#include <boost/numeric/conversion/cast.hpp>

#include "util/datafileinput.hpp"
#include "util/errdef.h"

using namespace LINZ;
using boost::numeric_cast;

/////////////////////////////////////////////////////////////////////////////
//
// DatafileInput

DatafileInput::DatafileInput( const std::string &filename, const std::string &description ) :
    RecordInputBase( filename),
    _df_own( DATAFILE::open( filename, description ) ),
    _check_progress(nullptr),
    _aborted(false)
{
    if( ! _df_own )
    {
        throw RecordError(std::string("Cannot open ") + description + " " + filename );
    }
    _df = *_df_own;

    setName( _df_own->file_name() );
    _df_own->set_comment( 0 );
    _df_own->set_continuation( 0 );
}

DatafileInput::DatafileInput( DATAFILE &df, bool (*check_progress)( DATAFILE &df ) ) :
    RecordInputBase( df.file_name() ),
    _df( df ),
    _check_progress(check_progress),
    _aborted(false)
{
    setName( df.file_name() );
    df.set_comment( 0 );
    df.set_continuation( 0 );
}

bool DatafileInput::getNextLine( std::string &line )
{
    DATAFILE &df = _df->get();
    if( df.read_record() != OK )
    {
        return false;
    }
    line = df.input_string().scanner.remainder();
    return true;
}


int DatafileInput::lineNumber()
{
    return _df ? numeric_cast<int>( _df->get().line_number() ) : -1;
}

bool DatafileInput::handleError( const RecordError &error )
{
    int status = INVALID_DATA;
    if( error.type() == Warning )
    {
        status = WARNING_ERROR;
    }
    _df->get().error( status, error.message() );
    return true;
}

int DatafileInput::errorCount()
{
    return _df->get().error_count();
}
