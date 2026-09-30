#ifndef _CLASSIFY_H
#define _CLASSIFY_H

/*
   $Log: classify.h,v $
   Revision 1.1  1995/12/22 17:41:14  CHRIS
   Initial revision

*/

#include <stdint.h>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

// Pinned to int32_t (matching the field's previous plain `int` type)
// rather than left as the compiler's default choice of underlying type,
// which is implementation-defined and could differ in size between
// compilers - this gets written to a .bin file via write_raw<T>, which
// sizes itself off sizeof(T).
enum class ClassValueType : int32_t { Char = 0, Int = 1 };
#define CLASS_VALUE_NOT_DEFINED -1

/// A plain value type. value holds the Char name or the Int id this
/// value represents, matching the owning class_type's own type -
/// set_default_class_value can replace a Char value's name after
/// construction, so it isn't const.
struct class_value
{
    explicit class_value( std::variant<std::string, int> value ) :
        value( std::move(value) ), usage( 0 ), error_factor( 1.0 )
    {}

    std::variant<std::string, int> value; ///< The Char name, or the Int id, this value represents
    unsigned char usage;                    ///< Bitmask of usage flags, set via set_class_flag
    double error_factor;                      ///< Error scaling factor for this value, set via set_class_error_factor
};

/// A plain value type - value being a real vector means there's no
/// separate count/capacity bookkeeping to get wrong, and no destructor
/// or deleted copy constructor needed here either.
struct class_type
{
    class_type( std::string name, ClassValueType type ) :
        name( std::move(name) ), type( type )
    {}

    /// Id of value_name within this classification's values (0 based),
    /// or CLASS_VALUE_NOT_DEFINED if not found and create is 0. If type is
    /// Int, parses value_name as an integer instead, ignoring create.
    int value_id( std::string_view value_name, int create );

    /// Appends new_value as a new value of this classification, returning
    /// a pointer to it (stable until the next call to add_value).
    class_value *add_value( std::variant<std::string, int> new_value );

    /// The value identified by value_id (0 based for Char, the raw
    /// integer for Int), adding it first if create is set and it isn't
    /// already present, or nullptr if not found and create is 0.
    class_value *find_value( int value_id, int create );

    const std::string name;   ///< Name of this classification, e.g. "WEATHER"
    std::vector<class_value> value; ///< The values defined within this classification
    const ClassValueType type;      ///< Whether this classification's values are named (Char) or numeric (Int)
};

/// class_index being a vector of owned pointers (class_type has const
/// members, so it can't live directly in a vector that might reallocate)
/// means no manual new/delete, no separate count/capacity bookkeeping,
/// and no user-declared constructor or destructor needed here either -
/// the implicit ones are already correct. clear() resets to the same
/// empty state a freshly-constructed classifications has - a real,
/// callable reset operation in its own right, not just destructor
/// plumbing.
///
/// Class id is 1 based; class value id is 0 based, with 0 being the
/// default value that's always defined.
struct classifications
{
    void clear() { class_index.clear(); }

    int count() const;

    int id( std::string_view name, int create );
    std::string name( int id ) const;

    /// Id of the classification named name (1 based), adding it first
    /// with the given value type if create is set and it isn't already
    /// present, or 0 if not found and create is 0.
    int find_or_create_id( std::string_view name, ClassValueType type, int create );

    void set_default_value( int class_id, const std::string &dflt );
    int value_id( int class_id, std::string_view value, int create );
    std::string value_name( int class_id, int value_id ) const;
    int value_count( int class_id ) const;

    void set_value_flag( int class_id, int value_id, unsigned char flagbit );
    void set_value_error_factor( int class_id, int value_id, double error_factor );

    unsigned char value_usage( int class_id, int value_id );
    double value_error_factor( int class_id, int value_id );

    void dump( FILE *f ) const;
    int reload( FILE *f );

    std::vector<std::unique_ptr<class_type>> class_index;
};

#endif

