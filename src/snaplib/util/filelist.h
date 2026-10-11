#ifndef FILELIST_H
#define FILELIST_H

#include <string>
#include <string_view>

#define NO_FILENAME_ID -1

/* Returns previous state of recording filenames */
int set_record_filenames( int record );
int record_filename( std::string_view filename, std::string_view filetype );
int recorded_filename_count();
/* Note: filenames are 0 based. Returns false, leaving the outputs unchanged,
 * if i is not a recorded filename */
bool recorded_filename( int i, std::string &filename, std::string &filetype );
void delete_recorded_filenames();

#endif
