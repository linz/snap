#include "snapconfig.hpp"

// Standalone unit test for the observation modifications in
// snapdata/obsmod.cpp. A set of observations is modified by criteria of every
// kind (and of every action), and the result for each observation is compared
// with the result the previous implementation of obsmod.cpp gave. Failures are
// reported to stdout and the process exit code is the pass/fail signal
// (0 = all passed), as for the other tests in src/test/. Run with --print to
// write the results out instead of comparing them.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "snapdata/obsmod.h"
#include "snapdata/survdata.h"
#include "util/classify.h"
#include "util/errdef.h"

namespace
{

/// The data files known to the test, by name.
const std::map<std::string,int> file_ids{ { "a.dat", 1 }, { "b.dat", 2 }, { "c.dat", 3 } };

int test_file_id( std::string_view name, file_context * )
{
    const auto found=file_ids.find( std::string( name ) );
    return found == file_ids.end() ? -1 : found->second;
}

std::string test_file_name( int file_id )
{
    for( const auto &file : file_ids ) if( file.second == file_id ) return file.first;
    return "";
}

/// The observations for one data file. Observation i has the id
/// 10*file+i, a classification "equip" and "obsr" with the given values, and
/// the data type GB for even i and SD for odd i.
struct TestSurvey
{
    TestSurvey( int file, classifications &classes, const std::vector<std::pair<std::string,std::string>> &values )
    {
        std::memset( &sd, 0, sizeof( sd ) );
        const int equip=classes.id( "equip", 1 );
        const int obsr=classes.id( "obsr", 1 );
        sd.format=SD_OBSDATA;
        sd.file=file;
        sd.nobs=static_cast<int>( values.size() );
        odata.resize( values.size() );
        clsf.resize( 2*values.size() );
        for( size_t i=0; i<values.size(); ++i )
        {
            clsf[2*i].class_id=equip;
            clsf[2*i].name_id=classes.value_id( equip, values[i].first, 1 );
            clsf[2*i+1].class_id=obsr;
            clsf[2*i+1].name_id=classes.value_id( obsr, values[i].second, 1 );
            obsdata &od=odata[i];
            std::memset( &od, 0, sizeof( od ) );
            od.tgt.type= i%2 == 0 ? GB : SD;
            od.tgt.id=10*file+static_cast<int>( i );
            od.tgt.nclass=2;
            od.tgt.iclass=static_cast<int>( 2*i );
            od.tgt.errfct=1.0;
            od.error=1.0;
        }
        sd.obs.odata=odata.data();
        sd.clsf=clsf.data();
    }

