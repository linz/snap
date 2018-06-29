#ifndef _STRARRAY_H
#define _STRARRAY_H

/*

*/

typedef struct
{
    char **strings;
    int nstrings;
    int maxstrings;
} strarray;


void strarray_init( strarray *stra );
void strarray_delete( strarray *stra );

int strarray_count( strarray *stra );
int strarray_find( strarray *stra, char *string );
int strarray_add( strarray *stra, char *string );
char *strarray_get( strarray *stra, int i );

void dump_strarray( strarray *stra, FILE *f );
void reload_strarray( strarray *stra, FILE *f );

#define STRARRAY_NOT_FOUND -1

#endif
