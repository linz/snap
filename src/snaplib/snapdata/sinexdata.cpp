
#include "snapconfig.hpp"

#include <iostream>
#include <sstream>
#include <memory>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <boost/algorithm/string.hpp>

#include "util/snapregex.hpp"
#include "util/recordinputbase.hpp"
#include "util/datafileinput.hpp"
#include "util/optionstring.hpp"

#include "snapdata/datatype.h"
#include "snapdata/loaddata.h"

#include "util/dateutil.h"
#include "util/fileutil.h"
#include "util/datafile.h"
#include "util/pi.h"
#include "util/calcdltfile.hpp"
#include "util/recordstream.hpp"
#include "util/errdef.h"

namespace LINZ
{
namespace SNAP
{

// SinexDataReader

class SinexDataReader
{
public:
    SinexDataReader(const OptionString &config = OptionString(""));
    ~SinexDataReader();
    void loadObservations(RecordInputBase &dfi);

private:
    double readSinexDate(const std::string datestr);
    bool findSection(RecordInputBase &dfi, const std::string section, bool raiseError = true);
    int findSection(RecordInputBase &dfi, const std::vector<std::string> &sections, bool raiseError = true);
    std::string ref_frame;
    bool use_point_code;
    double obsdate;
    struct PointData
    {
        int id;
        int lineno;
        std::string code;
        std::string soln;
        double xyz[3];
    };
};

SinexDataReader::SinexDataReader(const OptionString &config)
{
    ref_frame = std::string(config.valueOf("ref_frame", "GNSS"));
    std::string code = config.valueOf("code", "SITE");
    boost::to_upper(code);
    use_point_code = code == "POINT";
    std::string datestr = config.valueOf("name", "");
    obsdate = UNDEFINED_DATE;
    if (datestr != "")
    {
        obsdate = snap_datetime_parse(datestr.c_str(), 0);
        if (obsdate == UNDEFINED_DATE)
        {
            throw RecordError(std::string("Invalid date ") + datestr + " specified for SINEX file");
        }
    }
}

SinexDataReader::~SinexDataReader() {}

void SinexDataReader::loadObservations(RecordInputBase &dfi)
{
    std::string input;
    if (!dfi.getNextLine(input))
        dfi.raiseError("SINEX header missing");
    if (input.size() < 69 || input.substr(0, 5) != "%=SNX")
        dfi.raiseError("Invalid SINEX header");
    double startdate = readSinexDate(input.substr(32, 12));
    double enddate = readSinexDate(input.substr(45, 12));
    double nprm = atoi(input.substr(60, 5).c_str());
    if (startdate == UNDEFINED_DATE || enddate == UNDEFINED_DATE)
        dfi.raiseError("Invalid start/end date in SINEX header");
    if (obsdate == UNDEFINED_DATE)
        obsdate = (startdate + enddate) / 2.0;
    if (input.substr(68).find('S') == std::string::npos)
        dfi.raiseError("Invalid SINEX file - does not contain station data");

    // Scan for SITE/ID section

    std::vector<int> prmMap(nprm + 1, -1);
    std::map<std::string, int> codeMap;
    std::vector<std::unique_ptr<PointData>> points;

    findSection(dfi, "SOLUTION/ESTIMATE");
    while (dfi.getNextLine(input))
    {
        if (input[0] == '-')
            break;
        if (input[0] != ' ')
            continue;

        if (input.size() < 70)
            dfi.raiseError("Invalid truncated SOLUTION/ESTIMATE line");

        // Is this a STAX, STAY, or STAX record?
        std::string prmtype = input.substr(7, 4);
        int ord = -1;
        switch (prmtype[3])
        {
        case 'X':
            ord = 0;
            break;
        case 'Y':
            ord = 1;
            break;
        case 'Z':
            ord = 2;
            break;
        }
        if (ord == -1)
            continue;
        if (prmtype.substr(0, 3) != "STA")
            continue;

        // Find point code and solution strings
        std::string code = input.substr(14, 4);
        std::string point = input.substr(19, 2);
        std::string soln = input.substr(22, 4);
        boost::trim(code);
        boost::trim(point);
        boost::trim(soln);
        if (use_point_code)
            code = code + "_" + point;
        else
            soln = point + "_" + soln;

        // Find an existing coordinate for the code or create a new one if none found
        auto mapped = codeMap.find(code);
        int id;
        if (mapped == codeMap.end())
        {
            id = points.size();
            codeMap[code] = id;
            points.push_back(std::unique_ptr<PointData>(new PointData));
            points[id]->id = id;
            points[id]->lineno = dfi.lineNumber();
            points[id]->code = code;
            points[id]->soln = soln;
        }
        else
        {
            id = codeMap[code];
        }

        // Check that there are not duplicated solutions for the mark
        if (points[id]->soln != soln)
            dfi.raiseError(std::string("Cannot have more than one solution for ") + code);

        // Store the ordinate
        double value = strtod(input.substr(47, 21).c_str(), 0);
        points[id]->xyz[ord] = value;
        int prmid = atoi(input.substr(1, 5).c_str());

        // Store the mapping from SINEX param number to cvr row number
        if (prmid < 1 || prmid > nprm)
            dfi.raiseError(std::string("Invalid parameter number " + input.substr(1, 5)));
        prmMap[prmid] = id * 3 + ord;
    }

    // Try creating the record

    // coef_class_info *ci=coef_class( COEF_CLASS_REFFRM );
    int classid = (int) ldt_get_id(ID_COEFCLASS, COEF_CLASS_REFFRM, 0);
    int idreffrm = (int) ldt_get_id(ID_CLASSNAME, classid, ref_frame.c_str());

    ldt_inststn(0, 0.0);
    ldt_date(obsdate);
    try
    {
        for (auto pt = points.begin(); pt != points.end(); pt++)
        {
            int tgtid = (int) ldt_get_id(ID_STATION, 0, (*pt)->code.c_str());
            if (tgtid == 0)
                dfi.raiseError(std::string("Undefined station ") + (*pt)->code);
            ldt_lineno((*pt)->lineno);
            ldt_tgtstn(tgtid, 0.0);
            ldt_nextdata(GX);
            ldt_value((*pt)->xyz);
            ldt_classification(classid, idreffrm);
        }

        double *cvr = ldt_covariance(CVR_FULL, 0);

        if (cvr)
        {
            std::vector<std::string> cova_sections;
            cova_sections.push_back("SOLUTION/MATRIX_ESTIMATE L COVA");
            cova_sections.push_back("SOLUTION/MATRIX_ESTIMATE U COVA");
            int section = findSection(dfi, cova_sections, false);
            if (section < 0)
            {
                dfi.raiseError(std::string("Cannot find SINEX file SOLUTION/MATRIX_ESTIMATE (U|L) COVA block"));
            }
            bool lcova = section == 0;

            while (dfi.getNextLine(input))
            {
                if (input[0] == '-')
                    break;
                if (input[0] != ' ')
                    continue;
                int prm1 = atoi(input.substr(1, 5).c_str());
                if (prm1 < 1 || prm1 > nprm)
                    continue;
                int icvr = prmMap[prm1];
                if (icvr < 0)
                    continue;
                int prm2 = atoi(input.substr(7, 5).c_str());
                if (prm2 < 1)
                    continue;
                int col = 13;
                int prm2end = lcova ? prm1 : nprm;
                for (; prm2 <= prm2end && col < 78; prm2++, col += 22)
                {
                    int jcvr = prmMap[prm2];
                    if (jcvr < 0)
                        continue;
                    double value = strtod(input.substr(col, 21).c_str(), 0);
                    int pcvr;
                    if (icvr > jcvr)
                    {
                        pcvr = (icvr * (icvr + 1)) / 2 + jcvr;
                    }
                    else
                    {
                        pcvr = (jcvr * (jcvr + 1)) / 2 + icvr;
                    }
                    cvr[pcvr] = value;
                }
            }
        }

        ldt_end_data();
    }
    catch (const RecordError &error)
    {
        ldt_cancel_inst();
        throw;
    }
}

bool SinexDataReader::findSection(RecordInputBase &dfi, const std::string section, bool raiseError)
{
    std::vector<std::string> sections;
    sections.push_back(section);
    return findSection(dfi, sections, raiseError) >= 0;
}

int SinexDataReader::findSection(RecordInputBase &dfi, const std::vector<std::string> &sections, bool raiseError)
{
    std::string input;
    while (dfi.getNextLine(input))
    {
        if (input[0] != '+')
            continue;
        for (int i = 0; i < (int)sections.size(); i++)
        {
            int len = sections[i].size();
            if (sections[i] == input.substr(1, len))
                return i;
        }
    }
    if (raiseError)
        dfi.raiseError(std::string("Cannot find SINEX file ") + sections[0] + " block");
    return -1;
}

double SinexDataReader::readSinexDate(const std::string datestr)
{
    RGX::regex re("^\\s*(\\d\\d?)\\:(\\d\\d\\d)\\:(\\d\\d\\d\\d\\d)\\s*$");
    RGX::smatch match;
    if (!RGX::regex_match(datestr.begin(), datestr.end(), match, re))
    {
        return UNDEFINED_DATE;
    }
    int yy = atoi(match[1].str().c_str());
    int ddd = atoi(match[2].str().c_str());
    int sec = atoi(match[3].str().c_str());
    yy += (yy < 50 ? 2000 : 1900);
    return snap_yds(yy, ddd, sec);
}

} // namespace SNAP
} // namespace LINZ

using namespace LINZ;
using namespace SNAP;

/////////////////////////////////////////////////////////////////////////////
//
// Global SINEX load function

int load_sinex_obs(const char *options, DATAFILE *df, int (*check_progress)(DATAFILE *df))
{
    try
    {
        OptionString config(options ? options : "");
        LINZ::SNAP::SinexDataReader snx(config);
        DatafileInput dfi(df, check_progress);
        try
        {
            snx.loadObservations(dfi);
        }
        catch (RecordError &error)
        {
            dfi.handleError(error);
        }
        if (dfi.aborted())
            return OPERATION_ABORTED;
    }
    catch (RecordError &error)
    {
        handle_error(INVALID_DATA, error.message().c_str(), error.location().c_str());
        return INVALID_DATA;
    }
    return OK;
}
