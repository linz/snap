# Working version of NZMS260MapRef
#
# This version is for immediate convenience pending a better implementation
# of coordinate representation.  This provides the functions for converting
# between NZMS260 map references and NZMG coordinates.
#
# $Id: NZMS260MapRef.pm,v 1.4 2006/04/17 21:34:02 gdb Exp $
#
# $Log: NZMS260MapRef.pm,v $
# Revision 1.4  2006/04/17 21:34:02  gdb
# Implementation of 6 digit map references rather than non-standard 8 digit references
#
# Revision 1.3  2005/11/27 19:39:30  gdb
# *** empty log message ***
#
# Revision 1.1  2001/05/22 21:09:03  gdb
# Added functions for calculating map references
#
#

use strict;

package LINZ::Geodetic::NZTopo50MapRef;

use vars qw/$ncodes $cisheets $cutsheets $sheetname/;

$ncodes = 'AS AT AU AV AW AX AY AZ BA BB BC BD BE BF BG BH BJ BK BL BM BN BP BQ BR BS BT BU BV BW BX BY BZ CA CB CC CD CE CF CG CH CJ CK ';

$cisheets  =
    {
    CI01=>{name=>'CI01', emin=>3458000,emax=>3482000,nmin=>5140000,nmax=>5176000},
    CI02=>{name=>'CI02', emin=>3482000,emax=>3506000,nmin=>5140000,nmax=>5176000},
    CI03=>{name=>'CI03', emin=>3506000,emax=>3530000,nmin=>5140000,nmax=>5176000},
    CI04=>{name=>'CI04', emin=>3482000,emax=>3506000,nmin=>5104000,nmax=>5140000},
    CI05=>{name=>'CI05', emin=>3506000,emax=>3530000,nmin=>5104000,nmax=>5140000},
    CI06=>{name=>'CI06', emin=>3506000,emax=>3530000,nmin=>5068000,nmax=>5104000}
    };

