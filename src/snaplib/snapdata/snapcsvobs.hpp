// #pragma once
#ifndef _SNAP_SNAPCSVOBS_HPP
#define _SNAP_SNAPCSVOBS_HPP

#include "snapdata/datatype.h"
#include "snapdata/snapcsvbase.hpp"

#include "util/parseangle.hpp"
#include "util/snapregex.hpp"
namespace LINZ
{
    namespace SNAP
    {

        using namespace LINZ;
        using namespace DelimitedTextFile;
        using namespace ParseAngle;
        class SnapCsvObs : public SnapCsvBase
        {
            class CsvClassification : public CsvValue
            {
            public:
                CsvClassification(const std::string &name);
                int classId() { return _classId; }
                int classValue();

            private:
                int _classId;
            };

            class CsvClassColumn
            {
            public:
                CsvClassColumn(const std::string &classname, const Column *column);
                int classId() { return _classId; }
                const std::string &value() { return _column->value(); }

            private:
                int _classId;
                const Column *_column;
            };

            enum AngleErrorUnits
            {
                AE_DEFAULT,
                AE_DEGREES,
                AE_SECONDS
            };

            //CsvObs
            class CsvObservation
            {
            public:
                CsvObservation(SnapCsvObs *owner);
                CsvValue &type() { return _type; }
                CsvValue &fromStn() { return _fromstn; }
                CsvValue &toStn() { return _tostn; }
                CsvValue &fromHgt() { return _fromhgt; }
                CsvValue &toHgt() { return _tohgt; }
                CsvValue &value() { return _value; }
                CsvValue &error() { return _error; }
                CsvValue &time() { return _time; }
                CsvValue &errorFactor() { return _errorfactor; }
                CsvValue &obsId() { return _obsid; }
                CsvValue &projection() { return _projection; }
                CsvValue &note() { return _note; }
                CsvValue &setId() { return _setid; }
                CsvValue &rejected() { return _rejected; }
                bool setVectorErrorMethod(const string &format);
                bool setDistanceErrorMethod(const string &format);
                bool setAngleFormat(const string &format);
                bool setAngleErrorUnits(const string &format);
                bool setAngleErrorMethod(const string &format);
                bool setZenDistErrorMethod(const string &format);
                bool setHgtDiffErrorMethod(const string &format);
                bool setDateTimeFormat(const string &format);
                bool setIgnoreMissingObs()
                {
                    _ignoremissingobs = true;
                    return true;
                }
                CsvClassification *classification(const string &classname);
                CsvClassification *addClassification(const string &classname);
                void addColumnClassification(const std::string &colname);

                void attach(CalcReader *reader);

                string missing();
                bool loadObservation();

            private:
                datatypedef *getDataType();
                void findClassColumns(CalcReader *reader, const std::string &colName);
                void definitionError(const string &message);
                void runtimeError(const string &message);
                void dataError(const string &message);
                bool setErrorMethod(const string &method, bool &calced, const string &quantity);
                bool calcVecError(double value[3], double error[6]);
                CsvValue _type;
                CsvValue _fromstn;
                CsvValue _fromhgt;
                CsvValue _tostn;
                CsvValue _tohgt;
                CsvValue _value;
                CsvValue _error;
                CsvValue _time;
                CsvValue _errorfactor;
                CsvValue _obsid;
                CsvValue _projection;
                CsvValue _note;
                CsvValue _setid;
                CsvValue _rejected;
                long _stnidfrom;
                long _stnidto;
                std::vector<CsvValue *> _parts;
                std::vector<std::unique_ptr<CsvClassification>> _classifications;

                // Columns used for classifications
                std::vector<std::string> _classColNames;
                std::vector<CsvClassColumn> _classCols;

                bool _ignoremissingobs;
                bool _disterrorcalced;
                bool _angleerrorcalced;
                bool _zderrorcalced;
                bool _hderrorcalced;
                AngleErrorUnits _angleerrorunits;
                AngleFormat _angleformat;
                int _vecerrorformat;
                int _nvecerror;
                SnapCsvObs *_owner;
                std::string _dateformat;
            };

            friend class CsvObservation;

        public:
            SnapCsvObs(const std::string &name, const OptionString &config = OptionString(""));
            virtual ~SnapCsvObs();

        protected:
            virtual void loadDefinitionCommand(const std::string &command, RecordStream &rs);
            virtual void readerAttached();
            virtual void initiallizeLoadData();
            virtual void loadRecord();
            virtual void terminateLoadData();

        private:
            void loadObservationDefinition(RecordStream &rs, CsvObservation &obs);
            void loadCoefDefinition(RecordStream &rs, CsvObservation &obs, int coef);

            // Information relating to observations sets

            bool InSet() { return _inset; }
            bool ContinueSet(const std::string &setid, const std::string &fromcode);
            void EndSet();

            // Observations
            std::vector<std::unique_ptr<CsvObservation>> _observations;

            // Set information
            bool _inset;
            std::string _setId;
            std::string _setFromCode;
        };

    } // End of namespace SNAP
} // End of namespace LINZ

#endif
