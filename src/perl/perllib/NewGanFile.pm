use strict;
use FileHandle;
use Geodetic::CoordSys;
use Geodetic::Datum;
use Geodetic::Ellipsoid;
use Geodetic::TMProjection;
use Carp;

my $warning = <<EOD;

WARNING: This is software is under development.  Much of it is untested.
Do not assume that any output is correct!!!

NOTES:
1) Do Variance modifications (record 56-58) apply to default variances, or
   only to entered precisions (assume the latter).
2) Many uncertainties in calculation of default precisions .. see CalcStdDev.
EOD

print $warning;

my $notes = <<EOD;

Coordinate systems are incomplete ... see GetCrdsys.  

EOD

######################################################################
# Package to identify referencing undefined hash elements

package AutoHash;
use vars qw/$AUTOLOAD/;

sub new {
  my ($class, @data) = @_;
  &Carp::confess() if scalar(@data) % 2 == 1;
  my $hash = {@data};
  return bless $hash, $class;
  }

sub AUTOLOAD {
  my ($self) = @_;
  my $element = $AUTOLOAD;
  $element =~ s/.*\:\://;
  if( ! exists $self->{$element} ) {
      my ($package, $filename, $line, $sub ) = caller(1);
      ($package, $filename, $line ) = caller(0);
      $filename =~ s/.*[\\\/]//;
      die "Invalid variable $element called from $sub at $filename line $line\n" if ! exists $self->{$element};
      }
  return $self->{$element};
  }

sub get {
  my( $self, $element, $default ) = @_;
  if( exists $self->{$element} ) {
     return $self->{$element};
     }
  return $default;
  }

sub has {
  my( $self, $element ) = @_;
  return exists $self->{$element};
  }

######################################################################
# Package to identify referencing undefined hash elements

package FixedFormatRecord;

# new - defines a new fixed format record.  Each record is defined by 
# 4 entries in a list, being name, start_column, end_column, default.
# A value of - for default is equivalent to nothing.

sub new 
{
    my($class, @definition) = @_;
    my $fields = [];
    my $name = shift(@definition);
    my $maxcol = 0;
    while( @definition > 4 )
    {
        my ($field, $startcol, $endcol, $default, $re ) = splice(@definition,0,5);
        $maxcol = $endcol if $endcol > $maxcol;
        $startcol--;
        my $length = $endcol - $startcol;
        $default = '' if ! defined($default) || $default eq '-';
        my $qre;
        my $trim = 1;
        if( defined($re) && $re =~ s/^notrim\:// ) {
           $trim = 1;
           }
 
        if( defined($re) && $re ne '-' && $re ne '' ) {
           eval {
              if( $re =~ /^((?:pos)?)float(\??)$/ ) {
                 $re = $1 eq 'pos' ? '\\+?' : '[\\-+]?';
                 $re .= '(\\d+\\.?|\\.\\d)\\d*(?:[efgEFG]\\-?\\d+)?';
                 $re = '(?:'.$re.')?' if $2 eq '?';
                 $re = '^'.$re.'$';
                 }
              $qre = qr/$re/ if $re ne '-';
              };
           if( $@ ) {
              print "Invalid regular expression for $field in $name record\n";
              };
           }
        push(@$fields,[$field,$startcol,$length,$default,$qre,$trim]);
    }
    return bless { name=>$name, fields=>$fields, padding=>' 'x$maxcol}, $class;
}


sub read
{
    my($self,$string,$data) = @_;
    my $fields = $self->{fields};
    $string .= $self->{padding};
    $data = new AutoHash if ! $data;
    my @errors = ();
    foreach my $field (@$fields)
    {
       my $fieldname = $field->[0];
       my $value = substr($string,$field->[1],$field->[2] );
       if( $field->[5] ) {
          $value =~ s/^\s+//;
          $value =~ s/\s*$//;
          }
       $value = $field->[3] if $value eq '';
       $data->{$fieldname} = $value;
       my $re = $field->[4];
       push(@errors,"Invalid value $value for $fieldname in $self->{name}\n")
          if defined($re) && $value !~ $re;
    }
    return $data, @errors;
} 

######################################################################
# Package for stations. Not very complete ... added late in the piece
# so much functionality still in NewGanFile...


package NewGanFile::Station;
use vars qw/@ISA/;
@ISA=qw/AutoHash/;

my $degtorad = atan2(1,1)/45.0;
my $sectorad = $degtorad/3600.0;

sub xyz {
   my($self) = @_;
   if( ! $self->has('xyzcoord') ) {
      $self->{xyzcoord} = $self->coord->as( $self->coord->coordsys->asxyz );
      }
   return $self->xyzcoord;
   }

sub distance_to {
   my( $self, $other ) = @_;
   my $xyz0 = $self->xyz;
   my $xyz1 = $other->xyz;
   my $dx = $xyz1->[0] - $xyz0->[0];
   my $dy = $xyz1->[1] - $xyz0->[1];
   my $dz = $xyz1->[2] - $xyz0->[2];
   return sqrt( $dx*$dx + $dy*$dy + $dz*$dz );
   }

sub metres_per_arcsec {
   my( $self ) = @_;
   my $xyz = $self->xyz;
   my $re = sqrt( $xyz->[0]*$xyz->[0] + $xyz->[1]*$xyz->[1] );
  
   my $mperslon = $re * $sectorad;

   my $latrad = $self->coord->lat * $degtorad;
   my $clat = cos($latrad);
   my $slat = sin($latrad);

   my $ellipsoid = $self->coord->coordsys->ellipsoid;
   my $a2 = $ellipsoid->a;
   my $b2 = $ellipsoid->b;
   $a2 = $a2*$a2; $b2 = $b2*$b2;
 
   my $bsac =  ($a2*$clat*$clat + $b2*$slat*$slat);

   my $mperslat = $sectorad*$a2*$b2/sqrt($bsac*$bsac*$bsac);

   return ($mperslat, $mperslon);
   }


######################################################################
# Package for scalar (terrestrial) obs. Not very completed ... added late in the piece
# so much functionality still in NewGanFile...

package NewGanFile::ScalarObs;
use Carp;
use vars qw/@ISA/;
@ISA=qw/AutoHash/;

my $radtosec = (45*3600)/atan2(1,1);

# newganfile parameter is a bit circular, but need lots of information 
# from the NewGan file to generate precision ...

