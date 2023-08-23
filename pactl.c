/* pactl.c - pulseaudio interaction code.
   Written in 2014 by Mehmet Kayaalp mkayaalp@cs.binghamton.edu

To the extent possible under law, the author(s) have dedicated all copyright
and related and neighboring rights to this software to the public domain
worldwide. This software is distributed without any warranty.
You should have received a copy of the CC0 Public Domain Dedication along with
this software. If not, see
<http://creativecommons.org/publicdomain/zero/1.0/>.*/

#include "pactl.h"
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <pulse/context.h>
#include <pulse/error.h>
#include <pulse/introspect.h>
#include <pulse/mainloop-api.h>
#include <pulse/mainloop-signal.h>
#include <pulse/mainloop.h>
#include <pulse/xmalloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

pa_proplist *proplist = NULL;
pa_context *context = NULL;
char context_ready;
pa_mainloop_api *mainloop_api = NULL;
int sink_input_idx;
int retry_update;
int pending_update;
int pending_mute;

// define nanosleep
extern int nanosleep( const struct timespec *req, struct timespec *rem );

void context_drain_complete( pa_context *c, void *userdata )
{
    (void)( userdata );
    pa_context_disconnect( c );
}

void drain( void )
{
    if ( !context )
        return;
    pa_operation *o;
    if ( !( o = pa_context_drain( context, context_drain_complete, NULL ) ) )
        pa_context_disconnect( context );
    else
        pa_operation_unref( o );
}

void mute_callback( pa_context *c, int success, void *userdata )
{
    (void)( userdata );
    if ( !success )
    {
        fprintf( stderr,
                 "Failure: %s\n",
                 pa_strerror( pa_context_errno( c ) ) );
        if ( retry_update > 0 )
        {
            pending_update = 1;
            update_sink();
        }
    }
}

void get_sink_input_info_callback( pa_context *c,
                                   const pa_sink_input_info *i,
                                   int is_last,
                                   void *userdata )
{
    (void)( userdata );
    if ( is_last < 0 )
    {
        fprintf( stderr,
                 "Failed to get sink input information: %s\n",
                 pa_strerror( pa_context_errno( c ) ) );
        exit( 1 );
        return;
    }
    if ( is_last )
    {
        retry_update--;
        return;
    }
    assert( i );

    if ( !strcmp( pa_proplist_gets( i->proplist, PA_PROP_MEDIA_NAME ),
                  "Spotify" ) )
    {
        fprintf( stderr,
                 "get_sink_input_info_callback(): Spotify is %u\n",
                 i->index );
        sink_input_idx = i->index;
        if ( pending_update )
        {
            pending_update = 0;
            pa_operation_unref( pa_context_set_sink_input_mute( context,
                                                                sink_input_idx,
                                                                pending_mute,
                                                                mute_callback,
                                                                NULL ) );
        }
    }
}

void context_state_callback( pa_context *c, void *userdata )
{
    (void)( userdata );
    assert( c );
    if ( pa_context_get_state( c ) == PA_CONTEXT_READY )
    {
        context_ready = 1;
    }
}


void *pactl( void *arg )
{
    (void)( arg );
    int ret;
    pa_mainloop *m = NULL;
    char *server = NULL;

    proplist = pa_proplist_new();
    if ( !( m = pa_mainloop_new() ) )
    {
        fprintf( stderr, "pa_mainloop_new() failed.\n" );
    }
    mainloop_api = pa_mainloop_get_api( m );

    if ( !( context =
                pa_context_new_with_proplist( mainloop_api, NULL, proplist ) ) )
    {
        fprintf( stderr, "pa_context_new() failed.\n" );
    }

    pa_context_set_state_callback( context, context_state_callback, NULL );
    if ( pa_context_connect( context, server, 0, NULL ) < 0 )
    {
        fprintf( stderr,
                 "pa_context_connect() failed: %s\n",
                 pa_strerror( pa_context_errno( context ) ) );
    }

    if ( pa_mainloop_run( m, &ret ) < 0 )
    {
        fprintf( stderr, "pa_mainloop_run() failed.\n" );
    }
    return NULL;
}

void update_sink( void )
{
    if ( context_ready )
    {
        pa_operation_unref(
            pa_context_get_sink_input_info_list( context,
                                                 get_sink_input_info_callback,
                                                 NULL ) );
    }
    else
        fprintf( stderr, "context is not ready\n" );
}

void set_mute( int mute )
{
    if ( context_ready )
    {
        retry_update = 1;
        pending_mute = mute;
        pa_operation_unref( pa_context_set_sink_input_mute( context,
                                                            sink_input_idx,
                                                            mute,
                                                            mute_callback,
                                                            NULL ) );
    }
    else
        fprintf( stderr, "context is not ready\n" );
}

void init_pactl( void )
{
    int s;
    pthread_t thread;
    s = pthread_create( &thread, NULL, pactl, NULL );
    if ( s != 0 )
    {
        perror( "pthread create error" );
        exit( EXIT_FAILURE );
    }
}

void wait_for_context( void )
{
    int msec = 100;
    while ( !context_ready )
    {
        struct timespec ts;
        int res;

        ts.tv_sec = msec / 1000;
        ts.tv_nsec = ( msec % 1000 ) * 1000000;

        do
        {
            res = nanosleep( &ts, &ts );
        } while ( res && errno == EINTR );
    }
}
