#define LOWER(c) ((c)>='A'&&(c)<='Z' ? (c)-'A'+'a' : (c))
#define UPPER(c) ((c)>='a'&&(c)<='z' ? (c)-'a'+'A' : (c))

char *strlwr(s) char *s;
{
    char *t;
    for (t=s; *t; t++) *t = LOWER(*t);
    return s;
}

char *strupr(s) char *s;
{
    char *t;
    for( t=s; *t; t++ ) *t = UPPER(*t);
    return s;
}

int stricmp(s1,s2) char *s1, *s2;
{
    while( LOWER(*s1)==LOWER(*s2) && *s1 ) {s1++; s2++;}
    return (int) (LOWER(*s2) - LOWER(*s1));
}

int strnicmp(s1,s2,n) char *s1,*s2; int n;
{
    while( LOWER(*s1)==LOWER(*s2) && *s1 && --n) {s1++; s2++;}
    return (int) (LOWER(*s2) - LOWER(*s1));
}

