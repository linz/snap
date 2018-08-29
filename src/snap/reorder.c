#include "snapconfig.h"
/* Station reordering is done to minimize matrix bandwidth.  The algorithm
   used is from

   Sloan S.W. (1986): An algorithm for profile and wavefront reduction of
   sparse matrices: International Journal for Numerical Methods in
   Engineering,  Vol 23, pp239-251.
*/

/*
   $Log: reorder.c,v $
   Revision 1.2  2004/04/22 02:35:44  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1996/01/03 22:05:28  CHRIS
   Initial revision

*/

#include <stdio.h>

#ifndef TEST

#include "snap/snapglob.h"
#include "snap/stnadj.h"
#include "stnobseq.h"
#include "output.h"
#include "util/geodetic.h"
#include "util/chkalloc.h"
#include "util/xprintf.h"
#include "util/leastsqu.h"
#include "util/bltmatrx.h"
#include "adjparam.h"

#else

#include "util/chkalloc.h"

#endif

#define REORDER_C
#include "reorder.h"
#undef REORDER_C

#define NO_CONNECTIONS 10
#define REORDER_THRESHOLD 50


#define UNUSED     0
#define INACTIVE   1
#define PREACTIVE  2
#define ACTIVE     3
#define POSTACTIVE 4

#define W1 2
#define W2 1

typedef struct
{
    int count;
    int maxcount;
    int status;
    int level;
    int *list;
} connections;


static connections *connlst = NULL;
static int maxconnlst;
static int nconnlst;
static int lastnode;
static int *order1, *order2;

#define CONNLIST_INC 128

/* Routines to maintain the list of connections */

static void init_connection( connections *conn )
{
        conn->count = conn->maxcount = 0;
        conn->list = NULL;
}

int init_connections( int nnodes )
{
    int i;

    if( !save_connections ) return 0;
    if( connlst ) return 1;

    connlst = (connections *) check_malloc( (nnodes+1) * sizeof(connections) );
    for( i = 0; i++ < nnodes; )
    {
        init_connection( connlst+i);
    }
    maxconnlst = nconnlst = lastnode = nnodes;

    return 1;
}

static void grow_connection_list( int nnodes )
{
    int maxconn0=maxconnlst;
    while( nnodes > maxconnlst ) maxconnlst += CONNLIST_INC;
    if( maxconnlst > maxconn0 )
    {
        int i;
        connlst = (connections *) check_realloc( connlst, (maxconnlst+1) * sizeof(connections) );
        for( i=maxconn0+1; i<=maxconnlst; i++ )
        {
            init_connection(connlst+i);
        }
    }
}

void term_connections( void )
{
    int i;
    if( !connlst ) return;
    for( i = 0; i++ < nconnlst; )
    {
        if( connlst[i].list ) check_free( connlst[i].list );
    }
    check_free( connlst );
    check_free( order1 );
    connlst = NULL;
    nconnlst = 0;
}

void create_order_arrays( void )
{
    if( order1 ) check_free(order1);
    if( order2 ) check_free(order2);
    order1 = (int *) check_malloc( (nconnlst+1)*2*sizeof(int) );
    order2 = order1 + nconnlst + 1;
    for( int i=0; i<=nconnlst;i++){ order1[i]=0; order2[i]=0; }
}


static void add_station_connection( int stn, connections *conn )
{
    int ncount;
    int i;
    int *list;

    for( i = conn->count, list = conn->list; i--; list++ )
    {
        if( *list == stn ) return;
    }

    i = conn->count;
    ncount = conn->maxcount;
    if( i >= ncount )
    {
        conn->maxcount += NO_CONNECTIONS;
        if( !ncount )
        {
            conn->list = (int *) check_malloc( conn->maxcount * sizeof(int) );
        }
        else
        {
            conn->list =
                (int *) check_realloc( conn->list, conn->maxcount * sizeof(int) );
        }
    }
    conn->list[i] = stn;
    conn->count++;
}


void add_connection( int stn1, int stn2 )
{
    if( connlst == NULL ) return;
    if( stn1 > nconnlst || stn2 > nconnlst )
    {
        int maxstn=stn1 > stn2 ? stn1 : stn2;
        lastnode=nconnlst=maxstn;
        if( maxstn > maxconnlst ) grow_connection_list( maxstn );
    }
    add_station_connection( stn1, connlst+stn2 );
    add_station_connection( stn2, connlst+stn1 );
}

