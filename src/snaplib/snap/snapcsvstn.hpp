// #pragma once
#ifndef _SNAP_SNAPCSVSTN_HPP
#define _SNAP_SNAPCSVSTN_HPP

#include <memory>
#include <string>
#include <vector>
#include "network/network.h"
#include "snapdata/snapcsvbase.hpp"
#include "util/parseangle.hpp"
#include "calcdltfile.hpp"
#include "delimitedtextfile.hpp"
#include "optionstring.hpp"

struct coordsys;
struct network;

namespace LINZ {
namespace SNAP {

using namespace LINZ;
using namespace DelimitedTextFile;
using namespace ParseAngle;

class SnapCsvStn : public SnapCsvBase {
    class CsvClassification : public CsvValue {
     public:
        CsvClassification(network* net, const std::string& name);
        [[nodiscard]] int classId() const {
            return _classId;
        }
        int classValue();

     private:
        int _classId;
        network* _net;
    };

    class CsvClassColumn {
     public:
        CsvClassColumn(network* net, const std::string& classname, const Column* column);
        [[nodiscard]] int classId() const {
            return _classId;
        }
        const std::string& value() {
            return _column->value();
        }

     private:
        int _classId;
        const Column* _column;
        network* _net;
    };

 public:
    SnapCsvStn(network* net, const std::string& name, const OptionString& config = OptionString(""));
    ~SnapCsvStn() override;

 protected:
    void loadDefinitionCommand(const std::string& command, RecordStream& rs) override;
    void readerAttached() override;
    void initialiseLoadData() override;
    void loadRecord() override;
    void terminateLoadData() override;

 private:
    void findClassColumns(CalcReader* reader, const std::string& colName);
    CsvClassification* classification(const string& classname);
    CsvClassification* addClassification(const string& classname);
    void addColumnClassification(const std::string& colname);
    void dataError(const std::string& message);
    CsvValue _code;
    CsvValue _name;
    CsvValue _crdlon;
    CsvValue _crdlat;
    CsvValue _crdhgt;
    CsvValue _crdund;
    CsvValue _crdxi;
    CsvValue _crdeta;
    CsvValue _crdsys;
    CsvValue _hgttype;
    std::vector<CsvValue*> _parts;

    // Columns used for classifications
    std::vector<std::unique_ptr<CsvClassification>> _classifications;
    std::vector<std::string> _classColNames;
    std::vector<CsvClassColumn> _classCols;

    network* _net;
    bool _haveGeoid;
    bool _geoidDefined;
    bool _deflectionDefined;
    bool _ellipsoidalHeights;
    std::string _cscode;
    std::string _heightType;
    coordsys* _cs;
    bool _projection;
    bool _geocentric;
    int _cstype;
    AngleFormat _angleformat;
    bool _dataError;
};

}  // End of namespace SNAP
}  // End of namespace LINZ

#endif