sub CalcStdDev {
    my( $self, $newganfile ) = @_;

    if( ref($newganfile) ne 'NewGanFile' ) {
       croak "newganfile parameter to NewGanFile::ScalarObs::CalcStdDev is not valid\n";
       }
 
    # If the precision is defined convert it to a variance ...

    my $precision;
    if( $self->precision ne '' ) {
        if( $newganfile->options->precision_type eq 'STAN' ) {
            $precision = $self->precision*$self->precision;
            }
        elsif( $newganfile->options->precision_type eq 'WEIG' ) {
            $precision = 1.0/$self->precision;
            }
        else {  
            $precision = $self->precision;
            }

        $precision = $precision * $self->get('scalevariance',1.0) 
                                + $self->get('addvariance',0.0);
        }

    # Else if calculated from defaults ...

    else {
        my $defprec = $self->default_precision;

        my $precisiontype = $self->obstype->precisiontype;

        # Distances ..

        if( $precisiontype eq 'dist0' ) {
          $precision = $defprec->sdobsabs^2 +
                       $defprec->sdinstplumb^2 +
                       $defprec->sdtrgtplumb^2 +
                       ($self->value*$defprec->sdobsppm*1.0E-4)^2;
          }
                  
        # Elevation ... only the abs and inst plumb apply ...
        # Note: Assume instrument plumbing here refers to vertical error?

        elsif( $precisiontype eq 'dist1' ) {
          $precision = $defprec->sdobsabs^2 +
                       $defprec->sdinstplumb^2;
          }
  
        # Height difference ...
        # Note: Assume instrument plumbing errors here refers to vertical error?
        # Apply the ppm length based on the slope distance between the stations?

        elsif( $precisiontype eq 'dist2' ) {
          my $stfrom = $newganfile->GetStation( $self->station_id_from );
          my $stto   = $newganfile->GetStation( $self->station_id_to );
          my $length = $stfrom->DistanceTo( $stto );

          $precision = $defprec->sdobsabs^2 +
                       $defprec->sdinstplumb^2 +
                       $defprec->sdtrgtplumb^2 +
                       ($self->value*$defprec->sdobsppm*1.0E-4)^2;
                       }

        # Direction of a direction set ... ignore correlations from 
        # instrument plumbing error.  Base error on trial coordinates of station.

        elsif( $precisiontype eq 'angle0' ) {
          my $stfrom = $newganfile->GetStation( $self->station_id_from );
          my $stto   = $newganfile->GetStation( $self->station_id_to );
          my $length = $stfrom->DistanceTo( $stto );
           
          my $posvar = $defprec->sdinstplumb^2 + $defprec->sdtrgtplumb^2;
          
          $precision = $defprec->sdobs^2 + $posvar*($radtosec/$length)^2;
          }
          
        # Zenith distance. Do we treat plumbing errors as horizontal or vertical.
        # Using horizontal, scaling by sine of angle .

        elsif( $precisiontype eq 'angle1' ) {
          my $stfrom = $newganfile->GetStation( $self->station_id_from );
          my $stto   = $newganfile->GetStation( $self->station_id_to );
          my $length = $stfrom->DistanceTo( $stto );
           
          my $posvar = $defprec->sdinstplumb^2 + $defprec->sdtrgtplumb^2;
          my $sina = sin($degtorad*($self->angle_deg + $self->angle_min/60.0 + $self->angle_sec/3600.0));
          
          $precision = $defprec->sdobs^2 + $posvar*($sina*$radtosec/$length)^2;
          }

        # Angle observation ... not properly calculated, as need to calculate
        # the effect of instrument station offset on angle properly.  For now 
        # treat maximum effect as being related to sine of the angle.

        elsif( $precisiontype eq 'angle2' ) {
          my $stfrom = $newganfile->GetStation( $self->station_id_from );
          my $stref  = $newganfile->GetStation( $self->station_id_ref );
          my $lengthref = $stfrom->DistanceTo( $stref );
          my $stto   = $newganfile->GetStation( $self->station_id_to );
          my $lengthto = $stfrom->DistanceTo( $stto );
           
          my $sina = sin($degtorad*($self->angle_deg + $self->angle_min/60.0 + $self->angle_sec/3600.0));
          my $posvar = ($sina*$defprec->sdinstplumb)^2 + $defprec->sdtrgtplumb^2;
          
          
          $precision = $defprec->sdobs^2 + 
                       $posvar*(($radtosec/$lengthref)^2+($radtosec/$lengthto)^2);
          }
          
        # Azimuth observations.  Ignoring the Axis levelling and longitude errors 
        # for now.

        elsif( $precisiontype eq 'angle3' ) {
          my $stfrom = $newganfile->GetStation( $self->station_id_from );
          my $stto   = $newganfile->GetStation( $self->station_id_to );
          my $length = $stfrom->DistanceTo( $stto );
           
          my $posvar = $defprec->sdinstplumb^2 + $defprec->sdtrgtplumb^2;
          
          $precision = $defprec->sdobs^2 + $posvar*($radtosec/$length)^2;
          }

        else {
          die "Cannot calculate default precision for $precisiontype\n";
          }
        }
          
    # Convert to standard deviation

    $precision = sqrt($precision);

    # Distance precisions are in centimetres, so correct to metres

    if( $self->obstype->precisiontype =~ /^dist/ ){
        $precision = $precision / 100;
        }
 
    return $precision;
    }


######################################################################
# Package for scalar (terrestrial) obs. Not very completed ... added late in the piece
# so much functionality still in NewGanFile...

package NewGanFile::VectorObs;
use Carp;
use vars qw/@ISA/;
@ISA=qw/AutoHash/;

my $radtosec = (45*3600)/atan2(1,1);

# Same comment as above for use of newganfile parameter

sub CalcLTCovariance {
   my($self,$newganfile) = @_;
   my $vcv = $self->vcv;
   my $nrow = $vcv->nrow;
   my $elements = $vcv->elements;
   my @lt = ();
   my $nelt = ($nrow*($nrow+1))/2;

   my $varmult = $vcv->varmult;

   my $elt = 0;
   for( my $i = 1; $i <= $nrow; $i++ ) {
     for( my $j = 1; $j <= $i; $j++, $elt++ ) {
        $lt[$elt] = $elements->{"$i,$j"} * $varmult;
        }
     }

   if( $vcv->varmultn != 1.0 || $vcv->varmulte != 1.0 || $vcv->varmultu != 1.0 ||
       $vcv->sdaddn != 0.0 || $vcv->sdadde != 0.0 || $vcv->sdaddu != 0.0 )  {
      die "Cannot apply differential scaling to VCV units";
      } 

   my $unitmult = 1.0;
   if( $vcv->units eq 'm' ) {
      }
   elsif( $vcv->units eq 'cm' ) {
      $unitmult = 0.0001;
      }
   else {
      # Need to add msec/cm units and topo_m (topocentric metres)
      die "Cannot calculate Variance covariance matrix for unit type ".$vcv->units;
      }

   for( my $i = 0; $i < $nelt; $i++ ) {
      $lt[$i] *= $unitmult;
      }

   if( $self->type eq 'dxyx_baseline' && $vcv->baselineppmerr != 0.0 ) {
      my $baselinevar = $self->x*$self->x + $self->y*$self->y + $self->z*$self->z;
      $baselinevar *= $vcv->baselineppmerr*$vcv->baselineppmerr*1.0E-12;
      for( my $i = 0; $i < $nrow; $i++ ) {
         $lt[($i*($i+3))/2] += $baselinevar;
         }
      }
   return \@lt;
   }

######################################################################
# Main package for reading a NEWGAN file...

package NewGanFile;
use vars qw/@ISA/;
@ISA=qw/AutoHash/;

# Note: Got bored entering these ... add extras as required from page 18 of Newgan manual

# Note: Some confusion in manual about interpretation of 0 id.  For secondary
# ellipsoid used to mean read explicit values.  For primary ellipsoid confusion -1
# is used for explicit reading, 0 is WGS84, and manual states 7 is also WGS84.  However
# it then lists 7 as Australian National

# Manual gives ellipsoid 20 as ANS 1966, which should be the same as ANS as far as I 
# can see.  However conversion from feet to metres as provided gives a=6378102.777.  
# In this table this has been replaced with 6378160.0


my $newgan_ellipsoid = {
      0 => new Geodetic::Ellipsoid ( 6378137.0, 298.257223563, 'WGS84', 'WGS84' ),
      1 => new Geodetic::Ellipsoid ( 6378206.4, 294.9786982, 'Clarke 1866', 'WGS84' ),
      2 => new Geodetic::Ellipsoid ( 6378388.0, 297.0, 'International', 'INTERNATIONAL' ),
      6 => new Geodetic::Ellipsoid ( 6378249.145, 293.465, 'Clarke 1880', 'CLARKE1880' ),
      7 => new Geodetic::Ellipsoid ( 6378160.0, 298.25, 'Australian National', 'ANS' ),
      13 => new Geodetic::Ellipsoid ( 6378135.0, 298.26, 'WGS72', 'WGS72' ),
      18 => new Geodetic::Ellipsoid ( 6378137.0, 298.257223563, 'WGS84', 'WGS84' ),
      19 => new Geodetic::Ellipsoid ( 6378137.0, 298.257222101, 'GRS80', 'GRS80' ),
      20 => new Geodetic::Ellipsoid ( 6378160.0, 298.25, 'Australian National', 'ANS' ),
      };
      
