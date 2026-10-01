#ifndef _DATAFILE_H
#define _DATAFILE_H

/*
   $Log: datafile.h,v $
   Revision 1.2  2004/04/22 02:35:24  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 18:56:46  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#ifndef IOSTRING_H
#include "util/iostring.h"
#endif

inline constexpr char DATAFILE_CONTINUATION_CHR = '&';
inline constexpr char DATAFILE_COMMENT_CHR = '!';

/// Byte order marks that make DATAFILE::open reject a file as unicode
inline constexpr std::string_view DATAFILE_UTF8_BOM = "\xEF\xBB\xBF";
inline constexpr std::string_view DATAFILE_UTF16_BOM = "\xFF\xFE\x46";

struct datafile_loc
{
    long loc;
    long line;
};

/// A text data file read a record at a time. Blank lines and comments are
/// skipped, a record can continue over several lines, and errors are reported
/// with the file name and line number. The fields of the current record are
/// read with the scanner of input_string(). The file is closed when the
/// DATAFILE is destroyed.
class DATAFILE
{
public:
    /// Opens a data file for reading. A failure is reported with handle_error
    /// and returns null, as does a file that is unicode or binary.
    static std::unique_ptr<DATAFILE> open(
        std::string_view fname,          ///< the file to open
        std::string_view description );  ///< what the file is, for messages and its recorded file type

    /// Sets the capacity initially reserved for the record of each data file
    /// opened from now on, if newlen is more than 80. Returns the previous capacity.
    static int default_reclen( int newlen );

    DATAFILE( const DATAFILE & ) = delete;
    DATAFILE &operator=( const DATAFILE & ) = delete;
    ~DATAFILE();

    const std::string &file_name() const { return _fname; }
    FILE *file() const { return _f; }
    void set_comment( char comment ) { _comment_char = comment; }
    void set_continuation( char continuation ) { _continuation_char = continuation; }

    /// Skips to the next blank line. Returns OK, or NO_MORE_DATA at the end of the file.
    int skip_to_blank_line();

    /// Reads the next record that is not blank, and starts a new scanner at the
    /// beginning of it. Returns OK, or NO_MORE_DATA at the end of the file.
    int read_record();

    /// The scanner over the current record, reporting errors through error().
    /// It is the same scanner until the next record is read, so the fields
    /// read so far are not read again.
    input_string_def &input_string() { return *_instr; }

    long line_number() const { return _reclineno; }

    /// Reports an error in the current record, giving the line number if a
    /// record has been read and always the file name. Errors at warning level
    /// or above are counted in error_count().
    /// \return sts
    int error(
        int sts,                   ///< the error status
        std::string_view errmsg ); ///< what is wrong with the record
    int error_count() const { return _errcount; }
    void save_loc( datafile_loc &dl ) const;
    void reset_loc( const datafile_loc &dl );

private:
    DATAFILE() = default;

    /// Starts a new scanner at the beginning of the current record
    void start_scanner();

    /// The input_string_errfunc of the input_string_def. Reports the error
    /// against the DATAFILE that is the source.
    static int report_input_string_error(
        void *source,              ///< the DATAFILE, as set by start_scanner()
        int sts,                   ///< the error status
        std::string_view errmsg ); ///< what is wrong with the record

    std::string _fname;
    FILE *_f = nullptr;         ///< owned, and closed when the DATAFILE is destroyed
    long _startloc = 0;
    long _startlineno = 0;
    long _reclineno = 0;
    long _lineno = 0;
    int  _errcount = 0;
    std::string _inrec;         ///< the current record, with continuation lines joined
    char _comment_char = DATAFILE_COMMENT_CHR;
    char _continuation_char = DATAFILE_CONTINUATION_CHR;
    std::optional<input_string_def> _instr; ///< the scanner over _inrec, constructed afresh by each read_record()
};

#endif
