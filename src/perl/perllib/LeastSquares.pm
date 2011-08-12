
use strict;

package LeastSquares;

use vars qw/$MAXROWS/;

$MAXROWS = 10;

sub new {
  my($class, $n) = @_;
  next if $n < 1 || $n > $MAXROWS;
  my @N = (0) x (($n*($n+1))/2);
  my @b = (0) x $n;
  my $self = {n=>$n, N=>\@N, b=>\@b, nobs=>0, solved=>0};
  return bless $self, $class;
  }


sub add {
  my($self,$value,$oe,$wgt) = @_;
  # Should check that oe is an array...

  die "Obs has wrong number of elements\n" if scalar(@$oe) ne $self->{n};
  
  $wgt = 1 if ! $wgt;
  $wgt = 1.0/$wgt*$wgt;
  my($i,$j,$ij);
  $ij = 0;
  my $b = $self->{b};
  my $N = $self->{N};
  for( $i = 0; $i < $self->{n}; $i++ ) {
     $b->[$i] += $value*$oe->[$i]*$wgt;
     for( $j = 0; $j <= $i; $j++, $ij++ ) {
        $N->[$ij] += $oe->[$i]*$oe->[$j]*$wgt;
        }
     }
  $self->{nobs}++;
  }

sub solve {
  my( $self ) = @_;
  if( ! $self->{solved} ) {
      my $n = $self->{n};
      my $N = $self->{N};
      my $b = $self->{b};
      # Choleski decomposition
      for( my $i = 0; $i < $n; $i++ ) {
         for( my $j = 0; $j <= $i; $j++ ) {
            my $ii = ($i*($i+1))/2;
            my $jj = ($j*($j+1))/2;
            my $sij = 0;
            for( my $k = 0; $k < $j; $k++, $ii++, $jj++ ) {
                $sij += $N->[$ii]*$N->[$jj];
                }
            $sij = $N->[$ii]-$sij;
            if( $i == $j ) {
                $self->{solved} = 2;
                die "Equations are singular\n" if $sij < 1.0e-10;
                $N->[$ii] = sqrt($sij);
                }
            else {
                $N->[$ii] = $sij/$N->[$jj];
                }
            }
         }

      # Solution of matrix ..
      for( my $i = 0; $i < $n; $i++ ) {
         my $ii = ($i*($i+1))/2;
         my $s = 0.0;
         for( my $k = 0; $k < $i; $k++, $ii++ ) { $s += $b->[$k]*$N->[$ii]; }
         $b->[$i] = ($b->[$i]-$s)/$N->[$ii];
         }
    
      my $nn = ($n*($n-1))/2;
      for( my $i = $n; $i--; ) {
         my $s = 0.0;
         my $ii = $nn+$i;
         for( my $k = $n-1; $k > $i; $ii -= $k, $k-- ) { 
           $s += $b->[$k] * $N->[$ii]; 
           }
         $b->[$i] = ($b->[$i]-$s)/$N->[$ii];
         }
      $self->{solved} = 1;       
      }
  die "Equations are singular\n" if $self->{solved} != 1;
  return $self->{b};
  }


  
  