# Don't know about C=>SGC, D=>AGC coordinates. Have ignored Cyprus.
# Also not handling plane...

my $newgan_projection = {
      'N' => new AutoHash(projcode=>'', ordtype=>'GEOG' ),
      'S' => new AutoHash(projcode=>'', ordtype=>'GEOG' ),
      'A' => new AutoHash(projcode=>'_AMG{zone}', ordtype=>'PROJ' ),
      'I' => new AutoHash(projcode=>'_ISG{zone}{subzone}', ordtype=>'PROJ' ),
      'U' => new AutoHash(projcode=>'_UTM{zone}S', ordtype=>'PROJ' ),
      'V' => new AutoHash(projcode=>'_UTM{zone}N', ordtype=>'PROJ' ),
      };

my $dimension_record = new FixedFormatRecord( "Dimension record", qw/
              dimension 1 1 2 ^[23]$
              calcgeoid 20 20 0 -
              geoid_dx 21 28 0.0 -
              geoid_dy 29 36 0.0 -
              geoid_dz 37 44 0.0 -
              geoid_zero_order 56 63 0.0 -
              /);

my $title_record = new FixedFormatRecord( "Title record", qw/
              title 1 80/, 'Newgan job', '-'
              );

my $options_record = new FixedFormatRecord( "Options record", qw/ 
              pause 1 1 - - 
              solution 2 2 C [CSI]
              tolerance 4 10 0.0001 posfloat
              iterations 14 15 6 ^(\d+|\-[12])$
              apriori_variance 16 20 1.0 posfloat
              print_full_inverse 24 25 - -
              print_normal_matrix 26 30 - -
              coordinate_output 35 35 - ^[GAIUCDKP]?$
              plane_fo_option 36 37 - ^(FO)?$
              plane_coord_bound 38 39 1 ^\d+$
              coord_output_option 40 40 1 [1-5]
              line_ellipse_radius 41 44 0 ^\d+$
              line_ellipse_option 45 45 - ^[12]?$
              list_input 50 50 0 [01]
              list_iteration_corrections 55 55 0 [01]
              apply_geoid_option 59 60 0 ^(0|1|2|-1|-2|4)$
              use_transform_params 62 62 0 [01]
              ellipsoid_id 64 65 0 ^([12]?\d|-1)$
              secondary_ellipsoid_id 69 70 - -
              max_obs_error 71 74 25.0 ^(\d+\.?\d*|-1)$
              print_equations 75 75 0 [01]
              precision_type 76 79 VARI (VARI|STAN|WEIG)
              bandwidth_optimisation 80 80 A [AYNS]
              /);
 
my $line4_record = new FixedFormatRecord("Optional line 4 record - ellipsoid", qw/
              ellipsoid_name 1 19 - -
              ellipsoid_a 20 32 - posfloat
              ellipsoid_rf 33 47 - posfloat
              /);

my $line5_record = new FixedFormatRecord("Optional line 5 record - tfm name", qw/
              tfm_name 1 40 - -
              /);
 
my $refsys_transformation_record = new FixedFormatRecord("Reference system transformation parameters", qw/
              tfm_dx 1 10 0.0 -
              tfm_dy 11 20 0.0 -
              tfm_dz 21 30 0.0 -
              tfm_rx 31 40 0.0 -
              tfm_ry 41 50 0.0 -
              tfm_rz 51 60 0.0 -
              tfm_scale 61 70 0.0 -
              /);

my $line7_record = new FixedFormatRecord("Optional line 7 record - max obs error", qw/
              max_obs_error 1 10 - posfloat
              /);

my $line8_record = new FixedFormatRecord ("Optional line 8 record - false origin", qw/
             false_easting 1 10 - -
             false_northing 11 20 - -
             /);

##################################################################]
# Records used to read stations

my $station_record = new FixedFormatRecord("Station record", qw/
            station_id 7 16 - notrim:\S
            station_name 17 30 - -
            class 32 32 - -
            source 33 39 - -
            crdtype 40 40 - ^[SNAIUVCDKP]?$
            elevation 71 79 - -
            height_class 80 80 - -
            /);

my $geog_coord_record = new FixedFormatRecord("Geographic coordinates", qw/
            lath 40 40 S [NS]
            latdeg 41 42 - \d\d?
            latmin 44 45 - \d\d?
            latsec 46 54 - posfloat
            lonh 55 55 E [EW]
            londeg 56 58 - \d\d?
            lonmin 60 61 - \d\d?
            lonsec 62 70 - posfloat
            /);

my $zone_coord_record = new FixedFormatRecord("UTM/AMG/ISG coordinates", qw/
            zone 41 42 - \S
            subzone 44 44 - -
            easting 45 57 float
            northing 58 70 float
            /);

my $proj_coord_record = new FixedFormatRecord("SGC/AGC/Plane coordinates", qw/
            easting 46 57 float
            northing 58 70 float
            /);

my $constraint_record = new FixedFormatRecord("Coordinate constraint values", qw/
            lat_constraint 4 13 0.00001 posfloat
            lon_constraint 14 23 0.00001 posfloat
            hgt_constraint 24 33 0.00001 posfloat
            /);

my $gravity_record = new FixedFormatRecord("Geoid separation", qw/
            station_id 7 16 - notrim:\S
            geoid_separation 71 79 - float
            /);

my $geoid_record = new FixedFormatRecord("Geoid separation", qw/
            station_id 7 16 - notrim:\S
            deflection_north 45 54 - float
            deflection_east 61 70 - float
            geoid_separation 71 79 - float
            /);
         


my $coord_format = {
        S => $geog_coord_record,
        N => $geog_coord_record,
        A => $zone_coord_record,
        I => $zone_coord_record,
        U => $zone_coord_record,
        V => $zone_coord_record,
        C => $proj_coord_record,
        D => $proj_coord_record,
        K => $proj_coord_record,
        P => $proj_coord_record,
        };

my $station_format = {
        station => $station_record,
        constraint => $constraint_record,
        geoid => $geoid_record,
        gravity=>$gravity_record,
        };
    
my $station_action = {
        4 => 'station',
        5 => 'station',
        6 => 'station',
        7 => 'station-astro',
       14 => 'constraint',
       15 => 'station-free',
       26 => 'gravity',
       28 => 'geoid-geoid',
       10 => 'switch-free',
       20 => 'switch-free',
       25 => 'switch-geoid',
       30 => 'switch-astro',
       40 => 'switch-end',
       19 => 'refrcoef'
       };

######################################################################
## Records used to read terrestrial observations

my $angle_format = new FixedFormatRecord("Direction observation", qw/
       user_code 4 6 - - 
       station_id_from 7 16 - notrim:\S
       station_id_to 31 40 - notrim:\S
       set_label 55 55 - -
       angle_deg 56 58 0 ^\d+$
       angle_min 60 61 0 ^\d\d?$
       angle_sec 62 70 0 posfloat
       precision 71 80 - posfloat?
       /);

