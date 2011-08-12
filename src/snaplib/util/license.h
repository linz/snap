#ifndef _LICENSE_H
#define _LICENSE_H

/*
   $Log: license.h,v $
   Revision 1.2  2004/04/22 02:35:26  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 19:52:52  CHRIS
   Initial revision

*/

#ifndef LICENSE_H_RCSID
#define LICENSE_H_RCSID "$Id: license.h,v 1.2 2004/04/22 02:35:26 ccrook Exp $"
#endif

/* Code to handle an encrypted password */

char *decrypted_license( unsigned char *license_text );

#endif

