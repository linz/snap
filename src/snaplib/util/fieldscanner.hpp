// #pragma once
#ifndef _FIELDSCANNER_HPP
#define _FIELDSCANNER_HPP

#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// Which characters may immediately follow a quoted value's closing quote
/// for FieldScanner::quotedValue() to accept it - see quotedValue() itself.
/// Kept in the global namespace alongside FieldScanner, not nested inside
/// it, so callers write QuoteFollowOption::Whitespace rather than the more
/// verbose FieldScanner::QuoteFollowOption::Whitespace.
enum class QuoteFollowOption
{
    Whitespace,  ///< a whitespace character may follow
    End          ///< end of input may follow
};

/// Non-owning, non-destructive cursor over a whitespace-delimited field
/// sequence. Replaces next_field()/dstring.cpp's destructive char*-based
/// tokenization, which writes '\0' into the source buffer, with a
/// std::string_view-based scan that never modifies its input.
///
/// Kept in the global namespace (not LINZ::), matching next_field's own
/// declaration site in dstring.h, since its real callers are C-flavoured
/// legacy files that don't otherwise use the LINZ namespace.
class FieldScanner
{
public:
    /// \param text must outlive the FieldScanner - it is never copied, only viewed.
    explicit FieldScanner( std::string_view text ) : _text(text), _pos(_text.begin()) {}

    /// Returns the next whitespace-delimited field, or nullopt at end. Runs
    /// of consecutive whitespace are skipped as a single delimiter, matching
    /// next_field's behavior.
    std::optional<std::string_view> next();

    /// Returns the text up to the next occurrence of delimiter, consuming it -
    /// a single-character-delimited split, unlike the no-argument next().
    /// Two real behavioral differences from next(): consecutive delimiters
    /// are NOT collapsed (an empty field between two adjacent delimiters is
    /// returned as an empty string_view, not skipped), and on failure - no
    /// further delimiter found - this returns nullopt \em without advancing
    /// the position, so a subsequent remainder() call still gives the
    /// caller everything not yet split off. That "fails without consuming"
    /// behavior is what lets a caller do a bounded split (e.g. at most two
    /// delimiters, three fields) by calling this a fixed number of times and
    /// falling back to remainder() for the final field, whether or not that
    /// last delimiter was actually present.
    /// \return the field, or nullopt if delimiter doesn't occur again before
    ///         the end of input.
    std::optional<std::string_view> next( char delimiter );

    /// Unconsumed text from the current position to the end, verbatim - for
    /// handing the rest of the line off to another parser, or as a
    /// checkpoint a caller can capture and compare against a later
    /// remainder() (via .data()/.size() arithmetic) to recover an exact
    /// verbatim span without needing raw iterator access.
    std::string_view remainder() const;

    /// Consumes c if it is the next character after any whitespace. Nothing is
    /// consumed if it is not.
    /// \return true if c was consumed.
    bool skipIfNext( char c );

    /// Tests whether nothing but whitespace is left to read, without consuming anything.
    /// \return true if next() would return nullopt.
    bool atEnd() const;

    /// Scans a quoted value, given the position right after its opening
    /// quote and the quote character itself (both found by the caller
    /// within a field next() already returned, e.g. a config value like
    /// key="a value"). A quoted value can contain the whitespace next()
    /// would otherwise split on, so this may consume further fields
    /// internally to find the closing quote. Either way leaves the scanner
    /// positioned right after wherever the scan stopped, so the caller can
    /// continue parsing after reporting an error.
    /// \return the text between the quotes verbatim, or nullopt if the
    ///         closing quote isn't found, or is found but not followed by
    ///         one of followOptions.
    std::optional<std::string_view> quotedValue(
        std::string_view::const_iterator start,  ///< position right after the opening quote
        char quoteChar,                          ///< the quote character to match ('"' or '\'')
        const std::optional<std::vector<QuoteFollowOption>> &followOptions );
                                                  ///< what may follow the closing quote for it to
                                                  ///< count - e.g. {Whitespace,End} to require the
                                                  ///< closing quote land on a field boundary next()
                                                  ///< would also recognize, or std::nullopt to
                                                  ///< require nothing.

