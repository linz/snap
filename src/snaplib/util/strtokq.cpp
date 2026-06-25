#include <string.h>
#include "util/strtokq.h"

/* This is a crude implementation if handling quoted strings with an strtok like
 * API.  It ignores badly formatted quoted strings, and provides no escaping of quotes
 */

char *strtokq( char *str, const char *delim )
{
    static char *laststr=0;
    static const char quote='"';
    char *s2;
    if( str == 0 )
    {
        str=laststr;
    }
    if( ! delim || ! *delim || ! str || ! *str )
    {
        laststr=0;
        return str;
    }
    while( *str && strchr(delim,*str))
    {
        str++;
    }
    s2=str;
    if( *str == quote )
    {
        str++;
        for( s2=str; *s2; s2++ ) 
        {
            /* Look for closing quote followed by delimiter or end of string */
            /* If not matched then continue */
            /* Ignore quotes in not followed by delimter.  No escaping of quotes */
            if( *s2 != quote ) continue;
            if( *(s2+1) && ! strchr(delim,*(s2+1))) continue;
            break;
        }
    }
    else
    {
        /* Ignore quotes within string not starting with quote */
        for( ; *s2; s2++ )
        {
            if( strchr(delim,*s2) ) break;
        }
    }
    /* Set the end of the string to 0, and then find the potential beginning 
     * of the next string
     */
    if( *s2 )
    {
        *s2=0;
        s2++;
        while( *s2 && strchr(delim,*s2) ) s2++;
    }
    laststr=*s2 ? s2 : 0;
    return str;
}

#ifdef TEST_STRTOKQ
#include <stdio.h>

int main()
{
    const char *strings[]={
        "This is a simple string",
        "  with spaces   \t\n before and after  ",
        "This one has \"a quoted string\"",
        "This one has not\"a quoted string\"",
        "This one \"has\" another \"quoted string\"",
        "This one \"an unterminated quoted string",
        "This one \"an muc\"ky quoted string",
        0,
    };

    for( const char **string=strings; *string; string++ )
    {
        char str[80];
        strcpy(str,*string);
        printf("======================================\n");
        printf("%s\n",str);
        for( char *sub=strtokq(str,"\r\n\t "); sub; sub=strtokq(0,"\r\n\t "))
        {
            printf("%s\n",sub);
        }
    }
    return 0;
}


#endif
