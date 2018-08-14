#ifndef FILELIST_H
#define FILELIST_H

#define NO_FILENAME_ID -1

/* Returns previous state of recording filenames */
int set_record_filenames( int record );
int record_filename( const char *filename, const char *filetype );
int recorded_filename_count();    
/* Note: filenames are 0 based */
const char *recorded_filename( int i, const char **pfiletype );
void delete_recorded_filenames();

#endif
