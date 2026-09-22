// #pragma once
#ifndef _FIELDSCANNER_HPP
#define _FIELDSCANNER_HPP

#include <optional>
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

    /// Current cursor position - a checkpoint to pass back into span().
    std::string_view::const_iterator pos() const { return _pos; }

    /// Unconsumed text from the current position to the end, verbatim - for
    /// handing the rest of the line off to another parser.
    std::string_view remainder() const;

    /// Exact original text between two positions (both from pos()), verbatim
    /// - including any interior whitespace runs (multiple spaces, tabs)
    /// untouched. Used to capture several consecutive fields as one span
    /// without re-joining tokens with an inserted separator, which would
    /// collapse any original multi-space run between them.
    std::string_view span(
        std::string_view::const_iterator start,  ///< a position from pos(), at or before end
        std::string_view::const_iterator end )   ///< a position from pos(), at or after start
        const;

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

/// A thin wrapper over parse_double() that additionally requires the value
/// be strictly positive - a std::from_chars-based replacement for the old
/// sscanf(field,"%lf%c",&value,&c) != 1 || value <= 0.0 idiom.
/// \return the parsed value, or nullopt if field isn't a valid positive
///         double or has trailing characters after the number.
std::optional<double> parse_positive_double(
    std::string_view field );  ///< the field to parse

#endif
