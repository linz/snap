#include "snapconfig.hpp"

#include <fstream>
#include <sstream>
#include "recordinputbase.hpp"

namespace LINZ
{


//////////////////////////////////////////////////////////////////////
// RecordError class

void RecordError::_setLocation()
{
    if( _filename != "" )
    {
        std::ostringstream location;
        location << "File: " << _filename;
        if( _lineno >= 0 ) location << "  line: " << _lineno;
        if( _recordno >= 0 ) location << "  record: " << _recordno;
        _location = location.str();
    }
}
//////////////////////////////////////////////////////////////////////
// RecordInputBase code

RecordInputBase::~RecordInputBase() {}
bool RecordInputBase::handleError( const RecordError &error ) { return false; }
int  RecordInputBase::lineNumber(){ return -1; }
int  RecordInputBase::recordNumber(){ return -1; }
void RecordInputBase::raiseError( const std::string message )
{
    throw RecordError( message, name(), lineNumber(), recordNumber() );
}

//////////////////////////////////////////////////////////////////////
// IstreamRecordInput code

IstreamRecordInput::IstreamRecordInput( std::istream &is ) : RecordInputBase("unnamed source"), _is(&is), _ownInput(false) {}

IstreamRecordInput::IstreamRecordInput( const std::string &filename ) :
    RecordInputBase(filename),
    _ownInput( false )
{
    _is = new std::ifstream(filename);
    if( ! _is->good())
    {
        delete _is;
        throw RecordError(std::string("Unable to open file ") + filename);
    }
    _ownInput = true;
    _lineno = 0;
}
IstreamRecordInput::~IstreamRecordInput() { if( _ownInput ) delete _is; }

bool IstreamRecordInput::getNextLine( std::string &line )
{
    if( _is->eof()) return false;
    std::getline( *_is, line );
    if( _is->fail()) return false; 
    _lineno++;
    return true;
}

int  IstreamRecordInput::lineNumber(){ return _lineno; }

}  // End of LINZ namespace
