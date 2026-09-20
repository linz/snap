// #pragma once
#ifndef _FIELDSCANNER_HPP
#define _FIELDSCANNER_HPP

#include <optional>
#include <string_view>

/* Non-owning, non-destructive cursor over a whitespace-delimited field
   sequence. Replaces next_field()/dstring.cpp's destructive char*-based
   tokenization, which writes '\0' into the source buffer, with a
   std::string_view-based scan that never modifies its input.

   Kept in the global namespace (not LINZ::), matching next_field's own
   declaration site in dstring.h, since its real callers are C-flavoured
   legacy files that don't otherwise use the LINZ namespace. */
class FieldScanner
{
public:
    /* text must outlive the FieldScanner - it is never copied, only viewed. */
    explicit FieldScanner( std::string_view text ) : _text(text), _pos(_text.begin()) {}

    /* Returns the next whitespace-delimited field, or nullopt at end. Runs
       of consecutive whitespace are skipped as a single delimiter, matching
       next_field's behavior. */
    std::optional<std::string_view> next();

    /* Current cursor position - a checkpoint to pass back into span(). */
    std::string_view::const_iterator pos() const { return _pos; }

    /* Unconsumed text from the current position to the end, verbatim - for
       handing the rest of the line off to another parser. */
    std::string_view remainder() const;

    /* Exact original text between two positions (both from pos()), verbatim
       - including any interior whitespace runs (multiple spaces, tabs)
       untouched. Used to capture several consecutive fields as one span
       without re-joining tokens with an inserted separator, which would
       collapse any original multi-space run between them. */
    std::string_view span( std::string_view::const_iterator start, std::string_view::const_iterator end ) const;

private:
    std::string_view _text;
    std::string_view::const_iterator _pos;
};

/* Parses field as a positive double, requiring the whole field to be
   consumed (no trailing characters) - a std::from_chars-based replacement
   for the old sscanf(field,"%lf%c",&value,&c) != 1 idiom, usable directly on
   a FieldScanner field without needing it null-terminated. */
bool parse_positive_double( std::string_view field, double &value );

#endif
