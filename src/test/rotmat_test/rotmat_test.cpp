#include "snapconfig.hpp"

// Standalone unit test for the rotmat class in util/geodetic.h. Awaiting
// migration to CTest - until then, failures are reported to stdout and the
// process exit code is the pass/fail signal (0 = all passed), matching how
// other test-only tools in src/test/ are checked.
//
// Expected values are worked out by hand from the definition: the rows of the
// rotation from geocentric to local axes are (-sinLon, cosLon, 0),
// (-sinLat cosLon, -sinLat sinLon, cosLat) and (cosLat cosLon, cosLat sinLon,
// sinLat).

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

#include "util/geodetic.h"

namespace
{

int failures = 0;

void check_close( const double actual, const double expected, const double tolerance, const std::string &description )
{
    if( std::fabs( actual - expected ) <= tolerance ) return;
    ++failures;
    std::cout << "FAIL: " << description << ": expected " << expected << " got " << actual << "\n";
}

void check_vector( const vector3 actual, const double x, const double y, const double z, const double tolerance,
                   const std::string &description )
{
    check_close( actual[0], x, tolerance, description + " (first component)" );
    check_close( actual[1], y, tolerance, description + " (second component)" );
    check_close( actual[2], z, tolerance, description + " (third component)" );
}

void test_default_is_the_identity()
{
    const rotmat identity;
    vector3 input = { 1.5, -2.0, 0.7 };
    vector3 output;
    identity.rotvec( input, output );
    check_vector( output, 1.5, -2.0, 0.7, 1.0e-15, "rotvec by the default rotation" );
    identity.unrotvec( input, output );
    check_vector( output, 1.5, -2.0, 0.7, 1.0e-15, "unrotvec by the default rotation" );
    vector3 vertical;
    identity.rot_vertical( vertical );
    check_vector( vertical, 0.0, 0.0, 1.0, 1.0e-15, "vertical of the default rotation" );
}

void test_constructor_at_the_origin()
{
    // At latitude 0 and longitude 0 the geocentric X axis is the local up
    // axis, Y is local east and Z is local north
    const rotmat origin( 0.0, 0.0 );
    vector3 output;
    vector3 xAxis = { 1.0, 0.0, 0.0 };
    origin.rotvec( xAxis, output );
    check_vector( output, 0.0, 0.0, 1.0, 1.0e-15, "geocentric X axis at the origin" );
    vector3 yAxis = { 0.0, 1.0, 0.0 };
    origin.rotvec( yAxis, output );
    check_vector( output, 1.0, 0.0, 0.0, 1.0e-15, "geocentric Y axis at the origin" );
    vector3 zAxis = { 0.0, 0.0, 1.0 };
    origin.rotvec( zAxis, output );
    check_vector( output, 0.0, 1.0, 0.0, 1.0e-15, "geocentric Z axis at the origin" );
    origin.rot_vertical( output );
    check_vector( output, 1.0, 0.0, 0.0, 1.0e-15, "vertical at the origin" );
}

void test_unrotvec_reverses_rotvec()
{
    const rotmat point( 0.5, -1.2 );
    vector3 input = { 1.5, -2.0, 0.7 };
    vector3 local;
    vector3 back;
    point.rotvec( input, local );
    point.unrotvec( local, back );
    check_vector( back, 1.5, -2.0, 0.7, 1.0e-12, "unrotvec of rotvec" );

    const double inputLength = std::sqrt( 1.5 * 1.5 + 2.0 * 2.0 + 0.7 * 0.7 );
    const double localLength = std::sqrt( local[0] * local[0] + local[1] * local[1] + local[2] * local[2] );
    check_close( localLength, inputLength, 1.0e-12, "rotvec keeps the length of a vector" );
}

void test_input_and_output_can_be_the_same_vector()
{
    const rotmat point( 0.5, -1.2 );
    vector3 separate = { 1.5, -2.0, 0.7 };
    vector3 expected;
    point.rotvec( separate, expected );

    vector3 inPlace = { 1.5, -2.0, 0.7 };
    point.rotvec( inPlace, inPlace );
    check_vector( inPlace, expected[0], expected[1], expected[2], 0.0, "rotvec in place" );

    vector3 expectedBack;
    point.unrotvec( expected, expectedBack );
    point.unrotvec( inPlace, inPlace );
    check_vector( inPlace, expectedBack[0], expectedBack[1], expectedBack[2], 0.0, "unrotvec in place" );
}

void test_vertical_is_the_local_up_axis()
{
    const rotmat point( 0.5, -1.2 );
    vector3 vertical;
    point.rot_vertical( vertical );
    vector3 up = { 0.0, 0.0, 1.0 };
    vector3 expected;
    point.unrotvec( up, expected );
    check_vector( vertical, expected[0], expected[1], expected[2], 1.0e-15, "vertical equals unrotvec of the up axis" );
    check_close( vertical[0], std::cos( 0.5 ) * std::cos( -1.2 ), 1.0e-15, "vertical X" );
    check_close( vertical[1], std::cos( 0.5 ) * std::sin( -1.2 ), 1.0e-15, "vertical Y" );
    check_close( vertical[2], std::sin( 0.5 ), 1.0e-15, "vertical Z" );
}

void test_gravimetric_without_deflection_is_topocentric()
{
    const rotmat topocentric( 0.5, -1.2 );
    const rotmat gravimetric = rotmat::gravimetric( 0.5, -1.2, 0.0, 0.0 );
    check_close( gravimetric.cslt, topocentric.cslt, 0.0, "cosine of latitude" );
    check_close( gravimetric.snlt, topocentric.snlt, 0.0, "sine of latitude" );
    check_close( gravimetric.csln, topocentric.csln, 0.0, "cosine of longitude" );
    check_close( gravimetric.snln, topocentric.snln, 0.0, "sine of longitude" );
}

void test_gravimetric_vertical_is_deflected()
{
    // The deflection xi in latitude and eta/cos(latitude) in longitude moves
    // the vertical by about sqrt(xi^2 + eta^2) of arc
    const double xi = 1.0e-4;
    const double eta = 2.0e-4;
    const rotmat topocentric( 0.5, -1.2 );
    const rotmat gravimetric = rotmat::gravimetric( 0.5, -1.2, xi, eta );
    vector3 topoVertical;
    vector3 gravVertical;
    topocentric.rot_vertical( topoVertical );
    gravimetric.rot_vertical( gravVertical );
    const double cx = topoVertical[1] * gravVertical[2] - topoVertical[2] * gravVertical[1];
    const double cy = topoVertical[2] * gravVertical[0] - topoVertical[0] * gravVertical[2];
    const double cz = topoVertical[0] * gravVertical[1] - topoVertical[1] * gravVertical[0];
    const double separation = std::sqrt( cx * cx + cy * cy + cz * cz );
    check_close( separation, std::sqrt( xi * xi + eta * eta ), 1.0e-7, "arc between the topocentric and gravimetric verticals" );
}

}  // namespace

int main()
{
    test_default_is_the_identity();
    test_constructor_at_the_origin();
    test_unrotvec_reverses_rotvec();
    test_input_and_output_can_be_the_same_vector();
    test_vertical_is_the_local_up_axis();
    test_gravimetric_without_deflection_is_topocentric();
    test_gravimetric_vertical_is_deflected();

    if( failures == 0 )
    {
        std::cout << "All rotmat tests passed\n";
        return EXIT_SUCCESS;
    }
    std::cout << failures << " rotmat test(s) failed\n";
    return EXIT_FAILURE;
}