    /// Reads the next field, whichever comes first: a quote-delimited
    /// segment (see isQuoted()/quotedValue()) if it starts with a quote
    /// character, otherwise a plain whitespace-delimited segment (see
    /// next()). Lets a caller read a field that might be quoted without
    /// checking-then-branching itself - e.g. coordsys.def's
    /// ANS "Australian National Spheroid (ANS)" 6378160 298.25, where the
    /// second field is quoted (and contains whitespace) but the others
    /// aren't.
    /// \return the field's value (quotes stripped, if quoted), or nullopt
    ///         at end of input or on a malformed quoted value.
    std::optional<std::string_view> checkAndRecoverQuotedValue(
        bool onlyDoubleQuote,                     ///< see isQuoted()
        const std::optional<std::vector<QuoteFollowOption>> &followOptions );
                                                   ///< see quotedValue()

    /// A thin wrapper over checkAndRecoverQuotedValue() that additionally
    /// parses the recovered field as a double (see parse_double()) - for
    /// callers where a numeric field may legally be quoted (iostring.cpp's
    /// double_from_string() went through the same quote-transparent field
    /// reader as its string fields, so a quoted number was always legal,
    /// however rarely used in practice).
    /// \return the parsed value, or nullopt at end of input, on a malformed
    ///         quoted value, or if the recovered field isn't a valid double.
    std::optional<double> checkAndRecoverQuotedDoubleValue(
        bool onlyDoubleQuote,                     ///< see isQuoted()
        const std::optional<std::vector<QuoteFollowOption>> &followOptions );
                                                   ///< see quotedValue()

    /// Checks whether field begins with a quote character - '"'
    /// unconditionally, and also '\'' unless onlyDoubleQuote is set.
    /// \return the position of the opening quote (field.begin()), or
    ///         field.end() if field is empty or doesn't start with a quote.
    static std::string_view::const_iterator isQuoted(
        std::string_view field,
        bool onlyDoubleQuote );

private:
    /// Exact original text between two positions (both from _pos at some
    /// point in this scanner's history), verbatim - including any interior
    /// whitespace runs (multiple spaces, tabs) untouched. Internal only -
    /// external callers get the same result via two remainder() captures
    /// and .data()/.size() arithmetic, without needing raw iterator access.
    std::string_view _span(
        std::string_view::const_iterator start,
        std::string_view::const_iterator end )
        const;

    std::string_view _text;                    ///< the text being scanned, owned by the caller
    std::string_view::const_iterator _pos;      ///< current cursor position within _text
};

/// Parses field as a double, requiring the whole field to be consumed (no
/// trailing characters) - a std::from_chars-based replacement for
/// iostring.cpp's double_from_string(), usable directly on a FieldScanner
/// field without needing it null-terminated.
/// \return the parsed value, or nullopt if field isn't a valid double or has
///         trailing characters after the number.
std::optional<double> parse_double(
    std::string_view field );  ///< the field to parse

/// A leading T (int, double, etc.) parsed from the start of a field, plus
/// where in field the number ended (result.ptr, as std::from_chars leaves
/// it) - the raw shared primitive behind parse_leading() and callers that
/// need to inspect whatever followed the number themselves (e.g. to
/// recognize a "?" continuation marker, as in control.cpp's reference frame
/// parameter parsing, via field.substr(result.ptr-field.data())). Only
/// instantiated (in fieldscanner.cpp) for the types real callers actually use.
/// \return nullopt if field doesn't start with a valid T.
template <typename T>
struct ParsedField
{
    T value;
    std::from_chars_result result;
};

template <typename T>
std::optional<ParsedField<T>> parse_leading_field(
    std::string_view field );  ///< the field to parse

/// A thin wrapper over parse_leading_field() that discards where the number
/// ended, tolerating trailing text after it (unlike parse_double(), which
/// requires the whole field be consumed) - a std::from_chars-based
/// replacement for the sscanf(field,"%d",&value)!=1/
/// sscanf(field,"%lf",&value)!=1 family of idioms, which themselves tolerate
/// trailing text.
/// \return the parsed value, or nullopt if field doesn't start with a valid T.
template <typename T>
std::optional<T> parse_leading(
    std::string_view field );  ///< the field to parse

/// A thin wrapper over parse_double() that additionally requires the value
/// be strictly positive - a std::from_chars-based replacement for the old
/// sscanf(field,"%lf%c",&value,&c) != 1 || value <= 0.0 idiom.
/// \return the parsed value, or nullopt if field isn't a valid positive
///         double or has trailing characters after the number.
std::optional<double> parse_positive_double(
    std::string_view field );  ///< the field to parse

