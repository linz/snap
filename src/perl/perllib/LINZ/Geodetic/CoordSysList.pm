#===============================================================================
# Module:             CoordSysList.pm
#
# Description:       Defines packages:
#                      LINZ::Geodetic::CoordSysList
#
# Dependencies:      Uses the following modules:
#                      Geodetic
#                      LINZ::Geodetic::BursaWolf
#                      LINZ::Geodetic::CoordSys
#                      LINZ::Geodetic::Ellipsoid
#                      LINZ::Geodetic::Projection
#                      LINZ::Geodetic::Datum
#
#  $Id: CoordSysList.pm,v 1.6 2005/11/27 19:39:29 gdb Exp $
#
#  $Log: CoordSysList.pm,v $
#  Revision 1.6  2005/11/27 19:39:29  gdb
#  *** empty log message ***
#
#  Revision 1.5  2004/06/16 23:56:43  ccrook
#  Updated definition of datum to allow comments at end of gmrid definition
#
#  Revision 1.4  2000/10/24 02:38:15  ccrook
#  Tidied handling of ellipsoid codes.
#
#  Revision 1.3  2000/10/15 18:55:14  ccrook
#  Modification to coordsys functions to allow them to use a distortion grid
#  definition for coordinate conversions.
#
#  Revision 1.2  1999/09/28 01:41:14  ccrook
#  Fixed error in reading reference frame BursaWolf parameters
#
#  Revision 1.1  1999/09/09 21:09:36  ccrook
#  Initial revision
#
#
#===============================================================================

use strict;

#===============================================================================
#
#   Class:       LINZ::Geodetic::CoordSysList
#
#   Description:  This routine manages a list of coordinates systems (as
#                 defined in a "coordsys.def" file.  The routine loads the
#                 the text definitions of ellipsoids, datums, and
#                 coordinate systems, but only parses them as required.
#                 Each is identified by a code (eg WGS84).
#
#                 The CoordSysList is a hash with the following components
#                   filename     The name of the source file
#                   ellipsoids   Hash storing ellipsoid definitions
#                   datums       Hash storing datum definitions
#                   crdsystems   Hash storing crdsystem definitions
#                   vdatums      Hash storing vertical datum definitions
#                   heightcoord  Hash storing height coordinate systems definitions
#                 The definitions are stored in hashes indexed by the
#                 the codes for the objects.  Each hash value is itself
#                 a hash with keys 'def' and 'object'.  Once the object
#                 has been constructed a reference is stored in the 'object'
#                 element so that it does not need constructing a second
#                 time.
#
#                 Defines the following routines:
#                 Constructor
#                  $cslist = new LINZ::Geodetic::CoordSysList($filename)
#
#                 Functions to retrieve objects from the list
#                  $ellipsoid = $cslist->ellipsoid($elpcode)
#                  $datum = $cslist->datum($dtmcode)
#                  $cs = $cslist->coordsys($cscode)
#                  $csname =$cslist->coordsysname($cscode)
#                  $vdatums =$cslist->vdatums($vdcode);
#                  $heightcoord=$cslist->heightcoord($vdcode);
#
#===============================================================================

package LINZ::Geodetic::CoordSysList;
require Exporter;
our @ISA=qw(Exporter);
our @EXPORT_OK=qw(GetCoordSys);

use Time::JulianDay;
require LINZ::Geodetic;

# Note: requires for other components are in subs below to delay loading
# until actually require.

my %cstype = (
    GEODETIC   => &LINZ::Geodetic::GEODETIC,
    CARTESIAN  => &LINZ::Geodetic::CARTESIAN,
    GEOCENTRIC  => &LINZ::Geodetic::CARTESIAN,
    PROJECTION => &LINZ::Geodetic::PROJECTION
);

our $DefaultCoordSysFile=undef;
our $DefaultCoordSysEnv='COORDSYSDEF';
our @PossibleCoordSysDef=(
    '/usr/share/linz/coordsys/coordsys.def',
    # Insert other operating system possible locations.
    );

our $_cslist=undef;

sub coalesce
{
    my( $value, $default ) = @_;
    return defined($value) ? $value : $default;
}

