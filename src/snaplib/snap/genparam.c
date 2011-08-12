#include "snapconfig.h"
/*


This code manages "general" parameters - meaning any parameters in the
adjustment other than the station related parameters (ie coordinates).

The routines provide the following capabilities:

   Maintaining a list of named parameters
   Selecting parameters to be used in adjustments, and automatically
      rejecting any parameters which are not referenced in any observations
   Specifying that two parameters are to be constrained as identical.
   Specifying and selecting parameters using a (very limited) wildcard which
      matches any parameter with a specified prefix.

To do this it initially builds two lists

   1) A list of parameters
   2) A list of specifications.  The specifications, defined in the
      prm_action structure, include defining parameter values and defining
      identical parameters, either explicitely or as wild cards.
After the lists are complete (when parameters are first used), the
specifications are applied to the list of parameters.

The procedure requires the following sequence of calls

   define_param     create a parameter  |  wildcard_param_value
                                        |  wildcard_param_match
   define_param_value  } in any order   |
   define_param_match  }                |
   flag_param_used     }                |

       ------------------------------------------------

              init_param_rowno

	      ....


*/


/*
   $Log: genparam.c,v $
   Revision 1.2  1999/05/18 14:38:30  ccrook
   Fixing minor bug.

   Revision 1.1  1995/12/22 17:43:38  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "util/chkalloc.h"
#include "util/dstring.h"
#include "snap/genparam.h"
#include "util/linklist.h"
#include "util/binfile.h"
#include "util/errdef.h"

static char rcsid[]="$Id: genparam.c,v 1.2 1999/05/18 14:38:30 ccrook Exp $";

typedef struct
{
    unsigned char action;
    union
    {
        param *p;
        char  *n;
    } prm;
    union
    {
        double v;
        param *p;
    } value;
} prm_action;

#define PA_SET       1
#define PA_ADJ       2
#define PA_MATCH     3
#define PA_WILDCARD  8

#define PRMLIST_INC 20

static int nparam = 0;
static int nprmlist = 0;
static param** prmlist = NULL;
static int* srtlist = NULL;
static void *action_list = NULL;

int param_count( void )
{
    return nparam;
}

static unsigned int hash( char *name )
{
    unsigned int h;
    for( h = 0; *name; name++ ) h = (h<<3) + h + *name;
    return h;
}

static param *reset_param( param *p, double value, int adjust )
{
    p->value = value;
    p->covar = 0.0;
    if( adjust )
    {
        p->flags |= PRM_ADJUST;
    }
    else
    {
        p->flags &= ~PRM_ADJUST;
    }
    return p;
}

static void sort_param( int n )
{
    char *name;
    int np;

    np = srtlist[n];
    name = param_name( np );
    while( n > 0 )
    {
        if( _stricmp( param_name(srtlist[n-1]), name ) <= 0 ) break;
        srtlist[n] = srtlist[n-1];
        n--;
    }
    srtlist[n] = np;
}

int define_param( char *name, double value, int adjust )
{
    int p;
    param *prm;

    p = find_param( name );
    if( !p )
    {
        prm = (param *) check_malloc( sizeof(param));
        prm->name = copy_string( name );
        prm->hash = hash(name);
        prm->identical = 0;
        prm->flags = 0;

        if( nparam >= nprmlist )
        {
            nprmlist = nparam + PRMLIST_INC;
            prmlist = (param **) check_realloc( prmlist, nprmlist * sizeof(param));
            srtlist = (int *) check_realloc( srtlist, nprmlist * sizeof(int));
        }

        prmlist[nparam] = prm;
        srtlist[nparam] = nparam+1;
        nparam++;
        sort_param(nparam-1);
        p = nparam;
    }
    reset_param( prmlist[p-1], value, adjust );
    return p;
}

void flag_param_used( int p )
{
    if( p ) prmlist[p-1]->flags |= PRM_USED;
}

void flag_param_listed( int p )
{
    if( p ) prmlist[p-1]->flags |= PRM_LISTED;
}

static int get_param_id( param *p )
{
    int i;
    for( i = 0; i < nparam; i++ )
    {
        if( prmlist[i] == p ) return i+1;
    }
    return 0;
}

param * param_from_id( int pid )
{
    if( pid <= 0 || pid > nparam ) return 0;
    return prmlist[pid - 1];
}

static param *identical_param_ptr( param *p )
{
    if( !p->identical ) return p;
    return prmlist[p->identical-1];
}

int sorted_param_id( int n )
{
    if( n <= 0 || n > nparam ) return 0;
    return srtlist[n-1];
}

static void merge_params( param *p1, param *p2 )
{
    int p2id;
    int np;
    if( !nparam ) return;

    p1 = identical_param_ptr( p1 );
    p2 = identical_param_ptr( p2 );

    if( p1 == p2 ) return;

    p1->value = p2->value;

    p2->flags |= p1->flags & PRM_USED;
    p1->flags = p2->flags;

    if( _stricmp(p1->name, p2->name) > 0  )
    {
        param *ptmp;
        ptmp = p1;
        p1 = p2;
        p2 = ptmp;
    }

    p2->identical = get_param_id( p1 );
    p2id = get_param_id( p2 );
    for( np = 0; np < nparam; np++ )
    {
        if( prmlist[np]->identical == p2id )
            prmlist[np]->identical = p2->identical;
    }

}

int find_param( char *name )
{
    int p;
    unsigned int hashedname;
    if( !nparam ) return 0;
    hashedname = hash(name);
    for( p = 0; p < nparam; p++ )
    {
        if( hashedname == prmlist[p]->hash &&
                strcmp( prmlist[p]->name, name ) == 0 ) return p+1;
    }
    return 0;
}

double param_value( int p )
{
    return prmlist[p-1]->value;
}

void update_param_value( int pid, double value, double var )
{
    param *p;
    p = param_from_id( pid );
    if( p )
    {
        p->value = value;
        p->covar = var;
    }
}

char *param_name( int p )
{
    return p ? prmlist[p-1]->name : "";
}


static void do_action( prm_action *pa )
{
    switch( pa->action )
    {
    case PA_SET:   reset_param( pa->prm.p, pa->value.v, 0 ); break;
    case PA_ADJ:   reset_param( pa->prm.p, pa->value.v, 1 ); break;
    case PA_MATCH: merge_params( pa->prm.p, pa->value.p ); break;
    }
}

static void do_prm_actions( void )
{
    prm_action *pa;
    prm_action apa;
    param *p;
    int np;
    int matchlen;

    if( !action_list ) return;

    reset_list_pointer( action_list );
    while ( NULL != (pa = (prm_action *) next_list_item(action_list)) )
    {
        if( pa->action & PA_WILDCARD )
        {
            memcpy( &apa, pa, sizeof( prm_action ) );
            apa.action &= ~PA_WILDCARD;
            matchlen = strlen( pa->prm.n );
            for( np=0; np < nparam; np++ )
            {
                p = prmlist[np];
                if( _strnicmp( pa->prm.n, p->name, matchlen ) != 0 ) continue;
                apa.prm.p = p;
                do_action( &apa );
            }
        }
        else
        {
            do_action( pa );
        }
    }
}

static void merge_common_params( void )
{
    param *p, *ip;
    int np;

    for( np = 0; np < nparam; np++ )
    {

        p = param_from_id(srtlist[np]);
        ip = identical_param_ptr(p);
        if( ip != identical_param_ptr(ip) )
        {
            merge_params( p, identical_param_ptr(ip) );
        }
    }
}

int find_param_row( int row, char *name, int nlen )
{
    param *p;
    int np;
    if( !nparam ) return 0;

    for( np = 0; np < nparam; np++ )
    {
        p = prmlist[np];

        if( p->rowno == row )
        {
            strncpy( name, p->name, nlen-1 );
            name[nlen-1] = 0;
            return 1;
        }
    }
    return 0;
}


int param_rowno( int pid )
{
    param *p;
    if( !pid ) return 0;
    p = identical_param_ptr(prmlist[pid-1]);
    if( !p ) return 0;
    return p->rowno;
}

int identical_param( int pid )
{
    param *p;
    if( !pid ) return 0;
    p = prmlist[pid-1];
    return p ? p->identical : 0;
}

static void add_prm_action( prm_action *pa )
{
    void *npa;
    if( !action_list ) action_list = create_list( sizeof(prm_action));
    npa = add_to_list( action_list, NEW_ITEM );
    memcpy( npa, pa, sizeof(prm_action) );
}


void define_param_value( int pid, double value, int adjust )
{
    prm_action pa;
    pa.prm.p = prmlist[pid-1];
    pa.value.v = value;
    pa.action = adjust ? PA_ADJ : PA_SET;
    add_prm_action( &pa );
}

void wildcard_param_value( char *name, double value, int adjust )
{
    prm_action pa;
    pa.prm.n = copy_string( name );
    pa.value.v = value;
    pa.action = (adjust ? PA_ADJ : PA_SET ) | PA_WILDCARD;
    add_prm_action( &pa );
}

void define_param_match( int pid1, int pid2 )
{
    prm_action pa;
    pa.prm.p = prmlist[pid1-1];
    pa.value.p = prmlist[pid2-1];
    pa.action = PA_MATCH;
    add_prm_action( &pa );
}

void wildcard_param_match( char *name, int pid )
{
    prm_action pa;
    pa.prm.n = copy_string( name );
    pa.value.p = prmlist[pid-1];
    pa.action = PA_MATCH | PA_WILDCARD;
    add_prm_action( &pa );
}


int init_param_rowno( int nextprm )
{
    param *p;
    int np;

    if( !nparam ) return nextprm;

    /* Sort needed for merging parameters to word correctly */
    /* Apply parameter constraints, etc */

    do_prm_actions();
    merge_common_params();

    for( np = 0; np < nparam; np++ )
    {
        p = param_from_id(srtlist[np]);
        p->rowno = 0;
        if( p->identical ) continue;
        if( !(p->flags & PRM_ADJUST ) ) continue;
        if( !(p->flags & PRM_USED) )
        {
            char errmsg[80];
            sprintf(errmsg,"Parameter %.40s cannot be calculated",p->name);
            handle_error( WARNING_ERROR, errmsg, NO_MESSAGE );
            continue;
        }
        p->rowno = nextprm++;
    }
    return nextprm;
}