my $cangle_format = new FixedFormatRecord("Angle observation", qw/
       user_code 4 6 - - 
       station_id_from 7 16 - notrim:\S
       station_id_ref 19 28 - notrim:\S
       station_id_to 31 40 - notrim:\S
       set_label 55 55 - -
       angle_deg 56 58 0 ^\d+$
       angle_min 60 61 0 ^\d\d?$
       angle_sec 62 70 0 posfloat
       precision 71 80 - posfloat?
       /);

my $angle_insthgt_format = new FixedFormatRecord("Zenith distance observation with heights", qw/
       user_code 4 6 - - 
       station_id_from 7 16 - notrim:\S
       inst_hgt_from 18 30 - float
       station_id_to 31 40 - notrim:\S
       inst_hgt_to 42 54 - float
       angle_deg 56 58 0 ^\d+$
       angle_min 60 61 0 ^\d\d?$
       angle_sec 62 70 0 posfloat
       precision 71 80 - posfloat?
       /);

my $distance_format = new FixedFormatRecord("Distance observation", qw/
       user_code 4 6 - - 
       station_id_from 7 16 - notrim:\S
       station_id_to 31 40 - notrim:\S
       value 56 70 - posfloat
       precision 71 80 - posfloat?
       /);

my $hgtdiff_format = new FixedFormatRecord("Height diff observation", qw/
       user_code 4 6 - - 
       station_id_from 7 16 - notrim:\S
       station_id_to 31 40 - notrim:\S
       value 56 70 - float
       precision 71 80 - posfloat?
       /);

my $elevation_format = new FixedFormatRecord("Elevation observation", qw/
       user_code 4 6 - - 
       station_id_from 7 16 - notrim:\S
       value 56 70 - float
       precision 71 80 - posfloat?
       /);

my $angle_precision_format = new FixedFormatRecord("Angle default precision", qw/
       subtype 5 5 0 [012]
       sdobs 8 15 0.0 posfloat
       sdinstplumb 30 34 0.0 posfloat
       sdtrgtplumb 35 39 0.0 posfloat
       /);

my $distance_precision_format = new FixedFormatRecord("Distance default precision", qw/
       subtype 5 5 0 [012]
       sdobsabs 8 14 0.0 posfloat
       sdobsppm 15 19 0.0 posfloat
       sdinstplumb 30 34 0.0 posfloat
       sdtrgtplumb 35 39 0.0 posfloat
       /);

my $azimuth_precision_format = new FixedFormatRecord("Azimuth default precision", qw/
       sdobs 8 14 0.0 posfloat
       sdaxislevelling 15 19 0.0 posfloat
       sdlongitude 20 24 0.0 posfloat
       sdinstplumb 30 34 0.0 posfloat
       sdtrgtplumb 35 39 0.0 posfloat
       /);

my $refrcoef_format = new FixedFormatRecord("Refraction coefficient", qw/
       refrcoef 4 10 - float
       /);

my $precision_format = {
       angle=>$angle_precision_format,
       dist=>$distance_precision_format,
       azimuth=>$azimuth_precision_format,
       };

my $override_precision_format = new FixedFormatRecord("Override precision", qw/
       overridetype 5 5 - [1234]
       /);

my $override_precision_mapping = {
       1 => 'angle0',
       2 => 'dist0',
       3 => 'angle3',
       4 => 'angle2',
       };

my $adjust_precision_format = new FixedFormatRecord("Override precision", qw/
       distorangle 5 5 - ^[16]?$
       scalevariance 8 14 1.0 posfloat
       addvariance 15 19 0.0 posfloat
       /);

my $adjust_precision_mapping = {
       '56-' => 'angle0',
       '56-1' => 'angle0',
       '56-6' => 'angle2',
       '57-' => 'dist0',
       '58-' => 'angle3',
       };

my $default_precision = {
       angle0=> new AutoHash( sdobs=>2.0, sdinstplumb=>0.2, sdtrgtplumb=>0.2 ),
       dist0=> new AutoHash(sdobsabs=>0.5, sdobsppm=>5, sdinstplumb=>0.2, sdtrgtplumb=>0.2 ),
       angle3=> new AutoHash( sdobs=>99.0, sdinstplumb=>0.2, sdtrgtplumb=>0.2 ),
       angle2=> new AutoHash( sdobs=>3, sdinstplumb=>0.2, sdtrgtplumb=>0.2 ),
       angle1=> new AutoHash( sdobs=>10, sdinstplumb=>0.2, sdtrgtplumb=>0.3 ),
       dist1=> new AutoHash(sdobsabs=>10.0, sdinstplumb=>0.0, sdtrgtplumb=>0.0 ),
       dist2=> new AutoHash(sdobsabs=>1.0, sdinstplumb=>0.0, sdtrgtplumb=>0.0 ),
       };


# Note: precisiontype also used to determine units (angle/distance) based on initial string
# and hence whether scaling is to apply.

my $obs_types = {
       direction_set => new AutoHash( type=>'direction_set', format=>$angle_format, precisiontype=>'angle0' ), 
       azimuth => new AutoHash( type=>'azimuth', format=>$angle_format, precisiontype=>'angle3' ), 
       slopedist => new AutoHash( type=>'slopedist', format=>$distance_format, precisiontype=>'dist0' ), 
       elldist => new AutoHash( type=>'elldist', format=>$distance_format, precisiontype=>'dist0' ), 
       msldist => new AutoHash( type=>'msldist', format=>$distance_format, precisiontype=>'dist0' ), 
       angle => new AutoHash( type=>'angle', format=>$cangle_format, precisiontype=>'angle2' ), 
       elevation => new AutoHash( type=>'elevation', format=>$elevation_format, precisiontype=>'dist1' ), 
       hgtdiff => new AutoHash( type=>'hgtdiff', format=>$hgtdiff_format, precisiontype=>'dist2' ), 
       zendist => new AutoHash( type=>'zendist', format=>$angle_format, precisiontype=>'angle1' ), 
       zendistih => new AutoHash( type=>'zendist', format=>$angle_insthgt_format, precisiontype=>'angle1' ), 
       }; 


######################################################################
## Stuff for spatial obs..

my $spatial_header_format = new FixedFormatRecord("Spatial data header", qw/
       description 31 44 - -
       /);

my $spatial_xyz_format = new FixedFormatRecord("Spatial XYZ record", qw/
       station_id 7 16 - notrim:\S
       station_name 17 30 - -
       station_class 32 32 - -
       x 40 52 - float
       y 53 65 - float
       z 66 78 - float
       /);

my $spatial_dxyz_format = new FixedFormatRecord("Spatial XYZ record", qw/
       station_id_from 7 16 - notrim:\S
       station_id_to 19 28 - notrim:\S
       x 40 52 - float
       y 53 65 - float
       z 66 78 - float
       /);

my $spatial_llh_format = new FixedFormatRecord("Spatial lat/lon/hgt record", qw/
       station_id 7 16 - notrim:\S
       station_name 17 30 - -
       station_class 32 32 - -
       lath 40 40 S [NS]
       latdeg 41 42 0 ^\d\d?$
       latmin 44 45 0 ^\d\d?$
       latsec 46 54 0.0 posfloat
       lonh 55 55 E [EW]
       londeg 56 58 0 ^\d\d?\d?$
       lonmin 60 61 0 ^\d\d?$
       lonsec 62 70 0.0 posfloat
       height 71 79 - float?
       /);

my $spatial_covariance_header = new FixedFormatRecord("Spatial covariance record", qw/
       matrixformat 7 11 - ^(UPPER)?$
       variancetype 17 22 - ^(WEIGHT)?$
       /);
       
my $spatial_scalarvarmult_format = new FixedFormatRecord("Spatial covariance scalar", qw/
       varmult 4 13 - posfloat
       /);
    