/// Compares two strings ignoring case, returning -1 if string1 sorts before
/// string2, 0 if they are equal and 1 if string1 sorts after string2. Each
/// character is folded to lower case before comparing, matching _stricmp.
int compare_ignoring_case(
    std::string_view string1,    ///< the first string
    std::string_view string2 );  ///< the second string

/// Compares at most the first maxLength characters of two strings ignoring
/// case, matching _strnicmp. A string shorter than maxLength is compared in
/// full, so it sorts before a longer string that it is a prefix of.
int compare_ignoring_case(
    std::string_view string1,    ///< the first string
    std::string_view string2,    ///< the second string
    size_t maxLength );          ///< the maximum number of characters to compare

/// Tests whether two names are the same, ignoring case and treating spaces,
/// underscores, control characters and characters outside ASCII as
/// interchangeable. So "Day Shift", "day_shift" and "DAY_SHIFT" all match.
/// The names must be the same length to match.
/// \return true if the names match.
bool is_name_match(
    std::string_view string1,    ///< the first name
    std::string_view string2 );  ///< the second name

/// Copies field into buf, truncating without error if it doesn't fit -
/// matches the legacy next_string_field()'s truncate-not-error behavior,
/// for callers not yet converted off fixed-size buffers.
void copy_field(
    std::string_view field,  ///< the field to copy
    char *buf,               ///< destination buffer
    int nbuf );              ///< buf's capacity, including the trailing '\0'

/// The outcome of reading a field with read_string_field() or
/// read_double_field().
enum class FieldResult
{
    Ok,              ///< the field was read
    NoMoreData,      ///< nothing but whitespace is left to read
    MalformedQuote,  ///< the field starts a quoted value that is not closed
    InvalidValue     ///< the field could not be converted to the requested type
};

/// Reads the next field from scanner as a string, treating a quoted value as
/// one field. A field longer than maxlength is cut short without error.
FieldResult read_string_field(
    FieldScanner &scanner,  ///< the scanner to read from
    std::string &value,     ///< set to the field when the result is Ok
    size_t maxlength );     ///< the most characters to keep

/// Reads the next field from scanner as a number, treating a quoted value as
/// one field. The whole field must be the number.
FieldResult read_double_field(
    FieldScanner &scanner,  ///< the scanner to read from
    double &value );        ///< set to the number when the result is Ok

/// Reads all the text left in scanner, without the whitespace before it and
/// without any carriage return or Ctrl-Z characters in it, and leaves the
/// scanner at the end. More than maxlength characters are cut short without
/// error.
FieldResult read_remaining_text(
    FieldScanner &scanner,  ///< the scanner to read from
    std::string &value,     ///< set to the remaining text when the result is Ok
    size_t maxlength );     ///< the most characters to keep

/// Reads the next field from scanner as a whole number, treating a quoted
/// value as one field. The whole field must be the number.
FieldResult read_int_field(
    FieldScanner &scanner,  ///< the scanner to read from
    int &value );           ///< set to the number when the result is Ok

/// Reads the next field from scanner as a whole number, treating a quoted
/// value as one field. The whole field must be the number.
FieldResult read_long_field(
    FieldScanner &scanner,  ///< the scanner to read from
    long &value );          ///< set to the number when the result is Ok

/// Reads the next field from scanner as an angle in degrees, and converts it
/// to radians.
FieldResult read_degree_angle_field(
    FieldScanner &scanner,  ///< the scanner to read from
    double &radians );      ///< set to the angle when the result is Ok

/// Reads the next three fields from scanner as an angle in degrees, minutes
/// and seconds, and converts it to radians. The degrees and minutes are whole
/// numbers.
FieldResult read_dms_angle_field(
    FieldScanner &scanner,  ///< the scanner to read from
    double &radians );      ///< set to the angle when the result is Ok

/// Reads the next field from scanner as an angle in the format commonly used
/// by Hewlett-Packard calculators, ddd.mmssfff, and converts it to radians.
/// The degrees can be omitted, the minutes and seconds are two digits each,
/// and the fraction of a second is any number of digits. The angle cannot be
/// negative.
FieldResult read_hp_angle_field(
    FieldScanner &scanner,  ///< the scanner to read from
    double &radians );      ///< set to the angle when the result is Ok

#endif