$cutsheets = 
{
    'AS21/AS22' => {name=>'AS21/AS22', emin=>1504000, emax=>1528000, nmin=>6198000, nmax=>6234000 },
    'AU28ptAV28' => {name=>'AU28ptAV28', emin=>1660000, emax=>1684000, nmin=>6114000, nmax=>6150000 },
    'AU29ptAV29' => {name=>'AU29ptAV29', emin=>1684000, emax=>1708000, nmin=>6114000, nmax=>6150000 },
    'AV25ptAV26' => {name=>'AV25ptAV26', emin=>1596000, emax=>1620000, nmin=>6090000, nmax=>6126000 },
    'AX32ptsAX31,AY31,AY32' => {name=>'AX32ptsAX31,AY31,AY32', emin=>1748000, emax=>1772000, nmin=>6006000, nmax=>6042000 },
    'AZ36ptsAZ35,BA35,BA36' => {name=>'AZ36ptsAZ35,BA35,BA36', emin=>1844000, emax=>1868000, nmin=>5937000, nmax=>5973000 },
    'BA36ptBA35' => {name=>'BA36ptBA35', emin=>1844000, emax=>1868000, nmin=>5910000, nmax=>5946000 },
    'BB30ptBB31' => {name=>'BB30ptBB31', emin=>1716000, emax=>1740000, nmin=>5874000, nmax=>5910000 },
    'BB37ptBB36' => {name=>'BB37ptBB36', emin=>1868000, emax=>1892000, nmin=>5874000, nmax=>5910000 },
    'BC40ptBD40' => {name=>'BC40ptBD40', emin=>1956000, emax=>1980000, nmin=>5826000, nmax=>5862000 },
    'BD31ptBD32' => {name=>'BD31ptBD32', emin=>1740000, emax=>1764000, nmin=>5802000, nmax=>5838000 },
    'BD39ptBE39' => {name=>'BD39ptBE39', emin=>1924000, emax=>1948000, nmin=>5790000, nmax=>5826000 },
    'BD40ptBE40' => {name=>'BD40ptBE40', emin=>1948000, emax=>1972000, nmin=>5790000, nmax=>5826000 },
    'BF45ptBF44' => {name=>'BF45ptBF44', emin=>2060000, emax=>2084000, nmin=>5730000, nmax=>5766000 },
    'BG30ptBH30' => {name=>'BG30ptBH30', emin=>1708000, emax=>1732000, nmin=>5682000, nmax=>5718000 },
    'BJ40ptBJ39' => {name=>'BJ40ptBJ39', emin=>1940000, emax=>1964000, nmin=>5622000, nmax=>5658000 },
    'BJ43ptsBJ42,BH42,BH43' => {name=>'BJ43ptsBJ42,BH42,BH43', emin=>2012000, emax=>2036000, nmin=>5634000, nmax=>5670000 },
    'BK28ptBJ28' => {name=>'BK28ptBJ28', emin=>1660000, emax=>1684000, nmin=>5598000, nmax=>5634000 },
    'BK40ptBK39' => {name=>'BK40ptBK39', emin=>1940000, emax=>1964000, nmin=>5586000, nmax=>5622000 },
    'BL31ptBK31' => {name=>'BL31ptBK31', emin=>1732000, emax=>1756000, nmin=>5562000, nmax=>5598000 },
    'BM24ptBN24' => {name=>'BM24ptBN24', emin=>1564000, emax=>1588000, nmin=>5502000, nmax=>5538000 },
    'BM25ptBN25' => {name=>'BM25ptBN25', emin=>1588000, emax=>1612000, nmin=>5502000, nmax=>5538000 },
    'BM39ptBM38' => {name=>'BM39ptBM38', emin=>1916000, emax=>1940000, nmin=>5514000, nmax=>5550000 },
    'BN29ptBN28' => {name=>'BN29ptBN28', emin=>1676000, emax=>1700000, nmin=>5478000, nmax=>5514000 },
    'BN32ptBP32' => {name=>'BN32ptBP32', emin=>1756000, emax=>1780000, nmin=>5466000, nmax=>5502000 },
    'BN38ptBN37' => {name=>'BN38ptBN37', emin=>1892000, emax=>1916000, nmin=>5478000, nmax=>5514000 },
    'BP26ptBP27' => {name=>'BP26ptBP27', emin=>1620000, emax=>1644000, nmin=>5442000, nmax=>5478000 },
    'BP30ptBQ30' => {name=>'BP30ptBQ30', emin=>1708000, emax=>1732000, nmin=>5430000, nmax=>5466000 },
    'BQ21ptBQ22' => {name=>'BQ21ptBQ22', emin=>1500000, emax=>1524000, nmin=>5406000, nmax=>5442000 },
    'BQ36ptBQ35' => {name=>'BQ36ptBQ35', emin=>1844000, emax=>1868000, nmin=>5406000, nmax=>5442000 },
    'BW14ptBX14' => {name=>'BW14ptBX14', emin=>1324000, emax=>1348000, nmin=>5178000, nmax=>5214000 },
    'BW25ptBW24' => {name=>'BW25ptBW24', emin=>1580000, emax=>1604000, nmin=>5190000, nmax=>5226000 },
    'BX12ptBY12' => {name=>'BX12ptBY12', emin=>1276000, emax=>1300000, nmin=>5142000, nmax=>5178000 },
    'BY10ptBZ10' => {name=>'BY10ptBZ10', emin=>1228000, emax=>1252000, nmin=>5106000, nmax=>5142000 },
    'BZ21ptBZ20' => {name=>'BZ21ptBZ20', emin=>1484000, emax=>1508000, nmin=>5082000, nmax=>5118000 },
    'CA07ptCB07' => {name=>'CA07ptCB07', emin=>1156000, emax=>1180000, nmin=>5034000, nmax=>5070000 },
    'CC19ptCC18' => {name=>'CC19ptCC18', emin=>1436000, emax=>1460000, nmin=>4974000, nmax=>5010000 },
    'CD04ptCD05' => {name=>'CD04ptCD05', emin=>1092000, emax=>1116000, nmin=>4938000, nmax=>4974000 },
    'CG07ptCF07' => {name=>'CG07ptCF07', emin=>1156000, emax=>1180000, nmin=>4842000, nmax=>4878000 },
    'CH05/CH06' => {name=>'CH05/CH06', emin=>1120000, emax=>1144000, nmin=>4794000, nmax=>4830000 },
    'CJ07/CK07' => {name=>'CJ07/CK07', emin=>1156000, emax=>1180000, nmin=>4740000, nmax=>4776000 },
};

