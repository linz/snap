#===============================================================================
# Module:             CoordSysList.pm
#
# Description:       Defines packages:
#                      Geodetic::CoordSysList
#
# Dependencies:      Uses the following modules:
#                      Geodetic
#                      Geodetic::BursaWolf
#                      Geodetic::CoordSys
#                      Geodetic::Ellipsoid
#                      Geodetic::Projection
#                      Geodetic::Datum
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
#   Class:       Geodetic::CoordSysList
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
#                   heightref    Hash storing height reference system definitions
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
#                  $cslist = new Geodetic::CoordSysList($filename)
#
#                 Functions to retrieve objects from the list
#                  $ellipsoid = $cslist->ellipsoid($elpcode)
#                  $datum = $cslist->datum($dtmcode)
#                  $cs = $cslist->coordsys($cscode)
#                  $csname =$cslist->coordsysname($cscode)
#                  $heightref=$cslist->heightref($hrcode);
#                  $heightcoord=$cslist->heightcoord($hrcode);
#
#===============================================================================

package Geodetic::CoordSysList;

use Time::JulianDay;
require Geodetic;

# Note: requires for other components are in subs below to delay loading
# until actually require.

my %cstype = (
    GEODETIC   => &Geodetic::GEODETIC,
    CARTESIAN  => &Geodetic::CARTESIAN,
    PROJECTION => &Geodetic::PROJECTION
);

#===============================================================================
#
#   Method:       new
#
#   Description:  $cslist= new Geodetic::CoordSysList($elldefs,$dtmdefs,$csdefs)
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
    my ( $class, $elldefs, $dtmdefs, $csdefs, $hrfdefs, $hcsdefs ) = @_;
    my $ellipsoids = {};
    my $datums     = {};
    my $crdsystems = {};
    my $hgtrefs    = {};
    my $hgtcrdsys  = {};

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
    if ($hrfdefs)
    {
        foreach ( keys %$hrfdefs )
        {
            $hgtrefs->{$_} = { code => $_, def => $hrfdefs->{$_} };
        }
    }
    if ($hcsdefs)
    {
        foreach ( keys %$hcsdefs )
        {
            $hgtcrdsys->{$_} = { code => $_, def => $hcsdefs->{$_} };
        }
    }

    my $self = {
        ellipsoids => $ellipsoids,
        datums     => $datums,
        crdsystems => $crdsystems,
        hgtrefs    => $hgtrefs,
        hgtcrdsys  => $hgtcrdsys
    };
    return bless $self, $class;
}

#===============================================================================
#
#   Method:       newFromCoordSysDef
#
#   Description:  $cslist = new Geodetic::CoordSysList($filename)
#
#   Parameters:   $filename  The name of the coordinate system definition file
#
#   Returns:      A coordinate system list object
#
#===============================================================================

