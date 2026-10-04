#ifndef _WRITECSV_H
#define _WRITECSV_H

#include <fstream>
#include <memory>
#include <string>
#include <string_view>

/// File name extension for comma delimited output.
inline constexpr std::string_view WRITECSV_CSV_EXT = ".csv";

/// File name extension for tab delimited output.
inline constexpr std::string_view WRITECSV_TAB_EXT = ".txt";

/// Writes one comma separated (or tab delimited) file a field at a time. The
/// caller writes each field in turn and calls endRecord() after the last
/// field of a row. The destructor closes the file.
///
/// In csv mode text fields are enclosed in quotes, with a quote inside the
/// text doubled, so that commas and line feeds in the text survive. Tab
/// delimited files have no quoting, so a tab or a line feed in the text is
/// replaced by a space.
class output_csv
{
public:
    /// Opens a file for writing, replacing any existing file.
    /// \return the writer, or nullptr if the file cannot be opened.
    static std::unique_ptr<output_csv> open(
        const std::string &filename,  ///< the file to create
        bool tab_delimited );         ///< true for tab delimited output, false for csv

    /// Not copyable, as the object owns the open file.
    output_csv( const output_csv & ) = delete;
    output_csv &operator=( const output_csv & ) = delete;

    /// Ends the current record, so that the next field written starts a new row.
    void endRecord();

    /// Writes a column name as a field. Characters other than letters and
    /// digits are replaced by '_', and the name is cut to 32 characters, so
    /// that it is usable as a field name in other software.
    void writeHeader( std::string_view fieldname );

    /// Writes a text field, quoted in csv mode (see the class comment).
    /// Use writeNullField() for a missing value: an empty string is written
    /// as an empty quoted field in csv mode, not as an empty field.
    void writeString( std::string_view value );

    /// Writes an integer field.
    void writeInt( long value );

    /// Writes a floating point field.
    void writeDouble(
        double value,  ///< the number to write
        int ndp );     ///< the number of decimal places, or negative for six

    /// Writes a date field as yyyy-mm-dd hh:mm:ss, or a null field if the
    /// date is undefined.
    void writeDate( double date );

    /// Writes an empty field with no quotes, for a missing value.
    void writeNullField();

    /// Writes several empty fields with no quotes, for several missing values.
    void writeNullFields( int count );

private:
    /// Use open() to create an output_csv.
    output_csv(
        const std::string &filename,  ///< the file to create
        bool tab_delimited );         ///< true for tab delimited output, false for csv

    /// Starts a field by writing the delimiter that separates it from the
    /// previous field in the record, if there is one.
    void _writeDelimiter();

    std::ofstream _f;         ///< the file being written
    bool _delimit = false;    ///< true once a field has been written in this record
    char _delim;              ///< the field delimiter
    bool _quoted;             ///< true if text fields are enclosed in quotes
    std::string _delimrep;    ///< what is written as a replacement for a delimiter inside text
    std::string _newlinerep;  ///< what is written as a replacement for a line feed inside text
};

#endif