my $spatial_enuvarmult_format = new FixedFormatRecord("Spatial covariance differential scalar", qw/
       varmultn 4 8 - posfloat
       varmulte 9 13 - posfloat
       varmultu 14 18 - posfloat
       /);

my $spatial_enuaddsd_format = new FixedFormatRecord("Spatial covariance differential scalar", qw/
       sdaddn 4 8 - posfloat
       sdadde 9 13 - posfloat
       sdaddu 14 18 - posfloat
       /);

my $spatial_baselineppmerr_format = new FixedFormatRecord("Spatial baseline ppm error", qw/
       baselineppmerr 4 8 - posfloat
       /);

my $spatial_ellipsoid_format = new FixedFormatRecord("Spatial ellipsoid", qw/
       ellipsoid_id 5 6 0 ^([12]?\d|-1)$
       ellipsoid_name 7 25 - -
       ellipsoid_a 26 40 - posfloat
       ellipsoid_rf 41 50 - posfloat
       /);

my $spatial_record_type = {
       91=>new AutoHash( type=>'llh_position', refsys=>'P', format=>$spatial_llh_format, vcv_units=>'topo_m', vcv_eltsperline=>4, hgtoptional=>0 ),
       95=>new AutoHash( type=>'llh_position', refsys=>'P', format=>$spatial_llh_format, vcv_units=>'msec/cm', vcv_eltsperline=>4, hgtoptional=>1 ),
       195=>new AutoHash( type=>'llh_position', refsys=>'P', format=>$spatial_llh_format, vcv_units=>'msec/cm', vcv_eltsperline=>4, hgtoptional=>1 ),
       105=>new AutoHash( type=>'xyz_position', refsys=>'S', format=>$spatial_xyz_format, vcv_units=>'m', vcv_eltsperline=>3, hgtoptional=>0 ),
       108=>new AutoHash( type=>'llh_position', refsys=>'S', format=>$spatial_llh_format, vcv_units=>'topo_m', vcv_eltsperline=>3, hgtoptional=>0 ),
       121=>new AutoHash( type=>'llh_position', refsys=>'P', format=>$spatial_llh_format, vcv_units=>'topo_m', vcv_eltsperline=>3, hgtoptional=>0 ),
       125=>new AutoHash( type=>'llh_position', refsys=>'P', format=>$spatial_llh_format, vcv_units=>'msec/cm', vcv_eltsperline=>3, hgtoptional=>0 ),
       135=>new AutoHash( type=>'xyz_baseline', refsys=>'S', format=>$spatial_xyz_format, vcv_units=>'m', vcv_eltsperline=>3, hgtoptional=>0 ),
       141=>new AutoHash( type=>'llh_baseline', refsys=>'S', format=>$spatial_llh_format, vcv_units=>'topo_m', vcv_eltsperline=>3, hgtoptional=>0 ),
       115=>new AutoHash( type=>'dxyz_baseline', refsys=>'S', format=>$spatial_dxyz_format, vcv_units=>'cm', vcv_eltsperline=>3, hgtoptional=>0 ),
       };

######################################################################
# Note: fail to distinguish codes 2 and 5 (MSL (no geoid), and ellipsoidal arc distances).
# See newgan manual page 26

my $observation_action = {
       1 => 'obs-direction_set', 
       2 => 'obs-msldist', 
       3 => 'obs-azimuth',
       4 => 'obs-slopedist', 
       5 => 'obs-elldist', 
       6 => 'obs-angle', 
       11 => 'obs-elevation', 
       12 => 'obs-hgtdiff',
       13 => 'obs-zendist',
       113 => 'obs-zendistih',
       19 => 'refrcoef', 
       51 => 'precision-angle',
       52 => 'precision-dist',
       53 => 'precision-azimuth',
       54 => 'resetprecision-defaults',
       55 => 'overrideprecision',
       56 => 'adjustvariance-angles',
       57 => 'adjustvariance-distance',
       58 => 'adjustvariance-azimuths',
       59 => 'resetprecision-all',
       80 => 'donothing',
       82 => 'donothing',
       84 => 'donothing',

       91 => 'spatialobs', 
       95 => 'spatialobs', 
       105 => 'spatialobs', 
       108 => 'spatialobs', 
       121 => 'spatialobs', 
       125 => 'spatialobs', 
       135 => 'spatialobs', 
       141 => 'spatialobs', 
       115 => 'spatialobs', 

       104 => 'spatialstate-scalarvarmult',
       103 => 'spatialstate-enuvarmult',
       102 => 'spatialstate-enuaddsd',
       114 => 'spatialstate-baselineppmerr',
       130 => 'spatialstate-refsys',
       131 => 'spatialstate-ellipse',
  

       99 => 'end',
       };


######################################################################
## NewGan file parsing code ...

sub new {
   my( $class, $newganfile ) = @_;
   my $fh = new FileHandle( $newganfile ) || die "Cannot open NewGan file $newganfile\n";

   my $errors = [];
   my $warnings = [];

   my $self = bless { 
            filename=>$newganfile, 
            fh=>$fh, 
            refr_coefs=>[],
            errors=>$errors, 
            warnings=>$warnings,
            next_ellipsoid_id=>0,
            precisions=>new AutoHash,
            nprecision=>1,
            options=>new AutoHash,
            stations=>[],
            station_index=>{},
            got_geoid=>0,
            constraints=>[],
            observations=>[],
            obsprecision=>{},
            nobsprecision=>0,
            spatial_obs_state=>new AutoHash,
            crdsyslist=>{},
            referencesystems=>[],
            nreferencesystems=>0,
            ellipsoids=>[],
            }, $class;

   $self->Warn($warning);

   $self->ReadOptions;
   $self->Setup;
   $self->ReadStations;
   $self->ReadObservations;
 
   die @$errors if @$errors;

   # print "\nWarnings:\n",@$warnings if @$warnings;

   return $self;
   }


sub Error {
   my ($self,@errors) = @_;
   my $firsterror = shift(@errors);
   $firsterror .= " at ".$self->Location."\n";
   push(@{$self->errors},$firsterror,@errors);
   }

sub Warn {
   my($self,$warning) = @_;
   push(@{$self->warnings},$warning."\n");
   }

sub Warnings {
   my($self) = @_;
   return @{$self->warnings};
   }

sub ReadOptions {
   my($self) = @_;
   my $options = $self->options;
   $self->ReadOptionsRecord( $dimension_record );
   $self->ReadOptionsRecord( $title_record );
   $self->ReadOptionsRecord( $options_record );

   $self->ReadOptionsRecord( $line4_record ) if $options->ellipsoid_id eq '-1';
   if( $options->use_transform_params eq '1' ) {
      $self->ReadOptionsRecord( $line5_record );
      $self->ReadOptionsRecord( $refsys_transformation_record );
      }
       
   $self->ReadOptionsRecord( $line7_record ) if $options->max_obs_error eq -1;
   $self->ReadOptionsRecord( $line8_record ) if $options->plane_fo_option eq 'FO';
   }

sub ReadOptionsRecord {
   my( $self, $record ) = @_;
   return $self->ReadRecord( $record, $self->options );
   }


sub BuildEllipsoid {
   my ($self,$elldef) = @_;
   my $id = $elldef->ellipsoid_id;
   my $ellipsoid;
   if( $id >= 0 ) {
      $ellipsoid = $newgan_ellipsoid->{$id};
      if( ! $ellipsoid ) {
          $self->Error("Invalid ellipsoid id $id requested");
          }
      }
   else {
      my $code = sprintf("USER%02d",++$self->next_ellipsoid_id);
      eval {
         $ellipsoid = new Geodetic::Ellipsoid( $elldef->ellipsoid_a,
              $elldef->ellipsoid_rf, $code, $code );
         };
      if( $@ ) {
         $self->Error("Cannot build ellipsoid: $@");
         }
      }
   return $ellipsoid;
   }

