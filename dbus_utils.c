#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <systemd/sd-bus.h>

#include "dbus_utils.h"

// function prototypes
int bus_free_sv( dbus_sv_t *sv );
int bus_read_s_array( char **str_ptr, sd_bus_message *msg );
int bus_read_v( dbus_v_t *v, char *type, bool *need_free, sd_bus_message *msg );
int bus_read_sv( dbus_sv_t *sv, sd_bus_message *msg );

int bus_read_s_array( char **str_ptr, sd_bus_message *msg )
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
        // read string
        const char *tmp_str = NULL;
        ret = sd_bus_message_read_basic( msg, 's', (void *)&tmp_str );
        if ( ret < 0 )
        {
            fprintf( stderr,
                     "Error reading varient value: %s\n",
                     strerror( -ret ) );
            goto exit_container;
        }
        // if we got a valid string
        else if ( tmp_str )
        {
            // two cases, empty string, and concatenation
            if ( !( *str_ptr ) )
            {
                *str_ptr = malloc( strlen( tmp_str ) + 1 );
                strcpy( *str_ptr, tmp_str );
            }
            else
            {
                // copy over the string, reallocating enough space for both plus
                // the null character and a comma
                *str_ptr = realloc( *str_ptr,
                                    strlen( *str_ptr ) + strlen( ", " ) +
                                        strlen( tmp_str ) + 1 );
                strcat( *str_ptr, ", " );
                strcat( *str_ptr, tmp_str );
            }
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
        char *tmp_str = NULL;
        ret = bus_read_s_array( &tmp_str, msg );
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
        v->s = tmp_str;
        *type = 's';
        *need_free = true;
        ret = EXIT_SUCCESS;
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
                fprintf( stderr, "varient type: %s\n", contents_type );
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
        bus_free_sv_array( &sv );
        sv = NULL;
    }
    *asv_ptr = sv;

exit_container:
    // exit varient
    ret = sd_bus_message_exit_container( msg );

no_cleanup:
    return ret;
}

/*
 * Free a sv (string, value) dictionary
 *
 * Returns: 0 (`EXIT_SUCCESS`) if entry were freed, a negative value
 * (`EXIT_FAILURE`) if invalid.
 */
int bus_free_sv( dbus_sv_t *sv )
{
    int ret = EXIT_SUCCESS;
    if ( !sv )
    {
        ret = -EXIT_FAILURE;
        goto no_cleanup;
    }

    // always cleanup the entry name string
    free( sv->s );
    sv->s = NULL;

    // possibly cleanup the value
    if ( sv->need_free )
    {
        free( sv->v.s );
        sv->v.s = NULL;
    }

no_cleanup:
    return ret;
}


/*
 * Free a sv (string, value) dictionary array
 *
 * Returns: 0 (`EXIT_SUCCESS`) if array was freed, a negative value
 * (`EXIT_FAILURE`) if invalid.
 */
int bus_free_sv_array( dbus_sv_array_t **sv_array_ptr )
{
    int ret = EXIT_SUCCESS;

    if ( !sv_array_ptr )
    {
        ret = -EXIT_FAILURE;
        goto no_cleanup;
    }

    // check for a valid sv array
    dbus_sv_array_t *sv_array = *sv_array_ptr;
    if ( !sv_array )
    {
        // array has already been freed
        ret = EXIT_SUCCESS;
        goto no_cleanup;
    }

    // loop through the array members, recursively freeing them
    for ( int i = 0; i < sv_array->len; ++i )
    {
        dbus_sv_t *sv = &sv_array->sv_array[i];

        // free the inner dict contents
        bus_free_sv( sv );
    }

    // free the containing structure
    free( sv_array );

    // null the container
    *sv_array_ptr = NULL;

no_cleanup:
    return ret;
}

/*
 * Prints the contents of a `dbus_sv_array_t` struct.
 *
 * Takes a const pointer to an sv_array struct (array of string,value
 * dictionaries). The values in the pointer are not modified.
 *
 * Returns: 0 (`EXIT_SUCCESS`) if values were printed, a negative value
 * (`EXIT_FAILURE`) if invalid.
 */
int bus_print_sv_array( const dbus_sv_array_t *sv_array )
{
    int ret = EXIT_SUCCESS;
    // check for a valid sv array
    if ( !sv_array )
    {
        ret = -EXIT_FAILURE;
        goto no_cleanup;
    }

    for ( int i = 0; i < sv_array->len; ++i )
    {
        const dbus_sv_t *sv = &sv_array->sv_array[i];
        if ( sv )
        {
            if ( sv->v_type == 's' )
            {
                printf( "%20s: %s\n", sv->s, sv->v.s );
            }
            else if ( sv->v_type == 'd' )
            {
                printf( "%20s: %lf\n", sv->s, sv->v.d );
            }
            else if ( sv->v_type == 'i' )
            {
                printf( "%20s: %d\n", sv->s, sv->v.i );
            }
            else
            {
                printf( "%20s: null\n", sv->s );
            }
        }
    }

no_cleanup:
    return ret;
}
