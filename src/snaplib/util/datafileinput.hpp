// #pragma once
#ifndef _SNAP_DATAFILEINPUT_HPP
#define _SNAP_DATAFILEINPUT_HPP

#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>

#include "util/recordinputbase.hpp"
#include "util/datafile.h"

namespace LINZ
{

class DatafileInput : public RecordInputBase
{
public:
    DatafileInput( const std::string &filename, const std::string &description = "data file" );
    DatafileInput( DATAFILE &df, bool (*check_progress)( DATAFILE &df ) = nullptr );
    virtual bool getNextLine( std::string &line );
    // Return true if the error is handled
    virtual bool handleError( const RecordError &error );
    virtual int lineNumber();
    int errorCount();
    bool aborted() { return _aborted; }
private:
    std::unique_ptr<DATAFILE> _df_own;                      ///< the data file, if this class opened it
    std::optional<std::reference_wrapper<DATAFILE>> _df;    ///< the data file being read, set once it is known to be open
    bool (*_check_progress)( DATAFILE &df );
    bool _aborted;
};

} // End of namespace LINZ

#endif