sub Setup {
   my($self) = @_;
   # Set up the primary coordinate system ...
   my $ellipsoid = $self->BuildEllipsoid($self->options);

   my $name = $ellipsoid->name;
   my $code = $ellipsoid->code;
   my $datum = new Geodetic::Datum(
                  $name, 
                  $ellipsoid, $code,
                  undef,
                  $code
                  );
   $self->{crdsys} = new Geodetic::CoordSys( Geodetic::GEODETIC,
                  $name, $datum, undef, 
                  $code );
   $self->crdsyslist->{$code} = $self->crdsys;

   my $secellid = $self->options->secondary_ellipsoid_id;
   if( $secellid eq '' ) {
      $self->{secondary_ellipsoid} = $ellipsoid;
      }
   else {
      $self->{secondary_ellipsoid} = $self->BuildEllipsoid({ellipsoid_id=>$secellid});
      }
   }
   

sub ReadStations {
   my ( $self ) = @_;
   my $stations = $self->stations;
   my $station_index = $self->station_index;
   my $constraint;

   my $errors = $self->errors;

   my $mode = 'fixed';
   my $got_gravity = 0;
   my %unhandled = ();
  
   while( $mode ne 'end' && (my ($inrec,$rectype) = $self->GetNextRecord()) )
   {
      next if $rectype eq '';
      my $action = $station_action->{$rectype};
      my $recmode = '';
      $recmode = $1 if $action =~ s/\-(.*$)//;
      my $format = $station_format->{$action};

      if( $action eq 'switch' ) {
          $mode = $recmode;
          }
      elsif ( $recmode ne '' && $recmode ne $mode ) {
          $self->Error("Type $rectype record out of place");
          }
      elsif( $action eq 'station' ) {
          my $station = new NewGanFile::Station;
          $self->ParseRecord( $format, $inrec, $station );
          my $crdtype = $station->crdtype;
          if( $crdtype eq '' ) {
            $crdtype = $self->options->coordinate_output eq 'P' ? 'P' : 'S';
            $station->{crdtype} = $crdtype;
            }
          my $crdformat = $coord_format->{$crdtype};
          if( ! $crdformat ) {
              $self->Error("Invalid coordinate type $crdtype");
              }
          else {
             $self->ParseRecord($crdformat,$inrec,$station);
              }
          $station->{adjustment_mode} = $mode;
          if( $rectype == 15 ) {
             $station->{adjustment_mode} = 'float';
             $constraint = $self->GetConstraint() if ! $constraint;
             $station->{constraint} = $constraint;
             }
          $self->AddStation( $station );
          }
      elsif( $action eq 'constraint' ) {
          $constraint = $self->GetConstraint( $inrec );
          }
      elsif( $action eq 'geoid' || $action eq 'gravity' ) {
          my $gravity = $self->ParseRecord( $format, $inrec );
          my $gstation = $station_index->{$gravity->station_id};
          if( ! $gstation ) {
             $self->Error("Geoid data refers to undefined station");
             }
          else {
             foreach my $k (keys %$gravity) { $gstation->{$k} = $gravity->{$k}; }
             $self->{got_geoid} = 1;
             $got_gravity = 1 if $action eq 'gravity';
             }
          }
      elsif( $action eq 'refrcoef' ) {
          $self->ReadRefractionCoefficient( $inrec );
          }
      else {
          $unhandled{$rectype}++;
          }
   }
   if( $got_gravity ) {
       # Haven't figured out what to do with astro lat and lon entered separately in
       # 26 records (see newgan manual p21).  Presumably we should keep the astro lat and
       # lon fixed in an adjustment and recalc the deflection accordingly.  Not a 
       # feature in SNAP at present, so ignoring...
       $self->Warn("This program ignores deflection defined in 26 records");
       }
    foreach my $rt ( sort keys %unhandled ) {
       $self->Warn( $unhandled{$rt}." records of type $rt not handled in stationsection");
       }
          
   }

# Building a coordinate system .. assumes that the coordinate system is
# based on the primary datum.

sub GetCoordsys {
   my( $self, $code ) = @_;
   if( ! exists $self->crdsyslist->{$code} ) {
       my $basecrdsys = $self->crdsys;
       my $basecode = $basecrdsys->code;
       my $pcrdsys;
       if( $code =~ /^$basecode\_(AMG|UTM)(\d\d)([NS])/ ) {
           my $cm = $2*6-183;
           my $fn = ($3 eq 'N') ?  0 : 10000000.0;
           my $proj = new Geodetic::TMProjection($basecrdsys->ellipsoid,
              $cm, 0.0, 0.9996, 500000.0, $fn, 1.0);
           $pcrdsys = new Geodetic::CoordSys( &Geodetic::PROJECTION,
              $code, $basecrdsys->datum, $proj, $code );
           }
       else {
           $self->Error("Cannot handle coordinate system $code\n");
           }
       $self->crdsyslist->{$code} = $pcrdsys;
       }
   return $self->crdsyslist->{$code};
   }

sub AddStation {
   my( $self, $station ) = @_;
   # Derive the coordinate system... see page 22..
   my $basecode = $self->crdsys->code;
   my $crdtype = $station->crdtype;
   my $projdef = $newgan_projection->{$crdtype};
   my $scrdsys;
   if( ! defined $projdef ) {
       $self->Error("Invalid coordinate type $crdtype");
       }
   else 
       {
       $station->{ordtype} = $projdef->ordtype;
       my $projcode = $projdef->projcode;
       $projcode =~ s/\{(\w+)\}/$station->{$1}/eg;
       $scrdsys = $self->GetCoordsys( $basecode.$projcode );
       }
   # Construct a coordinate object from the station ..
   if( $scrdsys ) {
       if( $station->ordtype eq 'GEOG' ) {
          my $lat = $station->latdeg+$station->latmin/60.0 + $station->latsec/3600.0;
          $lat = -$lat if $station->lath eq 'S';
          my $lon = $station->londeg+$station->lonmin/60.0 + $station->lonsec/3600.0;
          $lon = -$lon if $station->lonh eq 'W';
          $station->{coord} = $scrdsys->coord( $lat, $lon, $station->elevation );
          }
       else {
          $station->{coord} = $scrdsys->coord( $station->northing, $station->easting, $station->elevation );
          }
       
       }
   push(@{$self->stations}, $station );
   $self->station_index->{$station->station_id} = $station;
   }

sub HaveStation {
   my($self,$station_id) = @_;
   return $self->station_index->{$station_id};
   }

sub StationList {
   my($self) = @_;
   return map {$_->station_id} @{$self->stations};
   }

sub GetStation {
   my($self,$station_id) = @_;
   return $self->HaveStation($station_id);
   }

sub GetConstraint {
  my($self,$inrec) = @_;
  my $constraint = $self->ParseRecord( $constraint_record, $inrec );
  push(@{$self->constraints},$constraint);
  $constraint->{id} = scalar(@{$self->constraints});
  return $constraint;
  }