#===============================================================================
#  Function:    GetCoordSys
#
#  Function to get a coordinate system definition from a code using the default
#  coordinate system definitions
#===============================================================================

sub GetCoordSys
{
    my($cscode)=@_;
    $_cslist ||= LINZ::Geodetic::CoordSysList->newFromCoordSysDef();
    return $_cslist->coordsys($cscode);
}

#===============================================================================
#
#   Method:       new
#
#   Description:  $cslist= new LINZ::Geodetic::CoordSysList($elldefs,$dtmdefs,$csdefs)
#
#   Parameters:   $elldefs   Hash reference to lookup ellipoid definitions
#                 $dtmdefs   Hash reference to lookup datum definitions
#                 $csdefs    Hash reference to lookup coord sys definitions
#
#   Returns:      A coordinate system list object
#
#===============================================================================

sub new
{
    my ( $class, $elldefs, $dtmdefs, $csdefs, $vddefs ) = @_;
    my $ellipsoids = {};
    my $datums     = {};
    my $crdsystems = {};
    my $vdatums    = {};

    if ($elldefs)
    {
        foreach ( keys %$elldefs )
        {
            $ellipsoids->{$_} = { code => $_, def => $elldefs->{$_} };
        }
    }
    if ($dtmdefs)
    {
        foreach ( keys %$dtmdefs )
        {
            $datums->{$_} = { code => $_, def => $dtmdefs->{$_} };
        }
    }
    if ($csdefs)
    {
        foreach ( keys %$csdefs )
        {
            $crdsystems->{$_} = { code => $_, def => $csdefs->{$_} };
        }
    }
    if ($vddefs)
    {
        foreach ( keys %$vddefs )
        {
            $vdatums->{$_} = { code => $_, def => $vddefs->{$_} };
        }
    }

    my $self = {
        ellipsoids => $ellipsoids,
        datums     => $datums,
        crdsystems => $crdsystems,
        vdatums    => $vdatums,
    };
    return bless $self, $class;
}

#===============================================================================
#
#   Method:       newFromCoordSysDef
#
#   Description:  $cslist = new LINZ::Geodetic::CoordSysList($filename)
#
#   Parameters:   $filename  The name of the coordinate system definition file
#
#   Returns:      A coordinate system list object
#
#===============================================================================

