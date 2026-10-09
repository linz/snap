// Standalone unit test for util/writecsv.h (output_csv). Awaiting migration to
// CTest - until then, failures are reported to stdout and the process exit code
// is the pass/fail signal (0 = all passed), matching how other test-only tools
// in src/test/ are checked.
//
// Each case writes a file, reads it back and compares the whole text. The
// expected text is written out by hand from the documented behaviour of the
// class, not copied from its output.

#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "util/dateutil.h"
#include "util/writecsv.h"

namespace
{

int failures = 0;

void check_equal( const std::string &actual, const std::string &expected, const std::string &description )
{
    if( actual == expected ) return;
    ++failures;
    std::cout << "FAIL: " << description << ": expected [" << expected << "] got [" << actual << "]\n";
}

void check( const bool condition, const std::string &description )
{
    if( condition ) return;
    ++failures;
    std::cout << "FAIL: " << description << "\n";
}

const std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "snap_writecsv_test";

/// Writes a file with the writer function and returns what was written. Carriage
/// returns are removed, as a text mode stream on Windows ends lines with CR LF.
std::string write_file( const bool tab_delimited, const std::function<void( output_csv & )> &writer )
{
    const std::string filename = (test_dir / "out.csv").string();
    {
        const std::unique_ptr<output_csv> csv = output_csv::open( filename, tab_delimited );
        if( ! csv )
        {
            ++failures;
            std::cout << "FAIL: could not open " << filename << "\n";
            return "";
        }
        writer( *csv );
    }
    std::ifstream in( filename, std::ios::binary );
    std::ostringstream content;
    content << in.rdbuf();
    std::string text = content.str();
    std::string::size_type cr;
    while( (cr = text.find( '\r' )) != std::string::npos ) text.erase( cr, 1 );
    return text;
}

std::string write_csv( const std::function<void( output_csv & )> &writer )
{
    return write_file( false, writer );
}

std::string write_tab( const std::function<void( output_csv & )> &writer )
{
    return write_file( true, writer );
}

void check_open()
{
    const std::string unwritable = (test_dir / "no_such_directory" / "out.csv").string();
    check( output_csv::open( unwritable, false ) == nullptr, "open: a file in a missing directory gives nullptr" );

    write_csv( []( output_csv &csv ) { csv.writeString( "a much longer first version" ); csv.endRecord(); } );
    check_equal( write_csv( []( output_csv &csv ) { csv.writeString( "b" ); csv.endRecord(); } ),
                 "\"b\"\n", "open: an existing file is replaced" );
}

void check_csv_strings()
{
    check_equal( write_csv( []( output_csv &csv ) {
                     csv.writeString( "abc" );
                     csv.writeString( "say \"hi\"" );
                     csv.writeString( "a,b" );
                     csv.writeString( "x\ny" );
                     csv.writeString( "" );
                     csv.endRecord();
                 } ),
                 "\"abc\",\"say \"\"hi\"\"\",\"a,b\",\"x\ny\",\"\"\n",
                 "csv: text is quoted, quotes doubled, comma and line feed kept, empty string is a quoted field" );
}

void check_csv_null_fields()
{
    check_equal( write_csv( []( output_csv &csv ) {
                     csv.writeNullField();
                     csv.writeString( "a" );
                     csv.writeNullFields( 2 );
                     csv.writeString( "b" );
                     csv.endRecord();
                 } ),
                 ",\"a\",,,\"b\"\n", "csv: null fields are empty and unquoted" );
    check_equal( write_csv( []( output_csv &csv ) {
                     csv.writeString( "a" );
                     csv.writeNullFields( 0 );
                     csv.writeString( "b" );
                     csv.endRecord();
                 } ),
                 "\"a\",\"b\"\n", "csv: no null fields writes nothing, not even a delimiter" );
    check_equal( write_csv( []( output_csv &csv ) {
                     csv.writeNullField();
                     csv.writeNullField();
                     csv.endRecord();
                 } ),
                 ",\n", "csv: two null fields give one delimiter" );
    check_equal( write_csv( []( output_csv &csv ) { csv.writeNullField(); csv.endRecord(); } ),
                 "\n", "csv: one null field gives an empty row" );
}

void check_records()
{
    check_equal( write_csv( []( output_csv &csv ) { csv.endRecord(); } ), "\n", "records: an empty record is a bare newline" );
    check_equal( write_csv( []( output_csv &csv ) {
                     csv.writeInt( 1 );
                     csv.writeInt( 2 );
                     csv.endRecord();
                     csv.writeInt( 3 );
                     csv.endRecord();
                 } ),
                 "1,2\n3\n", "records: a new row starts without a delimiter" );
    check_equal( write_csv( []( output_csv &csv ) { csv.writeInt( 1 ); } ),
                 "1", "records: no newline is written until endRecord" );
}

void check_numbers()
{
    check_equal( write_csv( []( output_csv &csv ) {
                     csv.writeInt( 0 );
                     csv.writeInt( -5 );
                     csv.writeInt( 2147483647 );
                     csv.writeInt( -2147483647 );
                     csv.endRecord();
                 } ),
                 "0,-5,2147483647,-2147483647\n", "numbers: integers" );
    check_equal( write_csv( []( output_csv &csv ) {
                     csv.writeDouble( 3.14159, 3 );
                     csv.writeDouble( 2.6, 0 );
                     csv.writeDouble( -1.5, 2 );
                     csv.writeDouble( 1234.5678, 1 );
                     csv.writeDouble( 1.0, -1 );
                     csv.writeDouble( 1.0 / 3.0, -1 );
                     csv.writeDouble( 0.0, 4 );
                     csv.endRecord();
                 } ),
                 "3.142,3,-1.50,1234.6,1.000000,0.333333,0.0000\n",
                 "numbers: doubles with decimal places, rounding, and six places for a negative count" );
}

void check_headers()
{
    check_equal( write_csv( []( output_csv &csv ) {
                     csv.writeHeader( "obsid" );
                     csv.writeHeader( "obs id" );
                     csv.writeHeader( "a.b-c" );
                     csv.endRecord();
                 } ),
                 "\"obsid\",\"obs_id\",\"a_b_c\"\n", "headers: characters other than letters and digits become underscores" );

    check_equal( write_csv( []( output_csv &csv ) {
                     csv.writeHeader( std::string( 31, 'a' ) );
                     csv.writeHeader( std::string( 32, 'b' ) );
                     csv.writeHeader( std::string( 33, 'c' ) );
                     csv.writeHeader( std::string( 40, 'd' ) );
                     csv.endRecord();
                 } ),
                 "\"" + std::string( 31, 'a' ) + "\",\"" + std::string( 32, 'b' ) + "\",\"" + std::string( 32, 'c' )
                     + "\",\"" + std::string( 32, 'd' ) + "\"\n",
                 "headers: names are cut to 32 characters" );
    check_equal( write_tab( []( output_csv &csv ) {
                     csv.writeHeader( "obs id" );
                     csv.writeHeader( "x" );
                     csv.endRecord();
                 } ),
                 "obs_id\tx\n", "headers: not quoted in tab mode" );
}

void check_dates()
{
    check_equal( write_csv( []( output_csv &csv ) {
                     csv.writeString( "a" );
                     csv.writeDate( UNDEFINED_DATE );
                     csv.writeString( "b" );
                     csv.endRecord();
                 } ),
                 "\"a\",,\"b\"\n", "dates: an undefined date is a null field" );
    check_equal( write_csv( []( output_csv &csv ) {
                     csv.writeDate( snap_datetime( 2020, 3, 15, 13, 5, 9 ) );
                     csv.writeDate( snap_date( 2020, 3, 15 ) );
                     csv.endRecord();
                 } ),
                 "\"2020-03-15 13:05:09\",\"2020-03-15 00:00:00\"\n",
                 "dates: quoted yyyy-mm-dd hh:mm:ss, with the time shown at midnight" );
    check_equal( write_tab( []( output_csv &csv ) {
                     csv.writeDate( snap_datetime( 2020, 3, 15, 13, 5, 9 ) );
                     csv.writeDate( UNDEFINED_DATE );
                     csv.writeInt( 4 );
                     csv.endRecord();
                 } ),
                 "2020-03-15 13:05:09\t\t4\n", "dates: tab mode is unquoted, undefined is empty" );
}

void check_tab_delimited()
{
    check_equal( write_tab( []( output_csv &csv ) {
                     csv.writeString( "a\tb" );
                     csv.writeString( "c\nd" );
                     csv.writeString( "e,f" );
                     csv.writeString( "say \"hi\"" );
                     csv.writeInt( 7 );
                     csv.writeDouble( 2.5, 1 );
                     csv.endRecord();
                 } ),
                 "a b\tc d\te,f\tsay \"hi\"\t7\t2.5\n",
                 "tab: no quoting, tab and line feed in text become spaces, comma and quote kept" );
    check_equal( write_tab( []( output_csv &csv ) {
                     csv.writeNullField();
                     csv.writeString( "a" );
                     csv.writeNullFields( 2 );
                     csv.writeString( "b" );
                     csv.endRecord();
                 } ),
                 "\ta\t\t\tb\n", "tab: null fields are empty between tabs" );
    check_equal( write_tab( []( output_csv &csv ) {
                     csv.writeString( "" );
                     csv.writeString( "x" );
                     csv.endRecord();
                 } ),
                 "\tx\n", "tab: an empty string is an empty field" );
}

}

int main()
{
    std::filesystem::remove_all( test_dir );
    std::filesystem::create_directories( test_dir );

    check_open();
    check_csv_strings();
    check_csv_null_fields();
    check_records();
    check_numbers();
    check_headers();
    check_dates();
    check_tab_delimited();

    std::filesystem::remove_all( test_dir );

    if( failures )
    {
        std::cout << failures << " writecsv_test check(s) failed\n";
        return 1;
    }
    std::cout << "writecsv_test: all checks passed\n";
    return 0;
}