void clear_param_list( void )
{
    if( action_list )
    {
        free_list( action_list, NO_ACTION );
        action_list = NULL;
    }
    if( nparam )
    {
        param *p;
        int np;
        for( np = 0; np < nparam; np++ )
        {
            p = prmlist[np];
            if( p )
            {
                if( p->name ) check_free( p->name );
                check_free( p );
            }
        }
        nparam = 0;
    }

    if( nprmlist )
    {
        check_free( prmlist );
        check_free( srtlist );
        prmlist = NULL;
        srtlist = NULL;
        nprmlist = 0;
    }
}


void dump_parameters( BINARY_FILE *b )
{
    int np, nprm;
    create_section( b, "MISCPARAMS" );
    nprm = nparam;
    fwrite( &nprm, sizeof(nprm), 1, b->f );
    for( np = 0; np < nparam; np++ )
    {
        fwrite( prmlist[np], sizeof(param), 1, b->f );
        dump_string( prmlist[np]->name, b->f );
    }
    fwrite( srtlist, sizeof(int), nparam, b->f );
    end_section( b );
}

int reload_parameters( BINARY_FILE *b )
{
    int np, nprm;
    clear_param_list();
    if( find_section( b, "MISCPARAMS" ) != OK ) return MISSING_DATA;
    if( fread( &nprm, sizeof(nprm), 1, b->f ) != 1 ) return INVALID_DATA;
    if( nprm )
    {
        nprmlist = nprm;
        prmlist = (param **) check_malloc( nprm * sizeof(param *) );
        srtlist = (int *) check_malloc( nprm * sizeof(int) );
    }
    for( np = 0; np < nprm; np++ )
    {
        param *p;
        p = (param *) check_malloc( sizeof(param) );
        if( fread( p, sizeof(param), 1, b->f ) != 1 ) return INVALID_DATA;
        p->name = reload_string( b->f );
        if( !p->name ) return INVALID_DATA;
        prmlist[np] = p;
    }
    if( (int) fread(srtlist,sizeof(int),nprm,b->f) != nprm ) return INVALID_DATA;
    nparam = nprm;

    return check_end_section( b );
}