int get_connection_count( int stn )
{
    if( connlst == NULL ) return -1;
    if( stn > nconnlst ) return 0;
    return connlst[stn].count;
}

int get_connection( int stn, int nconn )
{
    connections *conn;
    if( connlst == NULL ) return -1;
    if( stn > nconnlst ) return -1;
    conn = connlst+stn;
    if( nconn < 0 || nconn >= conn->count ) return -1;
    return conn->list[nconn];
}


/* Routine to form a rooted level tree, returning the width and depth */
/* of the structure - aborts the construction if the width exceeds a  */
/* maximum value, and returns 0.                                      */

/* Ignore nodes for which order1[i] is not zero                       */

static int make_rooted_level_structure( int stn, int *width, int maxwidth )
{
    int i, l, w, wl, *list, c;

    for( i = 0; i++ < nconnlst; ) connlst[i].level = nconnlst+1;

    connlst[stn].level = 1;
    l = w = 1;
    wl = 1;

    while( wl )
    {
        wl = 0;
        for( i = 0; i++ < nconnlst; ) if( connlst[i].level == l )
            {
                for( list = connlst[i].list, c=connlst[i].count; c--; list++ )
                {
                    if( order1[*list] == 0 && connlst[*list].level > nconnlst )
                    {
                        connlst[*list].level = l+1;
                        wl++;
                    }
                }
            }
        if( w >= maxwidth ) return 0;
        if( wl > w ) w = wl;
        if( wl ) l++;
    }

    *width = w;
    return l;
}


/* Form a list of the end levels in integer array list */
/* Sort by degree and return a count of elements       */

static int form_end_list( int *list, int level )
{
    int ncount, i, nc, dg;

    ncount = 0;
    for( i=0; i++<nconnlst; ) if( connlst[i].level == level )
        {
            dg = connlst[i].count;
            for( nc = ncount; nc; nc-- )
            {
                if( connlst[list[nc-1]].count <= dg ) break;
                list[nc] = list[nc-1];
            }
            list[nc] = i;
            ncount++;
        }
    return ncount;
}


/* Find the proposed start and end nodes of the ordering */

static int find_end_nodes( int *s, int *e )
{

    /* Find a node of lowest degree                 */
    /* Ignore nodes for which order1[i] is not zero */
    /* If none found then we are finished.          */

    int start, end, nend, deg, wmin, hmax, w, h, i;
    deg  = nconnlst+1;
    start = 0;

    for( i=0; i++ < nconnlst; )
    {
        if( order1[i] == 0 && deg > connlst[i].count )
        {
            deg = connlst[i].count;
            start = i;
        }
    }

    if( !start ) return 0;

    /* See if we can find a better start while looking for an end
       suitably distant from the start */

    for(;;)
    {

        /* Form the rooted level structure - if it is of height 1 then
        there is just the one node in the structure, so return it */

        hmax = make_rooted_level_structure( start, &wmin, nconnlst+1 );
        if( hmax == 1 )
        {
            *s = *e = start;
            return 1;
        }

        /* Otherwise form the list of the deepest nodes and search it
        for an end node. */

        nend = form_end_list( order2, hmax );
        nend = (nend+2)/2;   /* Only use the first half of the list */

        /* For each of these ... */

        wmin = nconnlst + 1;
        end = 0;
        for( i=0; i<nend; i++ )
        {
            h = make_rooted_level_structure( order2[i], &w, wmin );
            if( !h ) continue;
            if( h > hmax )
            {
                start = order2[i];
                end = 0;
                break;
            }
            else
            {
                wmin = w;
                end = order2[i];
            }
        }

        /* If we found an end then quit, otherwise try again with the
        new start */

        if( end ) break;
    }

    *s = start;
    *e = end;
    return 1;
}


/* Given a start and end node - form the list of connected nodes.  For
   each node set the corresponding element of order1.  */