    survdata sd;
    std::vector<obsdata> odata;
    std::vector<classdata> clsf;
};

int add( void *obsmod, const std::string &criteria, int action, double err1=1.0, double err2=0.0 )
{
    return add_obs_modifications( nullptr, obsmod, criteria, action, err1, err2 );
}

/// Applies the modifications to the observations, and describes them.
std::string describe_results()
{
    classifications classes;
    void *obsmod=new_obs_modifications( nullptr, &classes );
    set_obs_modifications_file_func( obsmod, test_file_id, test_file_name );

    // The criteria are in the order they are added, and several share a
    // key test (equip=GPS1, or the file a.dat) or have none.
    add( obsmod, "equip=GPS1", OBS_MOD_REWEIGHT, 2.0 );
    add( obsmod, "equip=TS5", OBS_MOD_REWEIGHT, 3.0 );
    add( obsmod, "equip=GPS1 obsr=Bob", OBS_MOD_REWEIGHT, 5.0 );
    add( obsmod, "obsr=Ann", OBS_MOD_REJECT );
    add( obsmod, "data_type=GB id=22", OBS_MOD_IGNORE );
    add( obsmod, "data_file=a.dat", OBS_MOD_REWEIGHT_SET, 1.5 );
    add( obsmod, "data_file=*b.dat", OBS_MOD_REWEIGHT, 7.0 );
    add_obs_option_modification( nullptr, obsmod, "obsr=Bob", 1, 4 );
    add( obsmod, "data_file=b.dat", OBS_MOD_OFFSET_ERROR, 0.1, 0.2 );
    add( obsmod, "equip=TS5", OBS_MOD_ANTENNA_OFFSET, 0.5 );
    add( obsmod, "equip=TS5 obsr=Ann", OBS_MOD_CENTROID_ERROR, 0.3, 0.4 );
    add_obs_modifications_classification( nullptr, obsmod, "obsr", "Cat/Dan", OBS_MOD_REWEIGHT, 11.0, OK );
    add_obs_modifications_classification( nullptr, obsmod, "equip", "G*", OBS_MOD_REWEIGHT, 13.0, OK );
    add_obs_modifications_datafile_factor( nullptr, obsmod, 3, "c.dat", 17.0 );
    add_obs_modifications_classification( nullptr, obsmod, "data_file", "c.dat", OBS_MOD_REJECT, 1.0, OK );

    std::vector<TestSurvey> surveys;
    const std::vector<std::pair<std::string,std::string>> values{
        { "GPS1", "Ann" }, { "GPS1", "Bob" }, { "TS5", "Ann" }, { "TS5", "Bob" },
        { "GPS2", "Cat" }, { "GPS1", "Dan" }, { "TS5", "Cat" }, { "TS6", "Dan" } };
    for( const auto &file : file_ids ) surveys.emplace_back( file.second, classes, values );

    std::ostringstream result;
    for( TestSurvey &survey : surveys )
    {
        const int nignored=apply_obs_modifications( obsmod, &survey.sd );
        result << "file " << survey.sd.file << " ignored " << nignored
               << " fromhgt " << survey.sd.fromhgt << " options " << survey.sd.options << "\n";
        for( const obsdata &od : survey.odata )
        {
            result << "  obs " << od.tgt.id << " unused " << static_cast<int>( od.tgt.unused )
                   << " error " << od.error << " errfct " << od.tgt.errfct
                   << " tohgt " << od.tgt.tohgt << "\n";
        }
        result << "  ignore file " << survey.sd.file << " " << obsmod_ignore_datafile( obsmod, survey.sd.file ) << "\n";
    }

    // The summary of the modifications
    FILE *summary=std::tmpfile();
    summarize_obs_modifications( obsmod, summary, "> " );
    std::rewind( summary );
    char line[512];
    while( std::fgets( line, sizeof( line ), summary ) ) result << line;
    std::fclose( summary );

    delete_obs_modifications( obsmod );
    return result.str();
}

/// The result the previous implementation gave.
const char *const expected_results = R"EXPECTED(file 1 ignored 0 fromhgt 0.5 options 4
  obs 10 unused 1 error 3 errfct 3 tohgt 0
  obs 11 unused 0 error 15 errfct 15 tohgt 0
  obs 12 unused 1 error 4.5 errfct 4.5 tohgt 0.5
  obs 13 unused 0 error 4.5 errfct 4.5 tohgt 0
  obs 14 unused 0 error 1.5 errfct 1.5 tohgt 0
  obs 15 unused 0 error 3 errfct 3 tohgt 0
  obs 16 unused 0 error 4.5 errfct 4.5 tohgt 0.5
  obs 17 unused 0 error 1.5 errfct 1.5 tohgt 0
  ignore file 1 0
file 2 ignored 1 fromhgt 0.5 options 4
  obs 20 unused 1 error 14 errfct 14 tohgt 0
  obs 21 unused 0 error 70 errfct 70 tohgt 0
  obs 22 unused 3 error 1 errfct 1 tohgt 0
  obs 23 unused 0 error 21 errfct 21 tohgt 0
  obs 24 unused 0 error 7 errfct 7 tohgt 0
  obs 25 unused 0 error 14 errfct 14 tohgt 0
  obs 26 unused 0 error 21 errfct 21 tohgt 0.5
  obs 27 unused 0 error 7 errfct 7 tohgt 0
  ignore file 2 0
file 3 ignored 0 fromhgt 0.5 options 4
  obs 30 unused 1 error 34 errfct 34 tohgt 0
  obs 31 unused 1 error 170 errfct 170 tohgt 0
  obs 32 unused 1 error 51 errfct 51 tohgt 0.5
  obs 33 unused 1 error 51 errfct 51 tohgt 0
  obs 34 unused 1 error 17 errfct 17 tohgt 0
  obs 35 unused 1 error 34 errfct 34 tohgt 0
  obs 36 unused 1 error 51 errfct 51 tohgt 0.5
  obs 37 unused 1 error 17 errfct 17 tohgt 0
  ignore file 3 0

> The following observations are ignored:
>   Observations:
>     - which are of type GB (GPS baseline), and
>     - where the observation id is 22

> The following observations are rejected
>   Observations where obsr classification is "Ann"
>   Observations which are from file c.dat

> Errors of the following observations are scaled by 17.000
>   Observations which are from file c.dat

> Errors of the following observations are scaled by 13.000
>   Observations where equip classification is "G*"

> Errors of the following observations are scaled by 11.000
>   Observations where obsr classification is "Cat/Dan"

> Errors of the following observations are scaled by 7.000
>   Observations which are from files matching *b.dat

> Errors of the following observations are scaled by 5.000
>   Observations:
>     - where equip classification is "GPS1", and
>     - where obsr classification is "Bob"

> Errors of the following observations are scaled by 3.000
>   Observations where equip classification is "TS5"

> Errors of the following observations are scaled by 2.000
>   Observations where equip classification is "GPS1"

> Note: error factors are multiplied for observations meeting several criteria

> Errors of the following observations are scaled by set by 1.500
>   Observations in sets including one or more observations which are from file a.dat

> Offset error 0.100 0.200 m applied to the following observations
>   Observations which are from file b.dat

> Offset error 0.100 0.200 m applied to the following observations
>   Observations which are from file b.dat

> Offset error 0.100 0.200 m applied to the following observations
>   Observations which are from file b.dat

> Offset error 0.100 0.200 m applied to the following observations
>   Observations which are from file b.dat

> Offset error 0.100 0.200 m applied to the following observations
>   Observations which are from file b.dat

> Offset error 0.100 0.200 m applied to the following observations
>   Observations which are from file b.dat

> Offset error 0.100 0.200 m applied to the following observations
>   Observations which are from file b.dat

> Offset error 0.100 0.200 m applied to the following observations
>   Observations which are from file b.dat

> Offset error 0.100 0.200 m applied to the following observations
>   Observations which are from file b.dat

> Offset error 0.100 0.200 m applied to the following observations
>   Observations which are from file b.dat

> Offset error 0.100 0.200 m applied to the following observations
>   Observations which are from file b.dat

> Offset error 0.100 0.200 m applied to the following observations
>   Observations which are from file b.dat

> Offset error 0.100 0.200 m applied to the following observations
>   Observations which are from file b.dat

> Offset error 0.100 0.200 m applied to the following observations
>   Observations which are from file b.dat

> Offset error 0.100 0.200 m applied to the following observations
>   Observations which are from file b.dat

> Centroid error 0.300 0.400 m applied to the following observations
>   Observations:
>     - where equip classification is "TS5", and
>     - where obsr classification is "Ann"

> Centroid error 0.300 0.400 m applied to the following observations
>   Observations:
>     - where equip classification is "TS5", and
>     - where obsr classification is "Ann"

> Centroid error 0.300 0.400 m applied to the following observations
>   Observations:
>     - where equip classification is "TS5", and
>     - where obsr classification is "Ann"

> Centroid error 0.300 0.400 m applied to the following observations
>   Observations:
>     - where equip classification is "TS5", and
>     - where obsr classification is "Ann"

> Centroid error 0.300 0.400 m applied to the following observations
>   Observations:
>     - where equip classification is "TS5", and
>     - where obsr classification is "Ann"

> Centroid error 0.300 0.400 m applied to the following observations
>   Observations:
>     - where equip classification is "TS5", and
>     - where obsr classification is "Ann"

> Centroid error 0.300 0.400 m applied to the following observations
>   Observations:
>     - where equip classification is "TS5", and
>     - where obsr classification is "Ann"

> Centroid error 0.300 0.400 m applied to the following observations
>   Observations:
>     - where equip classification is "TS5", and
>     - where obsr classification is "Ann"

> Centroid error 0.300 0.400 m applied to the following observations
>   Observations:
>     - where equip classification is "TS5", and
>     - where obsr classification is "Ann"

> Centroid error 0.300 0.400 m applied to the following observations
>   Observations:
>     - where equip classification is "TS5", and
>     - where obsr classification is "Ann"

> Centroid error 0.300 0.400 m applied to the following observations
>   Observations:
>     - where equip classification is "TS5", and
>     - where obsr classification is "Ann"

> Centroid error 0.300 0.400 m applied to the following observations
>   Observations:
>     - where equip classification is "TS5", and
>     - where obsr classification is "Ann"

> Centroid error 0.300 0.400 m applied to the following observations
>   Observations:
>     - where equip classification is "TS5", and
>     - where obsr classification is "Ann"

> Centroid error 0.300 0.400 m applied to the following observations
>   Observations:
>     - where equip classification is "TS5", and
>     - where obsr classification is "Ann"

> Centroid error 0.300 0.400 m applied to the following observations
>   Observations:
>     - where equip classification is "TS5", and
>     - where obsr classification is "Ann"

> Antenna offset 0.500 m applied to the following GX/GB observations
>   Observations where equip classification is "TS5"
)EXPECTED";

}

int main( int argc, char *argv[] )
{
    const std::string actual=describe_results();
    if( argc > 1 && std::string( argv[1] ) == "--print" )
    {
        std::cout << actual;
        return EXIT_SUCCESS;
    }
    if( actual == expected_results )
    {
        std::cout << "All obsmod tests passed\n";
        return EXIT_SUCCESS;
    }
    std::cout << "FAIL: the observation modifications differ from the expected result:\n" << actual;
    return EXIT_FAILURE;
}