sub newFromCoordSysDef
{
    my ( $class, $filename ) = @_;
    local (*CSFILE);
    open( CSFILE, $filename )
      || die "Cannot open coordinate system file $filename.\n";
    my $ellipsoids = {};
    my $datums     = {};
    my $crdsystems = {};
    my $hgtrefs    = {};
    my $hgtcrdsys  = {};
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
                elsif ( $section eq 'HEIGHT_REFERENCES' )
                {
                    $list = $hgtrefs;
                }
                elsif ( $section eq 'HEIGHT_COORDINATE_SYSTEMS' )
                {
                    $list = $hgtcrdsys;
                }
            }
            next;
        }
        next if !$list;
        while (/(\&|\\)$/)
        {
            $_ = $` . <CSFILE>;
            s/\s*$//;
        }
        $list->{ uc($1) } = $' if /^\s*(\S+)\s*/;
    }
    close(CSFILE);

    my $self =
      new( $class, $ellipsoids, $datums, $crdsystems, $hgtrefs, $hgtcrdsys );

    $self->{filename} = $filename;

    return bless $self, $class;
}

#===============================================================================
#
#   Method:       definitions
#
#   Description:  ($elldef,$dtmdef,$csdef) = $cslist->definitions;
#
#   Parameters:   None
#
#   Returns:      Returns three hash references containing the coordinate
#                 system definition strings.
#
#===============================================================================

sub definitions
{
    my ($self) = @_;
    my ( %elldef, %dtmdef, %csdef, %hrdef, %hcsdef );
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
    foreach ( values %{ $self->{hgtrefs} } ) {
        $hrdef{ $_->{code} } = $_->{def};
    }
    foreach ( values %{ $self->{hgtcrdsys} } ) {
        $hcsdef{ $_->{code} } = $_->{def};
    }
    return ( \%elldef, \%dtmdef, \%csdef, \%hrdef, \%hcsdef );
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
        require Geodetic::Ellipsoid;
        $elpdef->{object} = $elp =
          new Geodetic::Ellipsoid( $2, $3, $1, $elpcode );
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
                                (?:(linzdef)\s+(\S+)) |
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
        $name,     $ellcode,     $baserf,     $iers,        $tx,
        $ty,       $tz,          $rx,         $ry,          $rz,
        $sf,       $refy,        $dtx,        $dty,         $dtz,
        $drx,      $dry,         $drz,        $dsf,         $gridtype,
        $gridfile, $griddesc,    $velgrid,    $velgridfile, $velgriddesc,
        $linzdef,  $linzdeffile, $bw14,       $bw14epoch,   $bw14dtx,
        $bw14dty,  $bw14dtz,     $bw14drx,    $bw14dry,     $bw14drz,
        $bw14dsf,  $euler,       $eulerepoch, $eulerplon,   $eulerplat,
        $eulerrate
      )
      = (
        $1,        $2,        uc($3),    uc($4),    $5 + 0.0,  $6 + 0.0,
        $7 + 0.0,  $8 + 0.0,  $9 + 0.0,  $10 + 0.0, $11 + 0.0, $12,
        $13 + 0.0, $14 + 0.0, $15 + 0.0, $16 + 0.0, $17 + 0.0, $18 + 0.0,
        $19 + 0.0, uc($20),   $21,       $22,       uc($23),   $24,
        $25,       uc($26),   $27,       uc($28),   $29,       $30,
        $31,       $32,       $33,       $34,       $35,       $36,
        uc($37),   $38,       $39,       $40,       $41
      );

    require Geodetic::Datum;
    require Geodetic::BursaWolf;
    my $ellipsoid = $self->ellipsoid($ellcode);
    if( $iers =~ /tsr/i)
    {
        my $tmp=$sf; $sf=$rx; $rx=$ry; $ry=$rz; $rz=$tmp;
        my $dtmp=$dsf; $dsf=$drx; $drx=$dry; $dry=$drz; $drz=$dtmp;
    }
    $refy=$1 if $iers =~ /\s(\d{4}(?:\.\d*))?/;
    my $transfunc =
      new Geodetic::BursaWolf( $tx, $ty, $tz, $rx, $ry, $rz, $sf, $refy, $dtx,
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
        require Geodetic::GridTransform;
        $transfunc =
          new Geodetic::GridTransform( $gridfile, $gridtype, $ellipsoid,
            $transfunc );
    }
    if ( $velgrid || $euler )
    {
        die
"$velgrid$euler deformation models not currently supported ($dtmcode datum)\n";
    }
    elsif ($bw14)
    {
        require Geodetic::BursaWolfVelocity;
        $bw14epoch = $refepoch if $refepoch;
        $defmodel = Geodetic::BursaWolfVelocity->new(
            $bw14epoch, $bw14dtx, $bw14dty, $bw14dtz,
            $bw14drx,   $bw14dry, $bw14drz, $bw14dsf
        );
    }
    elsif ($linzdef)
    {
        $linzdeffile = $filepath . $linzdeffile
          if $filepath && -r $filepath . $linzdeffile;

        require Geodetic::DefModelTransform;
        $defmodel =
          Geodetic::DefModelTransform->new( $linzdeffile, $linzdef, $refepoch,
            $ellipsoid );
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
    $dtmdef->{$objectkey} = $dtm = new Geodetic::Datum(
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
          ( $1, uc($2), $cstype{ uc($3) }, $4 );
        my $dtm = $self->datum( $dtmcode, $refepoch );
        my $proj;
        if ( $projdef ne '' )
        {
            $type = &Geodetic::PROJECTION if $projdef ne '';
            require Geodetic::Projection;
            $proj = new Geodetic::Projection( $projdef, $dtm->ellipsoid );
        }

        require Geodetic::CoordSys;
        $csdef->{object} = $cs =
          new Geodetic::CoordSys( $type, $name, $dtm, $proj, $cscode );
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
#   Method:       hgtref
#
#   Description:  $hgtref = $cslist->hgtref($hrfcode)
#
#   Parameters:   $hrfcode    The code of the height reference required
#
#   Returns:      Returns a HgtRef object
#
#===============================================================================

sub hgtref
{
    my ( $self, $hrfcode ) = @_;
    $hrfcode = uc($hrfcode);
    my $hrfdef = $self->{hgtrefs}->{$hrfcode};
    die "Invalid height reference code $hrfcode.\n" if !defined $hrfdef;
    my $hrf = $hrfdef->{object};
    if ( !$hrf )
    {
        die "Invalid definition of height reference $hrfcode.\n"
          if $hrfdef->{def} !~ /^\"([^\"]+)\"\s+(\S+)\s+geoid\s+(\S+)\s*$/i;
        my ( $name, $refcscode, $geoidfile ) = ( $1, $2, $3 );
        my $refcrdsys;
        eval { $refcrdsys = $self->coordsys($refcscode); };
        die "Invalid coordinate system code in height reference $hrfcode.\n"
          if !$refcrdsys;
        if ( $self->{filename} )
        {
            my ($filepath) = $self->{filename} =~ /^(.*[\\\/])/;
            $geoidfile = $filepath . $geoidfile if -r $filepath . $geoidfile;
            $geoidfile = $filepath . $geoidfile . '.grd'
              if -r $filepath . $geoidfile . '.grd';
        }
        require Geodetic::GeoidGrid;
        my $gridfunc;
        eval { $gridfunc = new Geodetic::GeoidGrid($geoidfile); };
        die "Invalid geoid grid specified for height reference $hrfcode.\n"
          if !$gridfunc;

        require Geodetic::HgtRef;
        $hrfdef->{object} = $hrf =
          new Geodetic::HgtRef( $name, $refcrdsys, $gridfunc, $hrfcode );
    }
    return $hrf;
}

#===============================================================================
#
#   Method:       hgtcrdsys
#
#   Description:  $hgtcrdsys = $cslist->hgtcrdsys($hcscode)
#
#   Parameters:   $hcscode    The code of the height reference required
#
#   Returns:      Returns a HgtCrdSys object
#
#===============================================================================

sub hgtcrdsys
{
    my ( $self, $hcscode ) = @_;
    $hcscode = uc($hcscode);
    my $hcsdef = $self->{hgtcrdsys}->{$hcscode};
    die "Invalid height coordinate system code $hcscode.\n" if !defined $hcsdef;
    my $hcs = $hcsdef->{object};
    if ( !$hcs )
    {
        die "Invalid definition of height reference $hcscode.\n"
          if $hcsdef->{def} !~ /^\"([^\"]+)\"\s+(\S+)\s+([+-]?\d+\.?\d*)\s*$/;
        my ( $name, $hrfcode, $offset ) = ( $1, $2, $3 );
        my $hrf;
        eval { $hrf = $self->hgtref($hrfcode); };
        if ($@)
        {
            die $@
              . "Invalid height reference $hrfcode specified for height coordinate system $hcscode.\n";
        }
        require Geodetic::HgtCrdSys;
        $hcsdef->{object} = $hcs =
          new Geodetic::HgtCrdSys( $name, $hrf, $offset, $hcscode );
    }
    return $hcs;
}

#===============================================================================
#
#   Method:       hgtcrdsysname
#
#   Description:  Routine provides a cheap look up of the height coordinate
#                 system name - avoiding loading the coordinate system
#                  $hcsname = $cslist->hgtcrdsysname($hcscode)
#
#   Parameters:   $hcscode     The code of the coordinate system required
#
#   Returns:      $hcsname     The name of the coordinate system
#
#===============================================================================

sub hgtcrdsysname
{
    my ( $self, $hcscode ) = @_;
    $hcscode = uc($hcscode);
    my $hcsdef = $self->{hgtcrdsys}->{$hcscode};
    die "Invalid height coordinate system code $hcscode.\n" if !$hcsdef;
    my $hcs = $hcsdef->{object};
    return $hcs->{name} if $hcs;
    die "Invalid definition of coordinate system $hcscode.\n"
      if $hcsdef->{def} !~ /^
                         \"([^\"]+)\"\s+
                         (\S+)\s+
                         ([+-]?\d+(?:\.\d+)?)
                         \s*$
                        /xi;

    my $name = $1;
    return $name;
}

1;