static int order_connected_nodes( int start, int end, int seqno )
{
    int npending, i, i1, i2, *list1, *list2, h, w, nmax;
    int next, maxpriority;

    /* If it is a degenerate list assign the element and return */

    if( start == end )
    {
        order1[start] = seqno++;
        return seqno;
    }

    h = make_rooted_level_structure( end, &w, nconnlst+1 );

    /* Find the connected node with maximum count */

    nmax = 0;
    for( i=0; i++<nconnlst; )
    {
        if( connlst[i].level <= h &&
                connlst[i].count > nmax ) nmax = connlst[i].count;
    }

    /* Assign initial priorities in the level member */
    /* Unconnected points are set to priority 0      */

    for( i = 0; i++ < nconnlst; )
    {
        if( connlst[i].level <= h )
        {
            connlst[i].level = (nmax - connlst[i].count)*W1 + connlst[i].level*W2;
            connlst[i].status = INACTIVE;
        }
        else
        {
            connlst[i].level = 0;
            connlst[i].status = UNUSED;
        }
    }

    /* Start the priority queue in order2 */

    npending = 1;
    order2[0] = start;
    connlst[start].status = PREACTIVE;

    while( npending )
    {

        next = maxpriority = 0;

        for( i = 0; i<npending; i++ )
        {
            if( connlst[order2[i]].level > maxpriority )
            {
                next = i;
                maxpriority = connlst[order2[i]].level;
            }
        }



        /* Remove this node from the queue and note its sequence number */

        i = next;
        next = order2[next];
        while( ++i < npending )
        {
            order2[i-1] = order2[i];
        }
        npending--;
        order1[next]=seqno++;

        /* If it is preactive make all adjacent inactive nodes preactive */

        if( connlst[next].status == PREACTIVE )
        {
            list1 = connlst[next].list;
            for( i=connlst[next].count; i--; list1++ )
            {
                if( connlst[*list1].status == UNUSED ) continue;
                connlst[*list1].level += W1;
                if( connlst[*list1].status == INACTIVE )
                {
                    connlst[*list1].status = PREACTIVE;
                    order2[npending++] = *list1;
                }
            }
        }

        connlst[next].status = POSTACTIVE;

        /* Check out adjacent preactive nodes */

        for( list1 = connlst[next].list, i1 = connlst[next].count; i1--; list1++)
        {
            if( connlst[*list1].status != PREACTIVE ) continue;
            connlst[*list1].level += W1;
            connlst[*list1].status = ACTIVE;

            for( list2 = connlst[*list1].list, i2 = connlst[*list1].count;
                    i2--; list2++ )
            {
                if( connlst[*list2].status == UNUSED ) continue;
                connlst[*list2].level += W1;
                if( connlst[*list2].status == INACTIVE )
                {
                    connlst[*list2].status = PREACTIVE;
                    order2[npending++] = *list2;
                }
            }
        }
    }

    /* Return the next available sequence number */

    return seqno;
}


/* Routine to order all stations */

static void order_nodes( void )
{
    int seqno, start, end;

    /* Initiallize the order array order1.  In the end each element will
       contain the sequence number of the corresponding station, starting
       with number 1.  Number 0 means not yet assigned - stations which
       are not connected or which have the noreorder flag set are immediately
       assigned order numbers starting at the top of the list. */

    seqno = 1;

    while( find_end_nodes( &start, &end ) )
    {
        seqno = order_connected_nodes( start, end, seqno );
    }
}

/* Exclude a node from the graph */

static void exclude_node( int node )
{
    order1[node] = lastnode--;
}

static void dump_connection_list( FILE *lst )
{
    int i1;
    fprintf(lst,"\nConnection list\n");
    for( i1=0; i1++ < nconnlst; )
    {
        int c;
        fprintf(lst,"   %3d =>",i1);
        for( c=0; c < connlst[i1].count; c++ )
        {
            if( c && c % 15 == 0 ) fprintf(lst,"\n         ");
            fprintf(lst," %3d",(int)(connlst[i1].list[c]));
        }
        fprintf(lst,"\n");
    }
}

static void dump_ordering( FILE *lst )
{
    int i1;
    station *stn;

    for( i1=0; i1++<nconnlst; )
    {
        order2[order1[i1]] = i1;
    }

    fprintf(lst,"\nFinal order...\n");

    for( i1=0; i1++<nconnlst; )
    {
        stn=station_ptr(net,order2[i1]);
        fprintf(lst,"   %s\n",stn->Code);
    }
}


#ifndef TEST


/* Set row numbers does two jobs.  Firstly it allocates row numbers for
   each station.  Secondly it records all connections from a station to
   a lower numbered station */