sub ReadObservations {
   my ($self) = @_;

   my $observations = $self->observations;

   my $mode = '';
   my %unhandled =();

   my %overrideprecision;
   my %adjustprecision;

   my $direction_set = undef;
  
   while( my ($inrec,$rectype) = $self->GetNextRecord() )
   {
      next if $rectype eq '';
      my $action = $observation_action->{$rectype} || 'invalid';
      last if $action eq 'end';
      next if $action eq 'donothing';

      my $recmode = '';
      $recmode = $1 if $action =~ s/\-(.*$)//;

      $direction_set = undef if $action ne 'obs';
      if( $action eq 'obs' ) {
    
           my $obstype = $obs_types->{$recmode};
           my $format = $obstype->format;
           my $obs = new NewGanFile::ScalarObs;
           $self->ParseRecord($format,$inrec,$obs);
           $obs->{source_line} = $self->fh->input_line_number;
           $obs->{type} = $obstype->type;
           $obs->{obstype} = $obstype;
           $obs->{category} = 'scalar';
           if( $obs->type eq 'direction_set' &&
               $direction_set && 
               $direction_set->station_id_from eq $obs->station_id_from &&
               $direction_set->set_label eq $obs->set_label ) {
               $direction_set->{next} = $obs;
               }
           else {
               push(@$observations, $obs );
               }
           $direction_set = $obs if $obs->type eq 'direction_set';

           $obs->{refraction_coef} = $self->GetRefractionCoefficient 
             if $obs->type =~ /^zendist/;

           if( $overrideprecision{$obstype->precisiontype} ) { $obs->{precision} = ''; }
           if( $obs->precision eq '' ) {
             $obs->{default_precision} = $self->GetPrecision( $obstype->precisiontype );
             }
           if( my $adjprecision = $adjustprecision{$obstype->precisiontype} ) {
             $obs->{scalevariance} = $adjprecision->scalevariance;
             $obs->{addvariance} = $adjprecision->addvariance;
             }
                 
           my @missing_stations;
           if( ! $self->HaveStation($obs->station_id_from) ) {
              push(@missing_stations,$obs->station_id_from);
              }
           if( $obs->has('station_id_to') &&
               ! $self->HaveStation($obs->station_id_to) ) {
              push(@missing_stations,$obs->station_id_to);
              }
           if( $obs->has('station_id_ref')  &&
               ! $self->HaveStation($obs->station_id_ref) ) {
              push(@missing_stations,$obs->station_id_ref);
              }
           if( @missing_stations ) {
             $self->Error("Undefined station id (".join(', ',@missing_stations).")");
             }
           }

       elsif( $action eq 'spatialstate' ) {
           $self->ReadSpatialObsState( $recmode, $inrec );
           }
       elsif( $action eq 'spatialobs' ) {
           $self->ReadSpatialObs( $rectype, $inrec );
           }
       elsif( $action eq 'refrcoef' ) {
           $self->ReadRefractionCoefficient( $inrec );
           }
       elsif( $action eq 'precision' ) {
           $self->ReadPrecision( $recmode, $inrec );
           }
       elsif( $action eq 'overrideprecision' ) {
           my $definition = $self->ParseRecord( $override_precision_format, $inrec );
           my $obstype = $override_precision_mapping->{$definition->overrridetype};
           $overrideprecision{$obstype} = 1;
           }
       elsif( $action eq 'adjustprecision' ) {
           my $definition = $self->ParseRecord($adjust_precision_format, $inrec );
           my $obstype = $adjust_precision_mapping->{$rectype.'-'.$definition->distorangle};
           $adjustprecision{$obstype} = $definition;
           }
       elsif( $action eq 'resetprecision' ) {
           $self->ResetDefaultPrecisions;
           if( $recmode eq 'all' ) {
               %adjustprecision = ();
               %overrideprecision = ();
               }
           }
       else {
           $unhandled{$rectype}++;
           }
       }
    foreach my $rt ( sort keys %unhandled ) {
       $self->Warn($unhandled{$rt}." records of type $rt not handled in observation section");
       }
          
   }

sub CreateRefsys {
   my ($self) = @_;
   my $state = $self->spatial_obs_state;
   $state->{nreferencesystems}++;
   my $refsys = new AutoHash( id=>'REFSYS'.sprintf("%03d",$self->nreferencesystems));
   push(@{$self->referencesystems},$refsys);
   $state->{refsys} = $refsys;
   return $refsys;
   }

sub GetRefsys {
   my ( $self ) = @_;
   my $state = $self->spatial_obs_state;

   # If a reference system is not defined, use the default system
   if( ! $state->has('refsys') ) {
      my $refsys = $self->CreateRefsys($state);
      $refsys->{tfm_dx} = $self->get('tfm_dx',0.0);
      $refsys->{tfm_dy} = $self->get('tfm_dy',0.0);
      $refsys->{tfm_dz} = $self->get('tfm_dz',0.0);
      $refsys->{tfm_rx} = $self->get('tfm_rx',0.0);
      $refsys->{tfm_ry} = $self->get('tfm_ry',0.0);
      $refsys->{tfm_rz} = $self->get('tfm_rz',0.0);
      $refsys->{tfm_scale} = $self->get('tfm_scale',0.0);
      }
   return $state->refsys;
   }

sub CreateEllipsoid {
   my ($self, $ellipsoid ) = @_;
   my $state = $self->spatial_obs_state;
   push(@{$self->ellipsoids},$ellipsoid);
   $state->{ellipsoid} = $ellipsoid;
   return $ellipsoid;
   }

sub GetEllipsoid {
   my ( $self ) = @_;
   my $state = $self->spatial_obs_state;
   if( ! $state->ellipsoid ) {
      $state->{ellipsoid} = $self->CreateEllipsoid( $self->secondary_ellipsoid );
      }
   return $state->ellipsoid;
   }

sub ReadSpatialObsState {
   my ($self,$recmode,$inrec ) = @_;
   my $state = $self->spatial_obs_state;

   if( $recmode eq 'scalarvarmult' ) {
       $self->ParseRecord( $spatial_scalarvarmult_format, $inrec, $state );
       }
   elsif( $recmode eq 'enuvarmult' ) {
       $self->ParseRecord( $spatial_enuvarmult_format, $inrec, $state );
       }
   elsif( $recmode eq 'enuaddsd' ) {
       $self->ParseRecord( $spatial_enuaddsd_format, $inrec, $state );
       }
   elsif( $recmode eq 'baselineppmerr' ) {
       $self->ParseRecord( $spatial_baselineppmerr_format, $inrec, $state );
       }
   elsif( $recmode eq 'refsys' ) {
       $self->refsysid++;
       my $refsys = $self->CreateRefsys();
       ($inrec) = $self->GetNextRecord();
       $self->ParseRecord( $refsys_transformation_record, $inrec, $refsys );
       }
   elsif( $recmode eq 'ellipse' ) {
       my $elldef =  $self->ParseRecord( $spatial_ellipsoid_format, $inrec );
       $elldef->{ellipsoid_id} = -1 if $self->{ellipsoid_id} == 0;
       my $ellipsoid = $self->BuildEllipsoid( $elldef );
       $self->CreateEllipsoid( $ellipsoid );
       }
   else {
       die "Invalid mode in call to ReadSpatialObsState";
       }
   }

