#ifndef BLTMATRX_MT_H
#define BLTMATRX_MT_H

#define BLT_DEFAULT_THREADS -1

void blt_set_number_of_threads( int threadcount );
void blt_chol_inv_mt( bltmatrix *blt );

#endif
