# LINZ::Geodetic::Topo50MapRef
#
# This provides the functions for converting
# between Topo50 map references and NZTM coordinates

use strict;

package LINZ::Geodetic::NZTopo250MapRef;

use vars qw/$sheets @sheetlist/;

$sheets = {
   "250-01"=>{code=>"250-01",name=>"North Cape (Otou)",emin=>1492000.0, emax=>1612000.0, nmin=>6054000.0, nmax=>6234000.0},
   "250-02"=>{code=>"250-02",name=>"Kaikohe",emin=>1612000.0, emax=>1732000.0, nmin=>6054000.0, nmax=>6234000.0},
   "250-03"=>{code=>"250-03",name=>"Warkworth",emin=>1732000.0, emax=>1852000.0, nmin=>5946000.0, nmax=>6126000.0},
   "250-04"=>{code=>"250-04",name=>"Dargaville",emin=>1612000.0, emax=>1732000.0, nmin=>5874000.0, nmax=>6054000.0},
   "250-05"=>{code=>"250-05",name=>"Auckland",emin=>1732000.0, emax=>1852000.0, nmin=>5766000.0, nmax=>5946000.0},
   "250-06"=>{code=>"250-06",name=>"Tauranga",emin=>1852000.0, emax=>1972000.0, nmin=>5766000.0, nmax=>5946000.0},
   "250-07"=>{code=>"250-07",name=>"East Cape",emin=>1972000.0, emax=>2092000.0, nmin=>5766000.0, nmax=>5946000.0},
   "250-08"=>{code=>"250-08",name=>"New Plymouth",emin=>1612000.0, emax=>1732000.0, nmin=>5586000.0, nmax=>5766000.0},
   "250-09"=>{code=>"250-09",name=>"Taumarunui",emin=>1732000.0, emax=>1852000.0, nmin=>5586000.0, nmax=>5766000.0},
   "250-10"=>{code=>"250-10",name=>"Napier",emin=>1852000.0, emax=>1972000.0, nmin=>5586000.0, nmax=>5766000.0},
   "250-11"=>{code=>"250-11",name=>"Gisborne",emin=>1972000.0, emax=>2092000.0, nmin=>5586000.0, nmax=>5766000.0},
   "250-12"=>{code=>"250-12",name=>"Takaka",emin=>1492000.0, emax=>1612000.0, nmin=>5406000.0, nmax=>5586000.0},
   "250-13"=>{code=>"250-13",name=>"Nelson",emin=>1612000.0, emax=>1732000.0, nmin=>5406000.0, nmax=>5586000.0},
   "250-14"=>{code=>"250-14",name=>"Palmerston North",emin=>1732000.0, emax=>1852000.0, nmin=>5406000.0, nmax=>5586000.0},
   "250-15"=>{code=>"250-15",name=>"Dannevirke",emin=>1852000.0, emax=>1972000.0, nmin=>5406000.0, nmax=>5586000.0},
   "250-16"=>{code=>"250-16",name=>"Wellington",emin=>1732000.0, emax=>1852000.0, nmin=>5298000.0, nmax=>5478000.0},
   "250-17"=>{code=>"250-17",name=>"Greymouth",emin=>1372000.0, emax=>1492000.0, nmin=>5226000.0, nmax=>5406000.0},
   "250-18"=>{code=>"250-18",name=>"Murchison",emin=>1492000.0, emax=>1612000.0, nmin=>5226000.0, nmax=>5406000.0},
   "250-19"=>{code=>"250-19",name=>"Kaikoura",emin=>1612000.0, emax=>1732000.0, nmin=>5226000.0, nmax=>5406000.0},
   "250-20"=>{code=>"250-20",name=>"Martins Bay",emin=>1132000.0, emax=>1252000.0, nmin=>5046000.0, nmax=>5226000.0},
   "250-21"=>{code=>"250-21",name=>"Haast",emin=>1252000.0, emax=>1372000.0, nmin=>5046000.0, nmax=>5226000.0},
   "250-22"=>{code=>"250-22",name=>"Timaru",emin=>1372000.0, emax=>1492000.0, nmin=>5046000.0, nmax=>5226000.0},
   "250-23"=>{code=>"250-23",name=>"Christchurch",emin=>1492000.0, emax=>1612000.0, nmin=>5046000.0, nmax=>5226000.0},
   "250-24"=>{code=>"250-24",name=>"Dusky Sound",emin=>1012000.0, emax=>1132000.0, nmin=>4866000.0, nmax=>5046000.0},
   "250-25"=>{code=>"250-25",name=>"Te Anau",emin=>1132000.0, emax=>1252000.0, nmin=>4866000.0, nmax=>5046000.0},
   "250-26"=>{code=>"250-26",name=>"Alexandra",emin=>1252000.0, emax=>1372000.0, nmin=>4866000.0, nmax=>5046000.0},
   "250-27"=>{code=>"250-27",name=>"Dunedin",emin=>1372000.0, emax=>1492000.0, nmin=>4866000.0, nmax=>5046000.0},
   "250-28"=>{code=>"250-28",name=>"Tuatapere",emin=>1084000.0, emax=>1204000.0, nmin=>4758000.0, nmax=>4938000.0},
   "250-29"=>{code=>"250-29",name=>"Invercargill",emin=>1132000.0, emax=>1252000.0, nmin=>4686000.0, nmax=>4866000.0},
   "250-30"=>{code=>"250-30",name=>"Owaka",emin=>1252000.0, emax=>1372000.0, nmin=>4686000.0, nmax=>4866000.0},
   "250-31"=>{code=>"250-31",name=>"Chatham Islands",emin=>3440000.0, emax=>3560000.0, nmin=>5040000.0, nmax=>5220000.0},
   };

@sheetlist=values(%$sheets);
@sheetlist=sort {$a->{nmin} <=> $b->{nmin}} @sheetlist;

sub Topo250Sheet 
{
   my( $n, $e ) = @_;
   foreach my $s (@sheetlist)
   {
       last if $n < $s->{nmin};
       next if $n > $s->{nmax};
       return $s if $e >= $s->{emin} && $e <= $s->{emax}
   }
   return undef;
}

sub write 
{
   my( $n, $e ) = @_;

   my $sheet = Topo250Sheet($n,$e);
   die "Location not on a Topo250 map sheet\n" if ! $sheet;

   my $nr = substr(int(($n+500)/1000),-3);
   my $er = substr(int(($e+500)/1000),-3);

   my $ref = $sheet->{code}.' '.$er.' '.$nr;
   return ($ref);
};

sub extents
{
   my($sheetcode) = @_;
   return $sheets->{$sheetcode}
}

sub read {
   my( $ref ) = @_;
   $ref = uc($ref);
   $ref =~ /^(250\-\d\d)\s+(\d\d\d)\s*(\d\d\d)$/
     || $ref =~ /^(250\-\d\d)\s+(\d\d\d\d)\s*(\d\d\d\d)$/
     || die "Invalid map reference $ref\n";
   my( $sheet, $er, $nr ) = ($1,$2,$3);
   my $extents = extents($sheet);
   die "Invalid map sheet in $ref\n" if ! $extents;
   
   my $n0 = ($extents->{nmin}+$extents->{nmax})/2;
   my $e0 = ($extents->{emin}+$extents->{emax})/2;

   if( length($er) == 3 ) { $er *= 10; $nr *= 10; };
   $er *= 100; $nr *= 100;

   $nr = int(($n0 - $nr)/1000000+0.5)*1000000 + $nr;
   $er = int(($e0 - $er)/1000000+0.5)*1000000 + $er;
   return ($nr, $er);
   }

1;
