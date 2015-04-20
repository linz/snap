// #pragma once
#ifndef _SNAP_DATAFILEINPUT_HPP
#define _SNAP_DATAFILEINPUT_HPP

#include <iostream>
#include <sstream>
#include <string>

#include "util/recordinputbase.hpp"
#include "util/datafile.h"

namespace LINZ
{

using namespace LINZ;

class DatafileInput : public RecordInputBase
{
public:
    DatafileInput( const std::string &filename, const std::string &description = "data file" );
    DatafileInput( DATAFILE *df, int (*check_progress)( DATAFILE *df ) = 0 );
    ~DatafileInput();
    virtual bool getNextLine( std::string &line );
    // Return true if the error is handled
    virtual bool handleError( const RecordError &error );
    int errorCount();
    bool aborted() { return _aborted; }
private:
    DATAFILE *_df;
    bool _owner;
    int (*_check_progress)( DATAFILE *df );
    bool _aborted;
};

} // End of namespace LINZ

#endif
