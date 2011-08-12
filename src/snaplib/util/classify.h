#ifndef _CLASSIFY_H
#define _CLASSIFY_H

/*
   $Log: classify.h,v $
   Revision 1.1  1995/12/22 17:41:14  CHRIS
   Initial revision

*/

#ifndef CLASSIFY_H_RCSID
#define CLASSIFY_H_RCSID "$Id: classify.h,v 1.1 1995/12/22 17:41:14 CHRIS Exp $"
#endif

#define CLASS_TYPE_CHAR 0
#define CLASS_TYPE_INT  1
#define CLASS_VALUE_NOT_DEFINED -1

typedef struct class_value_s
{
    union
    {
        char *name;
        int value;
    } value;
    unsigned char usage;
    double error_factor;
} class_value;

typedef struct class_type_s
{
    char *name;
    class_value **value;
    int type;
    int count;
    int alloc_size;
    char valuebuf[20];
} class_type;

typedef struct classifications_s
{
    class_type **class_index;
    int class_count;
    int class_index_size;
} classifications;

/* Conversion to/from name and index, and count of names */

/* Note: class id is 1 based ... */
/* Class value id is 0 based - 0 is the default value always defined  ... */

void init_classifications( classifications *csf );
void delete_classifications( classifications *csf );
int classification_count( classifications *csf );

int classification_id( classifications *csf, const char *name, int create );
char * classification_name( classifications *csf, int id );
int classification_type( classifications *csf, int class_id );

void set_default_class_value( classifications *csf, int class_id, char *dflt );
int class_value_id( classifications *csf, int class_id, const char *name, int create );
char * class_value_name( classifications *csf, int class_id, int id );
int class_value_count( classifications *csf, int class_id );

void set_class_flag( classifications *csf, int class_id, int val_id, unsigned char flagbit );
void set_class_reject( classifications *csf, int class_id, int val_id );
void set_class_error_factor( classifications *csf, int class_id, int val_id, double ef );

unsigned char get_class_usage( classifications *csf, int class_id, int val_id );
double get_class_errfct( classifications *csf, int class_id, int val_id );

void dump_classifications( classifications *csf, FILE *f );
int reload_classifications( classifications *csf, FILE *f );

#endif

