#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// The implementation to read what spotify is playing should be refactored out
// later On Linux we need to use DBus to access what music player is running and
// how we should talk to it For this we use the systemd dbus API (sd-bus)
#include <systemd/sd-bus.h>

#include "dbus_utils.h"

// include pulse audio so we can mute spotify
#include "pactl.h"

// We need to implement functions to read about Spotify on dbus using the
// org.mpris.MediaPlayer2 Interface.
// Documentation about the interface is availible here:
// https://specifications.freedesktop.org/mpris-spec/latest/
// I also like qdbusviewer to look at what is going on
// The best tutorial I was able to find was here:
// https://0pointer.net/blog/the-new-sd-bus-api-of-systemd.html

// based on the documentation the application will display itself with it's name
// const char *spotify_dbus_name =
// "org.mpris.MediaPlayer2.firefox.instance161006";
const char *spotify_dbus_name = "org.mpris.MediaPlayer2.spotify";
const char *spotify_dbus_path = "/org/mpris/MediaPlayer2";
const char *spotify_dbus_interface = "org.mpris.MediaPlayer2.Player";

/* Check if Spotify is availible on dbus
 * if it is avalible then return a positive value (indicating the number of
 * avalible interfaces).
 *
 * Accepts a string array pointer as an output parameter. Set to NULL if the
 * names of the spotify interfaces are unneeded.
 * Returns heap allocated memory which needs to be cleaned by the user with
 * free(). The memory is allocated contigously, so only single free is required.
 *
 * Returns: 0 for no spotify interfaces, a positive number (probably 1) for the
 * number of spotify instances avalible and -1 for a connection error
 */
int is_spotify_availible( sd_bus *bus_ptr, char ***instance_names );

int spotify_get_metadata( sd_bus *bus_ptr, dbus_sv_array_t **metadata );

int spotify_send_command( sd_bus *bus_ptr,
                          const char ***instance_names,
                          const char *command );

int is_spotify_availible( sd_bus *bus_ptr, char ***instance_names )
{
    char **bus_names = NULL;
    char **spotify_instances = NULL;
    int num_instances = 0;

    int ret = 0;

    // call dbus to list the objects availibe
    ret = sd_bus_list_names( bus_ptr, &bus_names, NULL );
    if ( ret < 0 )
    {
        fprintf( stderr,
                 "Error getting user bus names: %s\n",
                 strerror( -ret ) );
        goto cleanup;
    }

    // loop through and list all names
    for ( int i = 0; bus_names[i]; ++i )
    {
        const char *media_player_substr = "org.mpris.MediaPlayer2";
        if ( strncmp( media_player_substr,
                      bus_names[i],
                      strlen( media_player_substr ) ) == 0 )
        {
            printf( "media instance: %s\n", bus_names[i] );
        }

        // check if spotify is on the bus
        if ( strcmp( spotify_dbus_name, bus_names[i] ) == 0 )
        {
            // this is techinically unsafe
            spotify_instances =
                realloc( spotify_instances,
                         ( num_instances + 1 ) * sizeof( char * ) );

            // save the memory for the bus names we need
            spotify_instances[num_instances] = bus_names[i];
            num_instances++;
        }
        else
        {
            // free memory for the unneeded bus names
            free( bus_names[i] );
        }
    }
    // put on a null terminator on the end of the list
    spotify_instances =
        realloc( spotify_instances, ( num_instances + 1 ) * sizeof( char * ) );
    spotify_instances[num_instances] = NULL;

    // if the users asked us for the interface names, copy over the buffer,
    // othersize cleanup
    if ( instance_names != NULL )
    {
        *instance_names = spotify_instances;
    }
    else
    {
        FREE_DBUS_STRV( spotify_instances );
    }

cleanup:
    free( bus_names );

    return ret < 0 ? -EXIT_FAILURE : num_instances;
}