static void set_row_numbers( int nprm )
{
    bltmatrix *N;
    stn_adjustment *sa;
    int nrow, lowest, i, istn, irow;
    int count, *list;

    N = lsq_normal_matrix();

    /* Set the connection list lowest pointer to nprm+1 for all stations
       so that we know which have not been connected yet.  This will
       be reset to the row number as the stations are allocated.

       Set order2 to the inverse permutation of that in order1.  */

    for( istn = 0; istn++ < nconnlst; )
    {
        connlst[istn].level = nprm+1;
        order2[order1[istn]] = istn;
    }

    /* Now work backwards through the defined station order */

    nrow = 1;

    for( i = 0; i++ < nconnlst; )
    {
        istn = order2[i];
        sa = stnadj(stnptr(istn));

        if( sa->hrowno <= 0 && sa->vrowno <= 0 ) continue;   /* Not used */

        /* Set the row number */

        lowest = nrow;
        nrow += sa->hrowno + sa->vrowno;
        connlst[istn].level = lowest;
        if( sa->hrowno>0 )
        {
            sa->hrowno = lowest;
            if( sa->vrowno>0 ) sa->vrowno = lowest+2;
        }
        else
        {
            sa->vrowno = lowest;
        }

        /* Check through the connections to find a connection to a lower
        numbered station. */

        for( count = connlst[istn].count, list = connlst[istn].list;
                count--; list++ )
        {
            if( connlst[*list].level < lowest ) lowest = connlst[*list].level;
        }

        /* Register the bandwidth required with the banded matrix */

        for( irow = connlst[istn].level; irow < nrow; irow++ )
        {
            blt_nonzero_element( N, irow-1, lowest-1 );
        }
    }
}

/* Set the noreorder flag for stations which are not observed to or from */

void exclude_stations( void )
{
    int istn;
    stn_adjustment *sa;
    for( istn = 0; istn++ < nconnlst; )
    {
        sa = stnadj(stnptr(istn));
        if( (sa->hrowno <= 0 && sa->vrowno <= 0) || sa->flag.noreorder )
            exclude_node( istn );
    }
}



/* This routine initiallizes the least squares adjustment.  It allocates
   memory for the least squares, determines parameters numbers for all
   parameters involved, and if required reorders the stations to give an
   optimal ordering. */


int setup_parameters( FILE *lst )
{
    int nrowst, nprm, nrow, istn, maxstn;

    /* Count the coordinate parameters and set sa->hrowno and sa->vrowno
       to indicate which stations are being adjusted (and how many parameters
       each has */

    nrowst = init_station_rowno( );
    nprm = init_param_rowno( nrowst+1 );
    nprm--;

    /* Allocate space for the least squares equations - readies them
       for taking bandwidth information */

    lsq_alloc( nprm );

    /* If there are no stations to reorder, or if there are only a few
       and we are using default reordering, then skip the reordering */

    if( ! save_connections ||
            reorder_stations == SKIP_REORDERING ||
            nrowst == 0 ||
            (reorder_stations == DEFAULT_REORDERING && nrowst < REORDER_THRESHOLD))
    {
        reorder_stations = SKIP_REORDERING;
    }
    else
    {
        reorder_stations = FORCE_REORDERING;
    }

    /* If we have saved connections then use them to generate an optimal
       ordering of the stations */

    if( reorder_stations != SKIP_REORDERING )
    {

        xprintf("\nReordering the stations\n");

        if( output_debug_reordering ) 
        {
            print_section_header( lst, "Station row reordering" );
            dump_connection_list( lst );
        }
        create_order_arrays();
        exclude_stations();   /* for unobserved stations */
        order_nodes();
        set_row_numbers( nrowst );
        blt_set_sparse_rows( lsq_normal_matrix(), nrowst );
        if( output_debug_reordering ) 
        {
            dump_ordering( lst );
            print_section_footer( lst );
        }
    }

    /* Otherwise just number them in their current order */

    else
    {
        maxstn = number_of_stations(net);
        nrow = 1;
        for (istn = 0; istn++ < maxstn; )
        {
            stn_adjustment *sa;
            sa = stnadj(stnptr(istn));
            if( sa->hrowno ) {sa->hrowno = nrow; nrow += 2;}
            if( sa->vrowno ) {sa->vrowno = nrow; nrow ++;}
        }
    }
    return nprm;
}


#else /* if TEST is defined */


int main( int argc, char *argv[] )
{
    FILE *in;
    int i1, i2, ncon;

    if( argc < 2 || !(in = fopen(argv[1],"r")) )
    {
        printf("Missing or invalid file name");
    }

    save_connections = 1;

    fscanf(in,"%d",&ncon);
    init_connections( ncon );
    while( fscanf(in,"%d%d",&i1,&i2) == 2 )
    {
        if( i1 )
        {
            add_connection( i1, i2 );
        }
        else
        {
            exclude_node( i2 );
        }
    }
    dump_connection_list(stdout);
    order_nodes();
    dump_ordering(stdout);
}


#endif
