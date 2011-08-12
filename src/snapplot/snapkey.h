#ifndef SNAPKEY_HPP
#define SNAPKEY_HPP

#ifdef __cplusplus
//extern "C" {
#endif

#define SNAP_KEY_HASCOLOUR 0x01
#define SNAP_KEY_HASSTATE  0x02

    void *create_snap_key();
    void delete_snap_key( void *key );
    int  snap_key_count( void *key );

    int  snap_key_add( void *key, char *name, int type );
    int  snap_key_find_name( void *key, char *name );
    char *snap_key_name( void *key, int i );
    int  snap_key_type( void *key, int i );

    long snap_key_colour( void *key, int i );
    void snap_key_set_colour( void *key, int i, long colour );

    int  snap_key_state( void *key, int i );
    void snap_key_set_state( void *key, int i );

#ifdef __cplusplus
//}
#endif

#endif
