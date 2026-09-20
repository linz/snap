// #pragma once
#ifndef _FIELDSCANNER_HPP
#define _FIELDSCANNER_HPP

#include <optional>
#include <string_view>

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
    /// internally to find the closing quote. The closing quote must be
    /// immediately followed by whitespace or end of input to count - i.e.
    /// it must land on a field boundary next() would also recognize - or
    /// this fails. Either way leaves the scanner positioned right after
    /// wherever the scan stopped, so the caller can continue parsing after
    /// reporting an error.
    /// \return the text between the quotes verbatim, or nullopt on failure.
    std::optional<std::string_view> quotedValue(
        std::string_view::const_iterator start,  ///< position right after the opening quote
        char quoteChar );                        ///< the quote character to match ('"' or '\'')

private:
    std::string_view _text;                    ///< the text being scanned, owned by the caller
    std::string_view::const_iterator _pos;      ///< current cursor position within _text
};

/// Parses field as a positive double, requiring the whole field to be
/// consumed (no trailing characters) - a std::from_chars-based replacement
/// for the old sscanf(field,"%lf%c",&value,&c) != 1 idiom, usable directly on
/// a FieldScanner field without needing it null-terminated.
/// \return true if field was consumed in full as a positive double, with
///         value set; false otherwise, with value left unchanged.
bool parse_positive_double(
    std::string_view field,  ///< the field to parse
    double &value );         ///< set to the parsed value on success

#endif
