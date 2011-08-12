#include "snapconfig.h"
/* classify.c: Manages the classification of observations into user
   defined classes, such as instrument, weather, session, or whatever.

   Each classification is defined by an index.

   Within each classification are a set of values (names) such as
   WEATHER
      GOOD
      BAD

   INSTRUMENT
      T2
      DI20

   And so on.  These names are assigned an index within the
   classification.

   Observations have associated with them a value for each classification
   that applies to them.
*/

/*
   $Log: classify.c,v $
   Revision 1.1  1995/12/22 17:40:45  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>

#include "util/binfile.h"
#include "util/chkalloc.h"
#include "util/dstring.h"
#include "util/classify.h"

#include "util/errdef.h"

#define BLOCK_SIZE 10

static char rcsid[]="$Id: classify.c,v 1.1 1995/12/22 17:40:45 CHRIS Exp $";

#ifdef DEBUG

#define CHECK_CLASS_ID( csf, id) check_class_id( csf, id )
#define CHECK_VALUE_ID( csf, cid,id) check_value_id( csf, cid, id );

void check_class_id( classifications *csf, int id )
{
    if( id <= 0 || id > csf->class_count )
    {
        handle_error( INTERNAL_ERROR, "Out of range classification id specified",NO_MESSAGE);
    }
}

void check_value_id( classifications *csf, int cid, int id )
{
    int cmax;
    check_class_id( csf, cid );
    cid--;
    if( csf->class_index[cid]->type == CLASS_TYPE_INT ) return;
    cmax = csf->class_index[cid]->count;
    if( id < 0 || id >= cmax )
    {
        handle_error( INTERNAL_ERROR, "Out of range class value id specified", NO_MESSAGE );
    }
}

#else
#define CHECK_CLASS_ID(csf,id)
#define CHECK_VALUE_ID(csf,cid,id)
#endif


void init_classifications( classifications *csf )
{
    csf->class_index = NULL;
    csf->class_index_size = 0;
    csf->class_count = 0;
}

static void resize_class_index( class_type *ct, int new_size )
{
    size_t alloc_size;
    class_value **ci;
    alloc_size = new_size * sizeof( class_value * );
    ci = ct->value;
    ci = (class_value **) check_realloc( ci, alloc_size );
    ct->alloc_size = new_size;
    ct->value = ci;
}

static class_value *create_class_value( class_type *ct )
{
    class_value *cv;

    if( ct->count >= ct->alloc_size )
    {
        int newsize = ct->alloc_size > 0 ? ct->alloc_size * 2 : BLOCK_SIZE;
        resize_class_index(ct,newsize);
    }
    cv = (class_value *) check_malloc( sizeof( class_value ) );
    cv->value.value = 0;
    cv->usage = 0;
    cv->error_factor = 1.0;
    ct->value[ct->count] = cv;
    ct->count++;
    return cv;
}

static int class_type_value_id( class_type *ct, const char *name, int create )
{
    int cmax;
    int i;
    class_value *cv;
    class_value **cvs;

    if( ct->type == CLASS_TYPE_INT )
    {
        int id;
        if( sscanf(name,"%d",&id) != 1 )
        {
            char errmess[80];
            sprintf(errmess,"Invalid value %.10s for integer class %.20s",name,ct->name);
        }
        return id;
    };

    cmax = ct->count;
    cvs = ct->value;

    for( i=0; i<cmax; i++ )
    {
        if( _stricmp(cvs[i]->value.name, name) == 0 ) return i;
    }
    if( ! create ) return CLASS_VALUE_NOT_DEFINED;

    cv = create_class_value(ct);
    cv->value.name = copy_string( name );
    return ct->count-1;
}

static class_type *create_class_type( const char*name, int type, char *dflt )
{
    class_type *ct = (class_type *) check_malloc( sizeof( class_type ) );
    ct->name = copy_string( name );
    ct->type = type;
    ct->count = 0;
    ct->alloc_size = 0;
    ct->value = NULL;
    /* Set up the default classification */
    if( type == CLASS_TYPE_CHAR ) class_type_value_id( ct, dflt ? dflt : "Default", 1 );
    return ct;
}

