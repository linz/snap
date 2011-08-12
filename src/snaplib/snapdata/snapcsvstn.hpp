#pragma once
#ifndef _SNAP_SNAPCSVSTN_HPP
#define _SNAP_SNAPCSVSTN_HPP

#include "snapdata/snapcsvbase.hpp"

namespace LINZ
{
namespace SNAP
{

using namespace LINZ;
using namespace DelimitedTextFile;
class SnapCsvStn : public SnapCsvBase
{
    class CsvClassification : public CsvValue
    {
    public:
        CsvClassification( const std::string &name );
        int classId() { return _classId; }
        int classValue();
    private:
        int _classId;
    };

    class CsvClassColumn
    {
    public:
        CsvClassColumn( const std::string &classname, const Column *column );
        int classId() { return _classId; }
        const std::string &value() { return _column->value(); }
    private:
        int _classId;
        const Column *_column;
    };
public:
    SnapCsvStn( const std::string &name);
    virtual ~SnapCsvStn();

protected:
    virtual void loadDefinition( const std::string &name );
    virtual void readerAttached();
    virtual void initiallizeLoadData();
    virtual void loadRecord();
    virtual void terminateLoadData();

private:
        void findClassColumns( CalcReader *reader, const std::string &colName );
        void runtimeError( const string &message );
        void dataError( const string &message );
        bool calcVecError( double value[3], double error[6] );
        CsvValue _code;
        CsvValue _name;
        CsvValue _crdlon;
        CsvValue _crdlat;
        CsvValue _crdhgt;
        CsvValue _crdund;
        CsvValue _crdxi;
        CsvValue _crdeta;_
        CsvValue _crdsys;
        std::vector<CsvValue *> _parts;

        // Columns used for classifications
        std::vector<std::unique_ptr<CsvClassification> > _classifications;
        std::vector<std::string> _classColNames;
        std::vector<CsvClassColumn> _classCols;
 
};

} // End of namespace SNAP
} // End of namespace LINZ

#endif