sub ReadSpatialObs {
   my( $self, $rectype, $inrec ) = @_;
   my $state = $self->spatial_obs_state;
   my $baserectype = $rectype;
   my $spatialtype = $spatial_record_type->{$rectype};
   if( ! $spatialtype ) {
       $self->Error("Invalid spatial record type $baserectype passed to ReadSpatialObs");
       return;
       }

   my $obs = new NewGanFile::VectorObs;
   $self->ParseRecord( $spatial_header_format, $inrec, $obs );
   $obs->{user_code} = '';
   $obs->{source_line} = $self->fh->input_line_number;
   $obs->{category} = 'vector';

   push( @{$self->observations}, $obs );

   $obs->{type} = $spatialtype->type;
   if( $spatialtype->refsys eq 'S' ) {
       $obs->{refsys} = $self->GetRefsys;
       }
   if( $spatialtype->{type} =~ /^llh/ ) {
       $obs->{ellipsoid} = $self->GetEllipsoid;
       }

   my $components = $obs->{components} = [];

   my $format = $spatialtype->format;
   my $vcvelts = $spatialtype->vcv_eltsperline;

   my $obsrectype = $rectype+1;
   my $vcvrectype = $rectype+2;
   my $hgtoptional = $spatialtype->hgtoptional;
   my $havehgt = 0;

   while( ($inrec,$rectype) = $self->GetNextRecord ) {
      last if $rectype != $obsrectype;
      my $component = $self->ParseRecord($format,$inrec);
      if( $component->has('station_id') && ! $self->HaveStation($component->station_id) ) {
          # Can add station to list if it is a position equation
          if( $spatialtype->type eq 'llh_position' && $spatialtype->refsys eq 'P' ) {
             my $station = new NewGanFile::Station;
             $self->ParseRecord($format,$inrec,$station);
             $station->{adjustment_mode} = 'free';
             $station->{crdtype} = $station->lath;
             $station->{elevation} = $station->{height};
             $self->AddStation( $station );
             }
          else {
             $self->Error("Invalid station id (".$component->station_id.")");
             }
          }
      if( $component->has('station_id_from') && 
             ! $self->HaveStation($component->station_id_from) ) {
             $self->Error("Invalid station id (".$component->station_id_from.")");
             }
      if( $component->has('station_id_to') && 
             ! $self->HaveStation($component->station_id_to) ) {
             $self->Error("Invalid station id (".$component->station_id_to.")");
             }
      if( exists $component->{height} ) {
          if( $hgtoptional ) {
             $havehgt |= $component->height eq '' ? 1 : 2;
             }
          elsif( $component->{height} eq '' ) {
             $self->Error("Height component missing");
             }
          }
      push(@$components,$component);
      }
   if( $havehgt == 3 ) {
      $self->Error("Inconsistent dimension of adjustment points");
      }
   
   if( @$components == 0 ) {
      $self->Error("No observations $obsrectype in $baserectype group");
      return;
      }

   if( $rectype != $vcvrectype ) {
      $self->Error("Missing covariance type $vcvrectype in $baserectype group");
      return;
      }

   my $vcv = $obs->{vcv} = $self->ParseRecord( $spatial_covariance_header, $inrec );
   $vcv->{units} = $spatialtype->vcv_units;

   #Scaling applied to VCV
   $vcv->{varmult} = $state->get('varmult',1.0);
   my $usedifferential = $vcvrectype =~ /^(93|97|197)$/;
   $vcv->{varmultn} = $usedifferential ? ($state->get('varmultn',1.0)) : 1.0;
   $vcv->{varmulte} = $usedifferential ? ($state->get('varmulte',1.0)) : 1.0;
   $vcv->{varmultu} = $usedifferential ? ($state->get('varmultu',1.0)) : 1.0;

   $vcv->{sdaddn} = $usedifferential ? ($state->get('sdaddn',0.0)) : 0.0;
   $vcv->{sdadde} = $usedifferential ? ($state->get('sdadde',0.0)) : 0.0;
   $vcv->{sdaddu} = $usedifferential ? ($state->get('sdaddu',0.0)) : 0.0;

   $vcv->{baselineppmerr} = $state->get('baselineppmerr',0.0);
 
   #Calculate number of elements and read them in
   my $elements = $vcv->{elements} = {};
   my $nrow = scalar(@$components);
   $nrow-- if $spatialtype->{type} =~ /^(xyz|llh)\_baseline$/;
   my $ndim = $havehgt == 1 ? 2 : 3;
   $nrow *= $ndim;
   $vcv->{nrow} = $nrow;
   $vcv->{ndim} = $ndim;
   my ($nreadrows, $nreadcols);
   my $upper = $vcv->matrixformat eq 'UPPER';
   my $nread = $upper ? ($nrow*($nrow+1))/2 : $nrow*$nrow;
   my $r = 0;
   my $c = $nrow;
   my $pcolmax = $vcvelts*20;
   my $pcol = $pcolmax;
   # Read matrix so that it is can be strored with lower triangle indices..
   for( my $iread = 0; $iread < $nread; $iread++ ) {
      $c++;
      if( $c > $nrow ) {
         $r++;
         $c = $upper ? $r : 1;
         if( $upper ) { $pcol = $pcolmax; }
         }
      if( $pcol >= $pcolmax ) {
         $inrec = $self->fh->getline; 
         $pcol = 0;
         }
      my $number = substr($inrec,$pcol,20);
      $pcol += 20;
      $self->Error("Invalid covariance component $number")
             if $number !~ /^\s*[\-\d\.efg]+\s*$/;
      $elements->{"$c,$r"} = $number+0;
      }
   
   }

sub ReadPrecision {
   my ($self,$precisiontype,$inrec) = @_;
   my $format = $precision_format->{$precisiontype};
   my $precision = $self->ParseRecord( $format, $inrec );
   $precisiontype .= $precision->subtype;
   $self->obsprecision->{$precisiontype} = $precision;
   $self->nobsprecision++;
   $precision->{id} = $self->nobsprecision;
   return $precision;
   
   }

sub GetPrecision {
   my ($self,$precisiontype) = @_;
   my $precision = $self->obsprecision->{$precisiontype};
   if( ! $precision ) {
       $precision = $self->obsprecision->{$precisiontype} = $default_precision->{$precisiontype};
       $self->{nprecision}++;
       $precision->{id} = $self->nprecision;
       }
   return $precision;
   }

sub ResetDefaultPrecisions {
   my($self) = @_;
   $self->{obsprecision} = {};
   }

sub ReadRefractionCoefficient {
  my($self,$inrec) = @_;
  my $refcoef = $self->ParseRecord( $refrcoef_format, $inrec );
  push(@{$self->refrcoefs},$refcoef);
  $refcoef->{id} = sprintf("REFCOEF%02d",scalar(@{$self->refrcoefs}));
  return $refcoef;
  }

sub GetRefractionCoefficient {
  my( $self ) = @_;
  return $self->refrcoefs->[-1] if scalar(@{$self->refrcoefs}) > 0;
  return $self->ReadRefractionCoefficient('   0.07');
  }

sub ReadRecord {
   my ( $self, $record, $data ) = @_;
   my $fh = $self->fh;
   my ($inrec) = $self->GetNextRecord($fh);
   return undef if ! $inrec;
   return $self->ParseRecord($record,$inrec,$data);
   }

sub ParseRecord {
   my ( $self, $record, $inrec, $data ) = @_;
   my @errors;
   ($data,@errors) = $record->read($inrec,$data);
   if( @errors ) {
      $self->Error("Errors",@errors);
      }
   return $data;
   }

sub GetNextRecord {
   my($self) = @_;
   my $fh = $self->fh;
   my $inrec;
   while($inrec = <$fh>) { 
       $inrec =~ s/^NOPAUSE// if $fh->input_line_number == 1;
       # Don't return blank lines or comments..
       last if $inrec !~ /^(\s*$|\*|\-)/;
       }
   return if ! $inrec;

   my $rectype = substr($inrec,0,3);
   $rectype =~ s/^0+//;
   $rectype =~ s/^\s+//;
   $rectype =~ s/\s+$//;

   return ($inrec,$rectype);
   }

sub Location {
   my ($self) = @_;
   return "line ".$self->fh->input_line_number." of ".$self->filename;
   }

1;