static void delete_class_type( class_type *ct )
{
    int nv;
    for( nv = 0; nv < ct->count; nv++ )
    {
        if( ct->type == CLASS_TYPE_CHAR ) check_free( ct->value[nv]->value.name );
        check_free( ct->value[nv]);
        ct->value[nv] = NULL;
    }
    check_free( ct->value );
    check_free( ct->name );
    check_free( ct );
}

void delete_classifications( classifications *csf )
{
    int nc;
    if( ! csf ) return;
    if( csf->class_index )
    {
        for( nc = 0; nc < csf->class_count; nc++ )
        {
            delete_class_type( csf->class_index[nc]);
            csf->class_index[nc] = NULL;
        }
        check_free( csf->class_index );
    }
    csf->class_index = 0;
    csf->class_count = 0;
    csf->class_index_size = 0;
}

static void resize_index( classifications *csf, int new_size )
{
    size_t alloc_size;
    alloc_size = new_size * sizeof( class_type * );
    csf->class_index = (class_type **) check_realloc( csf->class_index, alloc_size );
    csf->class_index_size = new_size;
}

static int find_classification_id( classifications *csf, const char *name )
{
    int i;
    int class_count = csf->class_count;
    for( i = 0; i<class_count; i++ )
    {
        if( _stricmp(name, csf->class_index[i]->name ) == 0 ) return i+1;
    }
    return 0;
}

static int create_classification( classifications *csf, const char *name, int type, char *dflt, int create )
{
    int class_count;
    class_type *ct;

    class_count = find_classification_id( csf, name );
    if( class_count ) return class_count;
    if( ! create ) return 0;

    class_count = csf->class_count;

    if( class_count >= csf->class_index_size )
    {
        int newsize = class_count * 2;
        if( newsize < BLOCK_SIZE ) newsize = BLOCK_SIZE;
        resize_index( csf, newsize );
    }

    ct = create_class_type( name, type, dflt );

    csf->class_index[class_count] = ct;
    class_count++;

    csf->class_count = class_count;


    return class_count;
}

int classification_id( classifications *csf, const char *name, int create )
{
    return create_classification( csf, name, CLASS_TYPE_CHAR, NULL, create );
}

int classification_id_integer( classifications *csf, char *name, int create )
{
    return create_classification( csf, name, CLASS_TYPE_INT, NULL, create );
}



char * classification_name( classifications *csf, int id )
{
    CHECK_CLASS_ID( csf, id );
    return csf->class_index[id-1]->name;
}


int classification_count(  classifications *csf )
{
    return csf->class_count;
}

void set_default_class_value( classifications *csf, int class_id, char *dflt )
{
    class_type *ct;

    CHECK_CLASS_ID(csf,class_id);
    class_id--;
    ct = csf->class_index[class_id];

    if( ct->type != CLASS_TYPE_CHAR ) return;
    if( ct->count > 0 )
    {
        class_value *cv = ct->value[0];
        check_free( cv->value.name );
        cv->value.name = copy_string( dflt );
    }
}

int class_value_id( classifications *csf, int class_id, const char *name, int create )
{
    class_type *ct;

    CHECK_CLASS_ID(csf,class_id);
    class_id--;

    ct = csf->class_index[class_id];
    return class_type_value_id( ct, name, create );
}

static class_value *get_class_value(  classifications *csf, int class_id, int val_id, int create )
{
    class_type *ct;
    class_value *cv;
    int i;

    CHECK_CLASS_ID(csf,class_id);
    ct = csf->class_index[class_id-1];

    if( ct->type == CLASS_TYPE_CHAR )
    {
        CHECK_VALUE_ID( csf, class_id, id );
        return ct->value[val_id];
    }

    for( i = 0; i < ct->count; i++ )
    {
        cv = ct->value[i];
        if( cv->value.value == val_id ) return cv;
    }

    if( ! create ) return NULL;

    cv = create_class_value(ct);
    cv->value.value = val_id;

    return cv;
}


