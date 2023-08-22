#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <systemd/sd-bus.h>

#include "dbus_utils.h"

int bus_print_property( const char *name, sd_bus_message *property )
{
    char type;
    const char *contents;
    int r;


    r = sd_bus_message_peek_type( property, &type, &contents );
    if ( r < 0 )
        return r;

    switch ( type )
    {

        case SD_BUS_TYPE_STRING:
        {
            const char *s;

            r = sd_bus_message_read_basic( property, type, &s );
            if ( r < 0 )
                return r;

            printf( "%s=%s\n", name, s );

            return 1;
        }

        case SD_BUS_TYPE_BOOLEAN:
        {
            bool b;

            r = sd_bus_message_read_basic( property, type, &b );
            if ( r < 0 )
                return r;

            printf( "%s\n", name );

            return 1;
        }

        case SD_BUS_TYPE_INT64:
        {
            int64_t i64;

            r = sd_bus_message_read_basic( property, type, &i64 );
            if ( r < 0 )
                return r;

            printf( "%s=%i\n", name, (int)i64 );
            return 1;
        }

        case SD_BUS_TYPE_INT32:
        {
            int32_t i;

            r = sd_bus_message_read_basic( property, type, &i );
            if ( r < 0 )
                return r;

            printf( "%s=%i\n", name, (int)i );
            return 1;
        }

        case SD_BUS_TYPE_OBJECT_PATH:
        {
            const char *p;

            r = sd_bus_message_read_basic( property, type, &p );
            if ( r < 0 )
                return r;

            printf( "%s=%s\n", name, p );

            return 1;
        }

        case SD_BUS_TYPE_DOUBLE:
        {
            double d;

            r = sd_bus_message_read_basic( property, type, &d );
            if ( r < 0 )
                return r;

            printf( "%s=%g\n", name, d );
            return 1;
        }

        case SD_BUS_TYPE_ARRAY:
            if ( strcmp( contents, "s" ) == 0 )
            {
                bool first = true;
                const char *str;

                r = sd_bus_message_enter_container( property,
                                                    SD_BUS_TYPE_ARRAY,
                                                    contents );
                if ( r < 0 )
                    return r;

                while ( ( r = sd_bus_message_read_basic( property,
                                                         SD_BUS_TYPE_STRING,
                                                         &str ) ) > 0 )
                {
                    if ( first )
                        printf( "%s=", name );

                    printf( "%s%s", first ? "" : " ", str );

                    first = false;
                }
                if ( r < 0 )
                    return r;

                if ( first )
                    printf( "%s=", name );
                if ( !first )
                    puts( "" );

                r = sd_bus_message_exit_container( property );
                if ( r < 0 )
                    return r;

                return 1;
            }
            else
            {
                printf( "array unreadable" );
                return 0;
            }

            break;

        default:
            fprintf( stderr, "Error, default case" );
    }

    return 0;
}

int bus_print_sv_array( sd_bus_message *msg )
{
    sd_bus_error err = SD_BUS_ERROR_NULL;
    int ret = 0;

    ret = sd_bus_message_enter_container( msg, SD_BUS_TYPE_ARRAY, "{sv}" );
    if ( ret < 0 )
        goto cleanup;

    while ( ( ret = sd_bus_message_enter_container( msg,
                                                    SD_BUS_TYPE_DICT_ENTRY,
                                                    "sv" ) ) > 0 )
    {
        const char *name;
        const char *contents;

        ret = sd_bus_message_read_basic( msg, SD_BUS_TYPE_STRING, &name );
        if ( ret < 0 )
            goto cleanup;


        ret = sd_bus_message_peek_type( msg, NULL, &contents );
        if ( ret < 0 )
            goto cleanup;

        ret = sd_bus_message_enter_container( msg,
                                              SD_BUS_TYPE_VARIANT,
                                              contents );
        if ( ret < 0 )
            goto cleanup;

        ret = bus_print_property( name, msg );
        if ( ret < 0 )
            goto cleanup;

        if ( ret == 0 )
        {
            printf( "%s=[unprintable]\n", name );
            /* skip what we didn't read */
            ret = sd_bus_message_skip( msg, contents );
            if ( ret < 0 )
                goto cleanup;
        }

        ret = sd_bus_message_exit_container( msg );
        if ( ret < 0 )
            goto cleanup;


        ret = sd_bus_message_exit_container( msg );
        if ( ret < 0 )
            goto cleanup;
    }
    if ( ret < 0 )
        goto cleanup;

    ret = sd_bus_message_exit_container( msg );
    if ( ret < 0 )
        goto cleanup;

cleanup:
    if ( err._need_free != 0 )
    {
        printf( "%d \n", ret );
        printf( "returned error: %s\n", err.message );
    }
    sd_bus_error_free( &err );

    return ret < 0 ? -EXIT_FAILURE : EXIT_SUCCESS;
}

