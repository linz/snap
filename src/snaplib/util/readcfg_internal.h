#ifndef _READCFG_INTERNAL_H
#define _READCFG_INTERNAL_H

#include <cstddef>
#include <optional>
#include <string>

/// Implementation helpers behind CFG_FILE::get_config_line() (readcfg.h).
/// Declared in their own header, separate from readcfg.h, so that
/// readcfg.h's public config-file-reading API isn't cluttered with pieces
/// only get_config_line itself needs to see. Deliberately plain, generic
/// text-processing utilities (nothing here reads from a file or knows
/// about CFG_FILE) - real callers should use CFG_FILE::get_config_line(),
/// not these directly.

/// The rule filter_line() applies to recognise a comment - bundled
/// together since both are always sourced from the same CFG_FILE and
/// passed as one unit.
struct CommentRule
{
    std::optional<char> comment_char;  ///< text from this character onward is a comment, and is
                                        ///< discarded; nullopt means comments aren't recognised at all
    bool whole_line_only;               ///< if true, comment_char only starts a comment when it is the
                                        ///< very first character of the line - it's all-or-nothing, the
                                        ///< whole line is a comment or none of it is; otherwise a comment
                                        ///< may start anywhere, trailing real content on the same line
};

/// Filters line in place into its real content: strips '\r' and '\x1A' (the
/// DOS CR and Ctrl-Z/EOF marker) everywhere, discards everything from the
/// comment character onward (per comment), and normalises every whitespace
/// character that survives to a single ' '. The whole line is always
/// scanned to its true end, regardless of any later length limit - the
/// comment character may occur anywhere (unless comment.whole_line_only),
/// and a late comment discards everything from that point on, so a length
/// limit can't be applied until after this filtering is done.
void filter_line( std::string &line, const CommentRule &comment );

/// Truncates line in place to at most max_len characters, returning the
/// count of significant characters discarded past that point - a run of
/// pure whitespace immediately at the max_len boundary is tolerated
/// (neither kept nor counted, so it contributes 0 to the returned count),
/// matching get_config_line's original "don't flag harmless trailing
/// padding" behaviour, but once a real (non-whitespace) character appears
/// past max_len, it and everything after it - including further whitespace -
/// counts toward the returned total.
int cap_line( std::string &line, std::size_t max_len );

#endif  /* _READCFG_INTERNAL_H defined */