sub newFromCoordSysDef
{
    my ( $class, $filename ) = @_;
    # Default file name
    if( ! $filename )
    {
        $filename = $LINZ::Geodetic::CoordSysList::DefaultCoordSysFile
               ||= $ENV{$LINZ::Geodetic::CoordSysList::DefaultCoordSysEnv};
        if( ! $filename || ! -f $filename )
        {
            foreach my $f (@LINZ::Geodetic::CoordSysList::PossibleCoordSysDef)
            {
                if( -f $f )
                {
                    $filename=$f;
                    last;
                }
            }
        }
        if( ! $filename || ! -f $filename )
        {
            die "Cannot find default coordsys.def file.\n";
        }
    }
    local (*CSFILE);
    open( CSFILE, $filename )
      || die "Cannot open coordinate system file $filename.\n";
    my $ellipsoids = {};
    my $datums     = {};
    my $crdsystems = {};
    my $vdatums    = {};
    my $vdatums2   = {};
    my $list;

    while (<CSFILE>)
    {
        next if /^\s*[\#\!]/;
        next if /^\s*$/;
        s/\s+$//;
        if (/^\s*\[/)
        {
            $list = undef;
            if (/^\s*\[\s*(\S+)\s*\]\s*$/)
            {
                my $section = uc($1);
                if ( $section eq 'ELLIPSOIDS' )
                {
                    $list = $ellipsoids;
                }
                elsif ( $section eq 'DATUMS' || $section eq 'REFERENCE_FRAMES' )
                {
                    $list = $datums;
                }
                elsif ( $section eq 'COORDINATE_SYSTEMS' )
                {
                    $list = $crdsystems;
                }
                elsif ( $section eq 'VERTICAL_DATUMS' )
                {
                    $list = $vdatums;
                }
                # Handling of deprecated height surfaces reference 
                elsif ( $section eq 'HEIGHT_REFERENCE_SURFACES' || $section eq 'HEIGHT_REFERENCES' )
                {
                    $list = $vdatums;
                }
                elsif ( $section eq 'HEIGHT_COORDINATE_SYSTEMS' )
                {
                    $list = $vdatums2;
                }
            }
            next;
        }
        next if !$list;
        # Handle continuation lines
        while (/(\&|\\)$/)
        {
            $_ = $` . <CSFILE>;
            s/\s*$//;
        }
        next if ! /^\s*(\S+)\s+(\S.*?)\s*$/;
        my $codestr=$1;
        my $codedef=$2;
        my @codes=($codestr);
        if( $codestr =~ /^((?:^|=)(?:\([^\)]+\)|[^\(\)\=]+))+$/ )
        {
            @codes = $codestr=~ /[^\(\)\=]+/g;
        }
        foreach my $c (@codes)
        {
            $list->{uc($c)} = $codedef;
        }
    }
    close(CSFILE);

    # Handling of deprecated height surfaces reference 
    foreach my $k (keys %$vdatums2)
    {
        $vdatums->{$k} = $vdatums2->{$k} if ! exists $vdatums->{$k};
    }

    my $self =
      new( $class, $ellipsoids, $datums, $crdsystems, $vdatums );

    $self->{filename} = $filename;

    return bless $self, $class;
}

#===============================================================================
#
#   Method:       definitions
#
#   Description:  ($elldef,$dtmdef,$csdef,$hrdef) = $cslist->definitions;
#
#   Parameters:   None
#
#   Returns:      Returns five hash references containing the coordinate
#                 system definition strings
#
#===============================================================================

sub definitions
{
    my ($self) = @_;
    my ( %elldef, %dtmdef, %csdef, %hrdef );
    foreach ( values %{ $self->{ellipsoids} } )
    {
        $elldef{ $_->{code} } = $_->{def};
    }
    foreach ( values %{ $self->{datums} } ) {
        $dtmdef{ $_->{code} } = $_->{def};
    }
    foreach ( values %{ $self->{crdsystems} } ) {
        $csdef{ $_->{code} } = $_->{def};
    }
    foreach ( values %{ $self->{vdatums} } ) {
        $hrdef{ $_->{code} } = $_->{def};
    }
    return ( \%elldef, \%dtmdef, \%csdef, \%hrdef );
}

#===============================================================================
#
#   Method:       ellipsoid
#
#   Description:  $ellipsoid = $cslist->ellipsoid($elpcode)
#
#   Parameters:   $elpcode    The code of the ellipsoid required
#
#   Returns:      Returns an ellipsoid object
#
#===============================================================================

sub ellipsoid
{
    my ( $self, $elpcode ) = @_;
    return sort keys %{ $self->{ellipsoids} } if $elpcode eq '';
    $elpcode = uc($elpcode);
    my $elpdef = $self->{ellipsoids}->{$elpcode};
    die "Invalid ellipsoid code $elpcode.\n" if !defined $elpdef;
    my $elp = $elpdef->{object};
    if ( !$elp )
    {
        die "Invalid definition of ellipsoid $elpcode.\n"
          if $elpdef->{def} !~ /^\"([^\"]+)\"\s+(\d+\.?\d*)\s+(\d+\.?\d*)\s*$/;
        require LINZ::Geodetic::Ellipsoid;
        $elpdef->{object} = $elp =
          new LINZ::Geodetic::Ellipsoid( $2, $3, $1, $elpcode );
    }
    return $elp;
}

#===============================================================================
#
#   Method:       datum
#
#   Description:  $datum = $cslist->datum($dtmcode)
#
#   Parameters:   $dtmcode     The code of the datum object required
#
#   Returns:      The datum object.  Note that a separate datum object is
#                 produced for each epoch if the datum includes a deformation
#                 model
#
#===============================================================================

sub datum
{
    my ( $self, $dtmcode, $refepoch, $usedcodes ) = @_;
    return sort keys %{ $self->{datums} } if $dtmcode eq '';
    $dtmcode = uc($dtmcode);
    my $dtmdef = $self->{datums}->{$dtmcode};
    die "Invalid datum code $dtmcode.\n" if !defined $dtmdef;
    my $dtm = $dtmdef->{object};
    return $dtm if $dtm && ( !$refepoch || !$dtm->defmodel );

    my $objectkey = 'object';
    $objectkey .= sprintf( '@%.4f', $refepoch ) if $refepoch;
    $dtm = $dtmdef->{$objectkey};
    return $dtm if $dtm;

    # This is not comprehensive datum definition regular expression -
    # other possibilities include grid and definition
    die "Invalid definition of datum $dtmcode.\n"
      if $dtmdef->{def} !~ /^
                         \"([^\"]+)\"\s+
                         ellipsoid\s+(\S+)\s+
                         (\w+)
                         (?:
                         \s+
                         (?:(iers|iers_tsr|iers_etsr\s+\d{4}(?:\.\d*)?)\s+)?
                         ([+-]?\d+\.?\d*)\s+
                         ([+-]?\d+\.?\d*)\s+
                         ([+-]?\d+\.?\d*)\s+
                         ([+-]?\d+\.?\d*)\s+
                         ([+-]?\d+\.?\d*)\s+
                         ([+-]?\d+\.?\d*)\s+
                         ([+-]?\d+\.?\d*)
                         (?:
                         \s+
                         (?:rates
                         \s+(\d{4}(?:\.\d*)?)\s+
                         )?
                         ([+-]?\d+\.?\d*)\s+
                         ([+-]?\d+\.?\d*)\s+
                         ([+-]?\d+\.?\d*)\s+
                         ([+-]?\d+\.?\d*)\s+
                         ([+-]?\d+\.?\d*)\s+
                         ([+-]?\d+\.?\d*)\s+
                         ([+-]?\d+\.?\d*)
                         )?
                         )?
                         (?:
                           \s+grid\s+(snap2d)\s+(\S+)\s+(\"[^\"]*\")
                         )?
                         (?:
                             \s+deformation\s+
                             (?:
                                (?:(velgrid)\s+(\S+)\s+(\d{4}(?:\.\d*)?)) |
                                (?:(linzdef)\s+(\S+(?:\s+\S+)?)) |
                                (?:(bw14)\s+(\d{4}(?:\.\d*)?)\s+
                                     ([+-]?\d+\.?\d*)\s+
                                     ([+-]?\d+\.?\d*)\s+
                                     ([+-]?\d+\.?\d*)\s+
                                     ([+-]?\d+\.?\d*)\s+
                                     ([+-]?\d+\.?\d*)\s+
                                     ([+-]?\d+\.?\d*)\s+
                                     ([+-]?\d+\.?\d*)
                                     ) |
                                (?:(euler)\s+(\d{4}(?:\.\d*)?)\s+
                                     ([+-]?\d+\.?\d*)\s+
                                     ([+-]?\d+\.?\d*)\s+
                                     ([+-]?\d+\.?\d*)
                                     ) |
                                none)
                            )?
                         \s*$/xi;

    my (
        $name,     $ellcode,     $baserf,     $iers,        
        $tx,       $ty,          $tz,          
        $rx,       $ry,          $rz,
        $sf,       $refy,        
        $dtx,        $dty,         $dtz,
        $drx,      $dry,         $drz,        
        $dsf,         
        $gridtype, $gridfile, $griddesc,    
        $velgrid,    $velgridfile, $velgriddesc,
        $linzdef,  $linzdeffile, 
        $bw14,       $bw14epoch,   
        $bw14dtx, $bw14dty,  $bw14dtz,     
        $bw14drx,    $bw14dry,     $bw14drz,
        $bw14dsf,  
        $euler,   $eulerepoch, $eulerplon,   $eulerplat, $eulerrate
      )
      = (
        $1,        $2,        uc(coalesce($3,'')),    uc(coalesce($4,'')),    
        coalesce($5,0.0)  + 0.0,  coalesce($6,0.0) + 0.0, coalesce($7,0.0) + 0.0,  
        coalesce($8,0.0) + 0.0,  coalesce($9,0.0) + 0.0,  coalesce($10,0.0) + 0.0, 
        coalesce($11,0.0) + 0.0, 
        coalesce($12,''),
        coalesce($13,0.0) + 0.0, coalesce($14,0.0) + 0.0, coalesce($15,0.0) + 0.0, 
        coalesce($16,0.0) + 0.0, coalesce($17,0.0) + 0.0, coalesce($18,0.0) + 0.0,
        coalesce($19,0.0) + 0.0, 
        uc(coalesce($20,'')),   coalesce($21,''),       coalesce($22,''),       
        uc(coalesce($23,'')),   coalesce($24,''),       coalesce($25,''),       
        uc(coalesce($26,'')),   coalesce($27,''),       
        uc(coalesce($28,'')),   coalesce($29,''),       
        coalesce($30,0.0) + 0.0,        coalesce($31,0.0) + 0.0,       coalesce($32,0.0) + 0.0,       
        coalesce($33,0.0) + 0.0,       coalesce($34,0.0) + 0.0,       coalesce($35,0.0) + 0.0,       
        coalesce($36,0.0) + 0.0,
        uc(coalesce($37,'')),   coalesce($38,''), coalesce($39,0.0) + 0.0, coalesce($40,0.0) + 0.0, coalesce($41,0.0) + 0.0
      );

    require LINZ::Geodetic::Datum;
    require LINZ::Geodetic::BursaWolf;
    my $ellipsoid = $self->ellipsoid($ellcode);
    if( $iers =~ /tsr/i)
    {
        my $tmp=$sf; $sf=$rx; $rx=$ry; $ry=$rz; $rz=$tmp;
        my $dtmp=$dsf; $dsf=$drx; $drx=$dry; $dry=$drz; $drz=$dtmp;
    }
    $refy=$1 if $iers =~ /\s(\d{4}(?:\.\d*))?/;
    my $transfunc =
      new LINZ::Geodetic::BursaWolf( $tx, $ty, $tz, $rx, $ry, $rz, $sf, $refy, $dtx,
        $dty, $dtz, $drx, $dry, $drz, $dsf, $iers ne '' );

    my $defmodel;
    my $filepath = '';
    if ( $self->{filename} )
    {
        ($filepath) = $self->{filename} =~ /^(.*[\\\/])/;
    }

    if ($gridtype)
    {
        $gridfile = $filepath . $gridfile
          if $filepath && -r $filepath . $gridfile;
        require LINZ::Geodetic::GridTransform;
        $transfunc =
          new LINZ::Geodetic::GridTransform( $gridfile, $gridtype, $ellipsoid,
            $transfunc );
    }
    if ( $velgrid || $euler )
    {
        die
"$velgrid$euler deformation models not currently supported ($dtmcode datum)\n";
    }
    elsif ($bw14)
    {
        require LINZ::Geodetic::BursaWolfVelocity;
        $bw14epoch = $refepoch if $refepoch;
        $defmodel = LINZ::Geodetic::BursaWolfVelocity->new(
            $bw14epoch, $bw14dtx, $bw14dty, $bw14dtz,
            $bw14drx,   $bw14dry, $bw14drz, $bw14dsf
        );
    }
    elsif ($linzdef)
    {
        my($deffile,$version)=split(' ',$linzdeffile);
        $deffile = $filepath . $deffile
          if $filepath && -r $filepath . $deffile;

        require LINZ::Geodetic::DefModelTransform;
        $defmodel =
          LINZ::Geodetic::DefModelTransform->new( $deffile, $linzdef, $refepoch,
            $ellipsoid,$version );
    }

    # Check if the baserf is a datum code, and if so then replace it with
    # the corresponding datum definition
    if (   $baserf ne $dtmcode
        && $baserf ne 'NONE'
        && exists $self->{datums}->{$dtmcode} )
    {
        $usedcodes ||= {};
        die "Invalid cyclic definition of datum $baserf"
          if exists $usedcodes->{$baserf};
        $usedcodes->{$dtmcode} = 1;
        $baserf = $self->datum( $baserf, 0.0, $usedcodes );
    }

    $objectkey = 'object' if !$defmodel;
    $dtmdef->{$objectkey} = $dtm = new LINZ::Geodetic::Datum(
        $name,    $ellipsoid, $baserf, $transfunc,
        $dtmcode, $defmodel,  $refepoch
    );

    return $dtm;
}

#===============================================================================
#
#   Method:       coordsys
#
#   Description:  $cs = $cslist->coordsys($cscode)
#
#   Parameters:   $cscode     The code of the coordinate system required
#
#   Returns:      The coordinate system corresponding to the code
#
#===============================================================================

sub coordsys
{
    my ( $self, $cscode ) = @_;
    return sort keys %{ $self->{crdsystems} } if $cscode eq '';

    # Codes can be suffixed with @refepoch, where epoch is one of
    # YYYY.yyyy, YYYYMMDD, or now
    $cscode = uc($cscode);
    my $refepoch = 0;
    if ( $cscode =~ /^(\w+)\@(?:(\d{4}(?:\.\d*))|(\d{4})(\d\d)(\d\d)|(NOW))$/ )
    {
        my ( $csc, $ydec, $yy, $ym, $yd, $now ) =
          ( $1, $2, $3 + 0, $4 + 0, $5 + 0, $6 );
        $cscode = $csc;
        if ( $ydec ne '' ) { $refepoch = $ydec + 0.0; }
        else
        {
            if ($now)
            {
                ( $yy, $ym, $yd ) = ( localtime() )[ 5, 4, 3 ];
                $yy += 1900;
                $ym++;
            }
            my $t0  = julian_day( $yy,     $ym, $yd );
            my $ty0 = julian_day( $yy,     1,   1 );
            my $ty1 = julian_day( $yy + 1, 1,   1 );
            $refepoch = $yy + ( $t0 - $ty0 ) / ( $ty1 - $ty0 );
        }
    }

    my $csdef = $self->{crdsystems}->{$cscode};
    die "Invalid coordinate system code $cscode.\n" if !$csdef;
    my $cs = $csdef->{object};
    if ( !$cs )
    {
        die "Invalid definition of coordinate system $cscode.\n"
          if $csdef->{def} !~ /^
                         \"([^\"]+)\"\s+
                         ref\_frame\s+(\S+)\s+
                         ( geodetic |
                           geocentric |
                           projection \s+ (\S.*)
                         )
                         \s*$
                        /xi;

        my ( $name, $dtmcode, $type, $projdef ) =
          ( $1, uc(coalesce($2,'')), $cstype{ uc(coalesce($3,'')) }, $4 );
        my $dtm = $self->datum( $dtmcode, $refepoch );
        my $proj;
        if ( $projdef ne '' )
        {
            $type = &LINZ::Geodetic::PROJECTION if $projdef ne '';
            require LINZ::Geodetic::Projection;
            $proj = new LINZ::Geodetic::Projection( $projdef, $dtm->ellipsoid );
        }

        require LINZ::Geodetic::CoordSys;
        $csdef->{object} = $cs =
          new LINZ::Geodetic::CoordSys( $type, $name, $dtm, $proj, $cscode );
    }
    return $cs;
}

#===============================================================================
#
#   Method:       coordsysname
#
#   Description:  Routine provides a cheap look up of the coordinate
#                 system name - avoiding loading the coordinate system
#                  $cs = $cslist->coordsysname($cscode)
#
#   Parameters:   $cscode     The code of the coordinate system required
#
#   Returns:      $csname     The name of the coordinate system
#
#===============================================================================

sub coordsysname
{
    my ( $self, $cscode ) = @_;
    $cscode = uc($cscode);
    my $csdef = $self->{crdsystems}->{$cscode};
    die "Invalid coordinate system code $cscode.\n" if !$csdef;
    my $cs = $csdef->{object};
    return $cs->{name} if $cs;
    die "Invalid definition of coordinate system $cscode.\n"
      if $csdef->{def} !~ /^
                         \"([^\"]+)\"\s+
                         ref\_frame\s+(\S+)\s+
                         ( geodetic |
                           geocentric |
                           projection \s+ (\S.*)
                         )
                         \s*$
                        /xi;

    my $name = $1;
    return $name;
}

#===============================================================================
#
#   Method:       vdatum
#
#   Description:  $vdatum = $cslist->vdatum($vdcode)
#
#   Parameters:   $vdcode    The code of the vertical datum required
#
#   Returns:      Returns a VerticalDatum object
#
#   Additional parameters for use only within this module
#      $nameonly   if true then find the name and exit
#      $used       codes already used in defining the surface, manages 
#                  circular references
#===============================================================================

sub vdatum
{
    my ( $self, $vdcode, $nameonly, $used ) = @_;
    $vdcode = uc($vdcode);
    my $vddef = $self->{vdatums}->{$vdcode};
    if( ! defined($vddef) )
    {
        my $usedmsg=$used ? " in definition of ".$used->[-1] : "";
        die "Invalid vertical datum code $vdcode$usedmsg.\n"
    }
    my $vd = $vddef->{object};
    if( $vd && $nameonly )
    {
        return $vd->name;
    }
    if ( !$vd )
    {
        die "Invalid definition of vertical datum $vdcode.\n"
          if $vddef->{def} !~ 
            /^
            \"([^\"]+)\"\s+
            (\S+)\s+
            (?:
            (geoid|grid)\s+(\S+)|
            ((?:offset\s+)?[+-]?\d+(?:\.\d+)?)
            )
            \s*$
            /ix;
        my ( $name, $refcscode, $gridtype, $geoidfile, $offset ) = 
           ( $1, uc($2), coalesce($3,''), coalesce($4,''), coalesce($5,0.0)+0.0 );
        return $name if $nameonly;
        my $hrefbase;
        my $refcrdsys;
        eval { $refcrdsys = $self->coordsys($refcscode); };
        undef $refcrdsys if defined($refcrdsys) && $refcrdsys->type != &LINZ::Geodetic::GEODETIC;
        if( ! defined($refcrdsys))
        {
            $used ||= [];
            push(@$used,$vdcode);
            foreach my $usedrf (@$used)
            {
                die "Circular reference in definition of vertical datum $refcscode\n"
                    if $usedrf eq $refcscode;
            }
            $hrefbase=$self->vdatum( $refcscode, 0, $used );
            $refcrdsys=$hrefbase->refcrdsys();
        }
        die "Invalid coordinate system code in vertical datum $vdcode.\n"
          if !$refcrdsys;
        my $gridfunc;
        if( $geoidfile )
        {
            if ( $self->{filename} )
            {
                my ($filepath) = $self->{filename} =~ /^(.*[\\\/])/;
                $geoidfile = $filepath . $geoidfile if -r $filepath . $geoidfile;
                $geoidfile = $filepath . $geoidfile . '.grd'
                  if -r $filepath . $geoidfile . '.grd';
            }
            require LINZ::Geodetic::GeoidGrid;
            my $factor=uc($gridtype) eq 'GEOID' ? 1.0 : -1.0;
            eval { $gridfunc = new LINZ::Geodetic::GeoidGrid($geoidfile,$factor); };
            die "Invalid grid specified for vertical datum $vdcode.\n"
              if !$gridfunc;
        }
        else
        {
            require LINZ::Geodetic::VerticalDatum;
            $gridfunc=LINZ::Geodetic::VerticalDatum::Offset->new($offset);
        }

        require LINZ::Geodetic::VerticalDatum;
        $vddef->{object} = $vd =
          new LINZ::Geodetic::VerticalDatum( $name, $hrefbase, $refcrdsys, $gridfunc, $vdcode );
    }
    return $vd;
}

# Deprecated function
sub hgtref { return vdatum(@_); }

#===============================================================================
#
#   Method:       vdatumname
#
#   Description:  Routine provides a cheap look up of the vertical datum name
#                 system name - avoiding loading the coordinate system
#                  $vdname = $cslist->vdatumname($vdcode)
#
#   Parameters:   $vdcode     The code of the coordinate system required
#
#   Returns:      $vdname     The name of the coordinate system
#
#===============================================================================

sub vdatumname
{
    my ( $self, $vdcode ) = @_;
    return $self->vdatum($vdcode,1);
}

# Deprecated function
sub hgtrefname { return vdatumname(@_); }

1;
