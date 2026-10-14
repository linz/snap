// #pragma once
#ifndef _STNRECODEFILE_HPP
#define _STNRECODEFILE_HPP

#include <string>
#include <vector>
#include "snapdata/snapcsvbase.hpp"
#include "delimitedtextfile.hpp"

struct stn_recode_map;

namespace LINZ {
namespace SNAP {

using namespace LINZ;
using namespace DelimitedTextFile;

class SnapCsvRecode : public SnapCsvBase {
 public:
    SnapCsvRecode(stn_recode_map* stt, const std::string& name);
    ~SnapCsvRecode() override;

 protected:
    void loadDefinitionCommand(const std::string& command, RecordStream& rs) override;
    void readerAttached() override;
    void loadRecord() override;

 private:
    void dataError(const std::string& message);
    CsvValue _codefrom;
    CsvValue _codeto;
    CsvValue _datefrom;
    CsvValue _dateto;
    std::vector<CsvValue*> _parts;

    stn_recode_map* _stt;
    bool _dataError;
};

}  // End of namespace SNAP
}  // End of namespace LINZ

#endif