int bus_read_s_array( char *str, sd_bus_message *msg )
{
    int ret = 0;

    // enter string array
    ret = sd_bus_message_enter_container( msg, SD_BUS_TYPE_ARRAY, "s" );
    if ( ret < 0 )
    {
        fprintf( stderr,
                 "Error entering string array: %s\n",
                 strerror( -ret ) );
        goto no_cleanup;
    }

    // read data until we run out
    while ( true )
    {
        ret = sd_bus_message_read_basic( msg, 's', (void *)str );
        if ( ret < 0 )
        {
            fprintf( stderr,
                     "Error reading varient value: %s\n",
                     strerror( -ret ) );
            goto exit_container;
        }
        // out of data
        else if ( ret == 0 )
        {
            break;
        }
    }

exit_container:
    ret = sd_bus_message_exit_container( msg );

no_cleanup:
    return ret;
}
int bus_read_v( dbus_v_t *v, char *type, bool *need_free, sd_bus_message *msg )
{
    int ret = 0;
    char t;
    // we don't own this string, we don't need to free it.
    const char *contents_type = NULL;

    // we expect to see a varient type, see what the actual type is
    ret = sd_bus_message_peek_type( msg, &t, &contents_type );
    if ( ret < 0 )
    {
        fprintf( stderr, "Error reading message: %s\n", strerror( -ret ) );
        goto no_cleanup;
    }
    else if ( ret == 0 )
    {
        fprintf( stderr, "Error: message empty" );
        ret = -EXIT_FAILURE;
        goto no_cleanup;
    }
    else if ( t != 'v' )
    {
        fprintf( stderr,
                 "Error: message not varient\n"
                 "\tExpected 'v', received %c\n",
                 t );
        ret = -EXIT_FAILURE;
        goto no_cleanup;
    }

    // enter varient
    ret = sd_bus_message_enter_container( msg,
                                          SD_BUS_TYPE_VARIANT,
                                          contents_type );
    if ( ret < 0 )
    {
        fprintf( stderr,
                 "Error: message not varient\n"
                 "\tExpected 'v', received %s\n",
                 contents_type );
        goto no_cleanup;
    }

    // check if we have a string array
    // if we do, concatenate into a single list
    if ( strcmp( contents_type, "as" ) == 0 )
    {
        ret = bus_read_s_array( v->s, msg );
        if ( ret < 0 )
        {
            fprintf( stderr,
                     "Error reading varient value: %s\n",
                     strerror( -ret ) );
            if ( ret == -EINVAL )
            {
                printf( "varient type: %s\n", contents_type );
            }
            goto exit_container;
        }
    }
    // read single varient type
    else
    {
        ret = sd_bus_message_read_basic( msg, *contents_type, (void *)v );
        if ( ret < 0 )
        {
            fprintf( stderr,
                     "Error reading varient value: %s\n",
                     strerror( -ret ) );
            if ( ret == -EINVAL )
            {
                printf( "varient type: %s\n", contents_type );
            }
            goto exit_container;
        }
        else if ( ret == 0 )
        {
            fprintf( stderr, "Error: message empty" );
            ret = -EXIT_FAILURE;
            goto exit_container;
        }

        *type = *contents_type;
        *need_free = false;

        char *tmp = NULL;
        switch ( *contents_type )
        {
            // string types, collapse into string type
            case 's':
            case 'o':
            case 'g':
                tmp = malloc( strlen( v->s ) + 1 );
                tmp = strcpy( tmp, v->s );
                v->s = tmp;

                *type = 's';
                *need_free = true;

                ret = EXIT_SUCCESS;
                break;

            // base types, don't need to do anything
            case 'y':
            case 'b':
            case 'n':
            case 'q':
            case 'i':
            case 'u':
            case 'h':
            case 'x':
            case 't':
            case 'd':
                ret = EXIT_SUCCESS;
                break;

            // default, unexpected type
            default:
                ret = -ENXIO;
                goto no_cleanup;
        }
    }

exit_container:
    // exit varient
    ret = sd_bus_message_exit_container( msg );

no_cleanup:
    return ret;
}