char * class_value_name( classifications *csf, int class_id, int id )
{
    class_type *ct;

    CHECK_CLASS_ID(csf,class_id);
    ct = csf->class_index[class_id-1];
    if( ct->type == CLASS_TYPE_INT )
    {
        sprintf(ct->valuebuf,"%d",id);
        return ct->valuebuf;
    }
    CHECK_VALUE_ID( csv, class_id, id );
    return csf->class_index[class_id-1]->value[id]->value.name;
}

int class_value_count( classifications *csf, int class_id )
{
    class_type *ct;

    CHECK_CLASS_ID(csf,class_id);
    ct = csf->class_index[class_id-1];
    if( ct->type == CLASS_TYPE_INT ) return 0;

    CHECK_CLASS_ID( csf, class_id );
    return csf->class_index[class_id-1]->count;
}

void set_class_flag( classifications *csf, int class_id, int val_id, unsigned char flagbit )
{
    class_value *cv;
    CHECK_VALUE_ID( csf, class_id, val_id );
    cv=get_class_value(csf,class_id,val_id,1);
    cv->usage |= flagbit;
}

void set_class_error_factor( classifications *csf, int class_id, int val_id, double ef )
{
    class_value *cv;
    CHECK_VALUE_ID( csf, class_id, val_id );
    cv=get_class_value(csf,class_id,val_id,1);
    cv->error_factor = ef;
}

double get_class_errfct( classifications *csf, int class_id, int val_id )
{
    class_value *cv;
    CHECK_VALUE_ID( csf, class_id, val_id );
    cv=get_class_value(csf,class_id,val_id,0);
    return cv ? cv->error_factor : 1.0;
}

unsigned char get_class_usage( classifications *csf, int class_id, int val_id )
{
    class_value *cv;
    CHECK_VALUE_ID( csf, class_id, val_id );
    cv=get_class_value(csf,class_id,val_id,0);
    return cv ? cv->usage : 0;
}



/* Dump and reload classifications */

#define DUMP(x,f)   fwrite(&x,sizeof(x),1,f)

void dump_classifications( classifications *csf, FILE *f )
{
    int ic, iv;
    DUMP( csf->class_count, f );
    for(ic = 0; ic < csf->class_count; ic++ )
    {
        class_type *cl;
        cl = csf->class_index[ic];
        dump_string( cl->name, f );
        DUMP( cl->count, f );
        DUMP( cl->type, f );
        for( iv = 0; iv < cl->count; iv++ )
        {
            class_value *cv;
            cv = cl->value[iv];
            if( cl->type == CLASS_TYPE_INT )
            {
                DUMP( cv->value.value, f );
            }
            else
            {
                dump_string( cv->value.name, f );
            }
            DUMP( cv->usage, f );
            DUMP( cv->error_factor, f );
        }
    }
}

#define RELOAD(x,f) fread(&x,sizeof(x),1,f)

int reload_classifications( classifications *csf, FILE *f )
{
    int ic, iv;
    RELOAD( csf->class_count, f );
    if( csf->class_count )
    {
        csf->class_index = (class_type **) check_malloc( csf->class_count * sizeof(class_type *));
        csf->class_index_size = csf->class_count;
        for( ic = 0; ic<csf->class_count; ic++ )
        {
            class_type *cl;
            cl = (class_type *) check_malloc( sizeof(class_type) );
            csf->class_index[ic] = cl;
            cl->name = reload_string( f );
            RELOAD( cl->count, f );
            RELOAD( cl->type, f );
            cl->value = (class_value **) check_malloc( cl->count * sizeof(class_value *));
            cl->alloc_size = cl->count;
            for( iv = 0; iv < cl->count; iv++ )
            {
                class_value *cv;
                cv = (class_value *) check_malloc( sizeof( class_value ));
                cl->value[iv] = cv;
                if( cl->type == CLASS_TYPE_INT )
                {
                    RELOAD( cv->value.value, f );
                }
                else
                {
                    cv->value.name = reload_string( f );
                }
                RELOAD( cv->usage, f );
                RELOAD( cv->error_factor, f );
            }
        }
    }
    return OK;
}



