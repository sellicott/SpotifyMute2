#ifndef SDE_DBUS_UTILS_H
#define SDE_DBUS_UTILS_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <systemd/sd-bus.h>

// union value name based on dbus type names
// see: https://dbus.freedesktop.org/doc/dbus-specification.html#basic-types
typedef union
{
    uint8_t y;  // BYTE
    bool b;     // BOOLEAN
    int16_t n;  // INT16
    uint16_t q; // UINT16
    int32_t i;  // INT32
    uint32_t u; // UINT32
    int64_t x;  // INT64
    uint64_t t; // UINT64
    double d;   // DOUBLE
    uint32_t h; // UINX_FD

    // string types
    char *s; // regular string
} dbus_v_t;

typedef struct
{
    // information for string (s) type;
    char *s;

    // information for varient (v) type
    char v_type;
    bool need_free;
    dbus_v_t v;

} dbus_sv_t;

typedef struct
{
    int len;
    dbus_sv_t sv_array[];
} dbus_sv_array_t;

#define FREE_DBUS_STRV( strv_name )                       \
    do                                                    \
    {                                                     \
        for ( int i = 0; strv_name && strv_name[i]; ++i ) \
        {                                                 \
            free( strv_name[i] );                         \
        }                                                 \
        free( strv_name );                                \
    } while ( 0 )

int bus_print_property( const char *name, sd_bus_message *property );
int bus_print_sv_array( sd_bus_message *msg );
int bus_read_sv_array( dbus_sv_array_t **sv, sd_bus_message *msg );

#endif // SDE_DBUS_UTILS_H