/* bus_read_sv
 * read dbus dictionary entry ({sv}) into a structure.
 */
int bus_read_sv( dbus_sv_t *sv, sd_bus_message *msg )
{
    int ret = 0;

    // open the dictionary
    ret = sd_bus_message_enter_container( msg, SD_BUS_TYPE_DICT_ENTRY, "sv" );
    if ( ret < 0 )
    {
        // catch bad formatting errors and set our own message
        if ( ret == -ENXIO )
        {
            fprintf( stderr,
                     "Error: message not dict.\n"
                     "\tExpected '{sv}'\n" );
            ret = -EXIT_FAILURE;
        }
        goto no_cleanup;
    }
    // no data avalible
    else if ( ret == 0 )
    {
        goto no_cleanup;
    }

    // read string
    const char *tmp_str;
    ret = sd_bus_message_read_basic( msg, 's', (void *)&tmp_str );
    if ( ret < 0 )
    {
        fprintf( stderr, "Error reading dict key: %s\n", strerror( -ret ) );
        goto exit_container;
    }

    // copy over the string
    sv->s = malloc( strlen( tmp_str ) + 1 );
    strcpy( sv->s, tmp_str );

    // read the varient
    ret = bus_read_v( &sv->v, &sv->v_type, &sv->need_free, msg );
    if ( ret < 0 )
    {
        fprintf( stderr, "Error reading dict value: %s\n", strerror( -ret ) );
        goto exit_container;
    }

exit_container:
    // exit varient
    ret = sd_bus_message_exit_container( msg );

no_cleanup:
    return ret;
}

/* bus_read_sv_array
 * read dbus dictionary array entry (a{sv}) into a structure.
 */
int bus_read_sv_array( dbus_sv_array_t **asv_ptr, sd_bus_message *msg )
{
    int ret = 0;
    dbus_sv_array_t *sv = NULL;

    // open the dictionary
    ret = sd_bus_message_enter_container( msg, SD_BUS_TYPE_ARRAY, "{sv}" );
    if ( ret < 0 )
    {
        // catch bad formatting errors and set our own message
        if ( ret == -ENXIO )
        {
            fprintf( stderr, "Error: message not dict array.\n" );
            ret = -EXIT_FAILURE;
        }
        goto no_cleanup;
    }

    // do initial malloc for sv
    sv = malloc( sizeof( dbus_sv_array_t ) );
    sv->len = 0;

    bool data_remaining = true;
    while ( data_remaining )
    {
        // put the new dict entry on the stack
        dbus_sv_t new_sv = { 0 };

        // read the varient
        ret = bus_read_sv( &new_sv, msg );
        if ( ret < 0 )
        {
            fprintf( stderr, "Error reading dict: %s\n", strerror( -ret ) );
            goto memory_cleanup;
        }
        // no more data, exit
        else if ( ret == 0 )
        {
            break;
        }

        // grow the list
        size_t new_size =
            sizeof( dbus_sv_array_t ) + sizeof( dbus_sv_t[sv->len + 1] );
        sv = realloc( sv, new_size );

        // shallow copy the data into the flexable array member
        // also upddate the length at the same time
        sv->sv_array[sv->len++] = new_sv;
    }


memory_cleanup:
    // if we had an error, recursively free the structure
    if ( sv && ret < 0 )
    {
        // todo recursively free structure
        free( sv );
    }
    else
    {
        *asv_ptr = sv;
    }

exit_container:
    // exit varient
    ret = sd_bus_message_exit_container( msg );

no_cleanup:
    return ret;
}