int spotify_get_metadata( sd_bus *bus_ptr, dbus_sv_array_t **metadata_out )
{
    dbus_sv_array_t *metadata = NULL;
    sd_bus_message *msg = NULL;
    sd_bus_error err = SD_BUS_ERROR_NULL;
    int ret = 0;

    // get the metadata property from spotify on dbus
    ret = sd_bus_get_property( bus_ptr,
                               spotify_dbus_name,
                               spotify_dbus_path,
                               spotify_dbus_interface,
                               "Metadata",
                               &err,
                               &msg,
                               "a{sv}" );
    if ( ret < 0 )
    {
        fprintf( stderr, "Error getting Spotify metadata: %s\n", err.message );
        goto cleanup;
    }

    ret = bus_read_sv_array( &metadata, msg );
    if ( ret < 0 )
    {
        fprintf( stderr,
                 "Error reading Spotify metadata: %s\n",
                 strerror( -ret ) );
        goto cleanup;
    }

    *metadata_out = metadata;


cleanup:
    sd_bus_error_free( &err );
    sd_bus_message_unref( msg );

    // todo recursively free memory
    if ( ret < 0 )
    {
        free( metadata );
    }

    return ret < 0 ? -EXIT_FAILURE : EXIT_SUCCESS;
}

int main( int argc, char **argv )
{
    (void)( argc );
    (void)( argv );
    // TODO use getopt to parse command line arguments
    // we need to start by connecting to the system message bus and looking
    // for spotify
    char **instance_names = NULL;
    dbus_sv_array_t *metadata = NULL;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus *bus_ptr;
    int ret;

    init_pactl();
    wait_for_context();

    // while ( 1 )
    // {
    ret = sd_bus_default_user( &bus_ptr );
    if ( ret < 0 )
    {
        fprintf( stderr, "Could not open user bus: %s\n", strerror( -ret ) );
        goto cleanup;
    }

    ret = is_spotify_availible( bus_ptr, &instance_names );
    if ( ret < 0 )
    {
        fprintf( stderr, "Spotify is not running: %s\n", strerror( -ret ) );
        goto cleanup;
    }

    puts( "Spotify Instances" );
    char **str = instance_names;
    while ( *str )
    {
        puts( *str++ );
    }

    // spotify is availible, check if the current song is an ad
    // if it is mute spotify
    ret = spotify_get_metadata( bus_ptr, &metadata );
    if ( ret < 0 )
    {
        fprintf( stderr,
                 "Could not get metadata from Spotify: %s\n",
                 strerror( -ret ) );
        goto cleanup_instances;
    }

    printf( "%20s \n", "Metadata:" );
    bus_print_sv_array( metadata );
    puts( "" );

    // loop through the metadata and try and find an ad
    for ( int i = 0; i < metadata->len; ++i )
    {
        const char *track_name_id = "mpris:trackid";
        const char *ad_prefix = "spotify:ad:";

        const char *param = metadata->sv_array[i].s;
        const char *track_name = metadata->sv_array[i].v.s;

        if ( strncmp( track_name_id, param, strlen( track_name_id ) ) == 0 )
        {
            printf( "current track: %s\n", track_name );
            sd_bus_message *msg = NULL;
            sd_bus_error err = SD_BUS_ERROR_NULL;
            if ( strncmp( ad_prefix, track_name, strlen( ad_prefix ) ) == 0 ||
                 strstr( track_name, "/ad/" ) )
            {
                // mute spotify by setting it's output volume to 0
                printf( "Ad found, muting\n" );
                set_mute( 1 );
            }
            // otherwise unmute spotify
            else
            {
                printf( "No ad found, unmuting\n" );
                set_mute( 0 );
            }
            if ( ret < 0 )
            {
                fprintf( stderr,
                         "Error setting Spotify volume: %s\n",
                         err.message );
            }

            sd_bus_error_free( &err );
            sd_bus_message_unref( msg );
        }
    }
    // sleep( 2 );
    // printf( "\x0C" );
    //}

cleanup_instances:
    FREE_DBUS_STRV( instance_names );
    bus_free_sv_array( &metadata );

cleanup:
    sd_bus_error_free( &error );
    sd_bus_unref( bus_ptr );

    drain();

    return ret < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