$sheetname = 
   {
   'AS21' => 'AS21/AS22',
   'AS22' => 'AS21/AS22',
   'AU28' => 'AU28ptAV28',
   'AU29' => 'AU29ptAV29',
   'AV25' => 'AV25ptAV26',
   'AX32' => 'AX32ptsAX31,AY31,AY32',
   'AZ36' => 'AZ36ptsAZ35,BA35,BA36',
   'BA36' => 'BA36ptBA35',
   'BB30' => 'BB30ptBB31',
   'BB37' => 'BB37ptBB36',
   'BC40' => 'BC40ptBD40',
   'BD31' => 'BD31ptBD32',
   'BD39' => 'BD39ptBE39',
   'BD40' => 'BD40ptBE40',
   'BF45' => 'BF45ptBF44',
   'BG30' => 'BG30ptBH30',
   'BJ40' => 'BJ40ptBJ39',
   'BJ42' => 'BJ43ptsBJ42,BH42,BH43',
   'BJ43' => 'BJ43ptsBJ42,BH42,BH43',
   'BK28' => 'BK28ptBJ28',
   'BK40' => 'BK40ptBK39',
   'BL31' => 'BL31ptBK31',
   'BM24' => 'BM24ptBN24',
   'BM25' => 'BM25ptBN25',
   'BM39' => 'BM39ptBM38',
   'BN29' => 'BN29ptBN28',
   'BN32' => 'BN32ptBP32',
   'BN38' => 'BN38ptBN37',
   'BP26' => 'BP26ptBP27',
   'BP30' => 'BP30ptBQ30',
   'BQ21' => 'BQ21ptBQ22',
   'BQ30' => 'BP30ptBQ30',
   'BQ36' => 'BQ36ptBQ35',
   'BW14' => 'BW14ptBX14',
   'BW25' => 'BW25ptBW24',
   'BX12' => 'BX12ptBY12',
   'BY10' => 'BY10ptBZ10',
   'BZ21' => 'BZ21ptBZ20',
   'CA07' => 'CA07ptCB07',
   'CC19' => 'CC19ptCC18',
   'CD04' => 'CD04ptCD05',
   'CG07' => 'CG07ptCF07',
   'CH05' => 'CH05/CH06',
   'CH06' => 'CH05/CH06',
   'CJ07' => 'CJ07/CK07',
   'CK07' => 'CJ07/CK07',
   };

sub sheet {
   my( $n, $e, $fullname ) = @_;
   my $sheet;
   $sheet = &CISheet($n,$e) if $e >= 3458000 && $e <= 3530000 && $n >= 5068000 && $n <=5176000;
   if( ! $sheet )
   {
       die "Coordinates out of range for Topo50 map reference\n" if
          $n > 6234000 || $n < 4722000 || $e < 1084000 || $e > 2108000;
       my $ns = int( (6234000-$n)/36000 ); $ns = 41 if $ns > 41;
       my $es = int( ($e-1084000)/24000 ); $es = 43 if $es > 43;
       $sheet = substr($ncodes,$ns*3,2) . sprintf("%02d",$es+4);
       $sheet = $sheetname->{$sheet} || $sheet if $fullname;

   }
   return $sheet;
   }

sub write {
   my( $n, $e, $fullname ) = @_;

   my $sheet = sheet($n,$e,$fullname);

   my $nr = substr(int(($n+50)/100),-3);
   my $er = substr(int(($e+50)/100),-3);

   my $ref = $sheet.' '.$er.' '.$nr;
   return ($ref);
   };


sub CISheet
{
   my($n,$e) = @_;
   foreach my $sh (keys %$cisheets)
   {
       my $range = $cisheets->{$sh};
       if( $e >= $range->{emin} && $e <= $range->{emax} &&
           $n >= $range->{nmin} && $n <= $range->{nmax} )
       {
           return $sh;
       }
   }
   return '';
}

sub extents
{
   my($sheetname) = @_;
   $sheetname = uc($sheetname);
   my $ci = $cisheets->{$sheetname};
   return $ci if $ci;

   if( $sheetname =~ /^([A-Z][A-Z])([0-4]\d|50)$/ )
   {
       my($maplet,$mapnum) = ($1, $2);
       my $mapletn = index($ncodes,$maplet);
       return undef if $mapletn < 0;
       $mapletn /= 3;

       my $n0 = (6234000-36000) - 36000*$mapletn;
       my $e0 = 1084000+24000*($mapnum-4);

       return { name=>$sheetname, 
                emin=>$e0, emax=>$e0+24000,
                nmin=>$n0, nmax=>$n0+36000 };
   }
   $sheetname =~ s/\s//g;
   $sheetname = $1.lc($2).$3 if $sheetname =~ /^(....)(PTS?)(.*)$/;
   my $offset = $cutsheets->{$sheetname};
   return $offset if $offset;
   return undef;
}

sub read {
   my( $ref ) = @_;
   $ref = uc($ref);
   $ref =~ /^(.+?)\s+(\d\d\d)\s*(\d\d\d)$/
     || $ref =~ /^(.+?)\s+(\d\d\d\d)\s*(\d\d\d\d)$/
     || die "Invalid map reference $ref\n";
   my( $sheet, $er, $nr ) = ($1,$2,$3,$4);
   my $extents = extents($sheet);
   die "Invalid map sheet in $ref\n" if ! $extents;
   
   my $n0 = ($extents->{nmin}+$extents->{nmax})/2;
   my $e0 = ($extents->{emin}+$extents->{emax})/2;

   if( length($er) == 3 ) { $er *= 10; $nr *= 10; };
   $er *= 10; $nr *= 10;

   $nr = int(($n0 - $nr)/100000+0.5)*100000 + $nr;
   $er = int(($e0 - $er)/100000+0.5)*100000 + $er;
   return ($nr, $er);
   }

1;
