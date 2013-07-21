/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "apr.h"
#include "apr_lib.h"
#include "apr_strings.h"

#define APR_WANT_STRFUNC
#include "apr_want.h"

#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_request.h"

#include <math.h>

// Socket related libraries
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>

/* ********************************************

    Structs & Defines

   ******************************************** */

#ifdef DEBUG                // To print diagnostics to the error log
#define _DEBUG 1            // enable through gcc -DDEBUG
#else
#define _DEBUG 0
#endif

#define NOTE_NAME "statsd" // The note to set with the stat info.
#define ROOT_NAME "ROOT."  // The name for the stat when / is hit.
                           // Note: requires trailing .

// module configuration - this is basically a global struct
typedef struct {
    int enabled;     // module enabled?
    int divider;     // divide the request time by this number
    int socket;      // statsd connection
    char *host;      // statsd host
    char *port;      // statsd port
    char *prefix;    // prefix for stats
    char *stat;      // the stat itself, if provided in the config
    char *suffix;    // suffix for stats
    apr_array_header_t *exclude_regex;
                    // List of regexes to exclude path parts from stats
} settings_rec;

module AP_MODULE_DECLARE_DATA statsd_module;

// ******************************
// Connect to the remote socket
// ******************************

int _connect_to_statsd( settings_rec *cfg )
{
    // Grab 2 structs for the connection
    struct addrinfo *statsd;
    statsd = malloc(sizeof(struct addrinfo));

    struct addrinfo *hints;
    hints = malloc(sizeof(struct addrinfo));

    // what type of socket is the statsd endpoint?
    hints->ai_family   = AF_INET;
    hints->ai_socktype = SOCK_DGRAM;
    hints->ai_protocol = IPPROTO_UDP;
    hints->ai_flags    = 0;

    // using getaddrinfo lets us use a hostname, rather than an
    // ip address.
    int err;
    if( err = getaddrinfo( cfg->host, cfg->port, hints, &statsd ) != 0 ) {
        _DEBUG && fprintf( stderr, "getaddrinfo on %s:%s failed: %s\n",
            cfg->host, cfg->port, gai_strerror(err) );
        return -1;
    }

    // ******************************
    // Store the open connection
    // ******************************

    // getaddrinfo() may return more than one address structure
    // but since this is UDP, we can't verify the connection
    // anyway, so we will just use the first one
    cfg->socket = socket( statsd->ai_family, statsd->ai_socktype,
                            statsd->ai_protocol );

    if( cfg->socket == -1 ) {
        _DEBUG && fprintf( stderr, "socket creation failed\n" );
        close( cfg->socket );
        return -1;
    }

    // connection failed.. for some reason...
    if( connect( cfg->socket, statsd->ai_addr, statsd->ai_addrlen ) == -1 ) {
        _DEBUG && fprintf( stderr, "socket connection failed\n" );
        close( cfg->socket );
        return -1;
    }

    // now that we have an outgoing socket, we don't need this
    // anymore.
    // XXX In high request rate environments, freeing this struct
    // causes segfaults.. very confusing...
    //freeaddrinfo( statsd );
    //freeaddrinfo( hints );

    _DEBUG && fprintf( stderr, "statsd server: %s:%s (fd: %d)\n",
                cfg->host, cfg->port, cfg->socket );

    return cfg->socket;
}

// See here for the structure of request_rec:
// http://ci.apache.org/projects/httpd/trunk/doxygen/structrequest__rec.html
static int request_hook(request_rec *r)
{   settings_rec *cfg = ap_get_module_config( r->per_dir_config,
                                              &statsd_module );

    /* Do not run in subrequests, don't run if not enabled */
    if( !(cfg->enabled || r->main) ) {
        return DECLINED;
    }

    // XXX I can't find a way to run after the directory config has
    // been completed, so we can open a single statsd connection for
    // that config. So instead, we'll do a check here and initialize
    // it if it's not already there.
    int sock = cfg->socket > 0 ? cfg->socket : _connect_to_statsd( cfg );

    // If we didn't get a socket, don't bother trying to send
    if( sock == -1 ) {
        _DEBUG && fprintf( stderr, "Could not get Statsd socket\n" );
        return DECLINED;
    }

    // The stat, minus prefix/suffix/status code etc.
    char *key = cfg->stat;

    // If you provided the key as part of the configuration, we'll use
    // it, otherwise we will infer it from the path
    if( !strlen(key) ) {

        _DEBUG && fprintf( stderr, "stat key not set in config\n" );

        // The path, cleaned up.
        char *path = apr_pstrdup( r->pool, r->uri );

        // Remove leading slashes - quick fix for leading double slashes.
        while( path[0] == '/' ) { path++; }

        // Iterate over the path parts, get rid of slashes and unwanted
        // parts. Or, default to a root name.
        if( *path == 0 ) {
            key = ROOT_NAME;

        } else {
            char *last_part;
            char *part = apr_strtok( apr_pstrdup( r->pool, path ), "/", &last_part );

            while( part != NULL ) {

                // This is just more stacked slashes, move on.
                if( part == 0 ) {
                    // And get the next part -- has to be done at every break
                    part = apr_strtok( NULL, "/", &last_part );
                    continue;
                }

                // Maybe we don't want this path part in the stat; check
                // the exclude regex list.
                int i;
                int exclude_part = 0;
                for( i = 0; i < cfg->exclude_regex->nelts; i++ ) {

                    ap_regex_t *regex = ((ap_regex_t **)cfg->exclude_regex->elts)[i];

                    _DEBUG && fprintf(
                        stderr, "checking white list prefix id: %d\n", i );

                    // ap_regexec returns 0 if there was a match
                    if( !ap_regexec( regex, part, 0, NULL, 0 ) ) {
                        _DEBUG && fprintf( stderr,
                                    "Part %s matches regex id %d\n", part, i );
                        exclude_part = 1;
                        break;
                    }
                }

                // We don't want this bit in the stat
                if( exclude_part ) {
                    // And get the next part -- has to be done at every break
                    part = apr_strtok( NULL, "/", &last_part );
                    continue;
                }

                _DEBUG && fprintf( stderr, "part = %s\n", part );

                key = apr_pstrcat( r->pool, key, part, ".", NULL );

                _DEBUG && fprintf( stderr, "key so far = %s\n", key );

                // and move the pointer
                part = apr_strtok( NULL, "/", &last_part );
            }
        }
    }

    // Request time until now
    char *duration = apr_psprintf(
                        r->pool, "%" APR_TIME_T_FMT,
                        (apr_time_now() - r->request_time) / cfg->divider);

    _DEBUG && fprintf( stderr, "duration %s\n", duration );

    // The entire stat, to be sent. Once as a timer, once as a counter.
    // Looks something like: prefix.keyname.suffix.GET.200
    char *stat = apr_pstrcat(
                    r->pool,
                    cfg->prefix,
                    key,         // no dot between key & method because key
                    cfg->suffix, // will always end in a dot.
                    r->method,
                    ".",
                    apr_psprintf(r->pool, "%d", r->status),
                    NULL
                 );


     _DEBUG && fprintf( stderr, "stat: %s\n", stat );

    // New enough versions of Statsd (which is all we will support),
    // support sending multiple stats in a single packet, delimited by
    // newlines. So do that here.
    char *to_send = apr_pstrcat(
                        r->pool,
                        stat, ":1|c",               // counter
                        "\n",
                        stat, ":", duration, "|ms", // timer
                        NULL
                    );

    _DEBUG && fprintf( stderr, "Will be sending to fd %d: %s\n", sock, to_send );

    // Send of the stat
    int len  = strlen(to_send);
    int sent = write( sock, to_send, len );

    _DEBUG && fprintf( stderr, "Sent %d of %d bytes to FD %d\n", sent, len, sock );

    // Should we unset the socket if this happens?
    if( sent != len ) {
        _DEBUG && fprintf( stderr, "Partial/failed write for %s\n", stat );
        _DEBUG && fflush( stderr );
    }

    char *note = apr_psprintf(r->pool, "%s %s %d", stat, duration, sent);
    _DEBUG && fprintf( stderr, "setting note %s: %s\n", NOTE_NAME, note );
     apr_table_setn(r->notes, NOTE_NAME, note);

    // We need to flush the stream for messages to appear right away.
    // Performing an fflush() in a production system is not good for
    // performance - don't do this for real.
    _DEBUG && fflush(stderr);

    return OK;
}

/* ********************************************

    Default settings

   ******************************************** */

/* initialize all attributes */
static void *init_settings(apr_pool_t *p, char *d)
{
    settings_rec *cfg;

    cfg = (settings_rec *) apr_pcalloc(p, sizeof(settings_rec));
    cfg->enabled        = 0;
    cfg->divider        = 1000; // default to milliseconds for timing
    cfg->host           = "localhost";
    cfg->port           = "8125";
    cfg->stat           = "";
    cfg->prefix         = "";
    cfg->suffix         = "";
    cfg->exclude_regex  = apr_array_make(p, 2, sizeof(ap_regex_t*) );

    return cfg;
}

/* ********************************************

    Parse settings

   ******************************************** */

/* Set the value of a config variabe, strings only */
static const char *set_config_value(cmd_parms *cmd, void *mconfig,
                                    const char *value)
{
    settings_rec *cfg;

    cfg = (settings_rec *) mconfig;

    char name[50];
    sprintf( name, "%s", cmd->cmd->name );

    /*
     * Apply restrictions on attributes.
     */
    if( strlen(value) == 0 ) {
        return apr_psprintf(cmd->pool, "%s not allowed to be NULL", name);
    }

    if( strcasecmp(name, "StatsdHost") == 0 ) {
        cfg->host = apr_pstrdup(cmd->pool, value);

    } else if( strcasecmp(name, "StatsdPort") == 0 ) {
        cfg->port = apr_pstrdup(cmd->pool, value);

    } else if( strcasecmp(name, "StatsdPort") == 0 ) {
        cfg->host = apr_pstrdup(cmd->pool, value);

    } else if( strcasecmp(name, "StatsdPrefix") == 0 ) {
        // Make sure the last character of prefix is a .

        char *copy = apr_pstrdup(cmd->pool, value);
        copy += strlen(copy) - 1;
        _DEBUG && fprintf( stderr, "prefix last char = %s\n", copy );

        char *sep  = *copy == '.' ? "" : ".";
        cfg->prefix = apr_pstrcat(cmd->pool, value, sep, NULL);

        _DEBUG && fprintf( stderr, "prefix = %s\n", cfg->prefix );


    } else if( strcasecmp(name, "StatsdSuffix") == 0 ) {
        // Make sure the first character of suffix is NOT a .
        // as we're auto-appending a . to any key we get already.
        // we'd end up with 2 dots in a row; one from the auto append,
        // one from the suffix.
        // Also, make sure we have a trailing dot, as the method &
        // status code will be appended after without checking for
        // trailing dots of the suffix
        if( *value == '.' ) { value++; }

        cfg->suffix = apr_pstrcat(cmd->pool, value, ".", NULL);

        _DEBUG && fprintf( stderr, "suffix = %s\n", cfg->suffix );

    } else if( strcasecmp(name, "StatsdStat") == 0 ) {

        // The stat key always needs to ends in a . so might
        // as well add it here.
        cfg->stat = apr_pstrcat( cmd->pool,
                        apr_pstrdup(cmd->pool, value),
                        ".",
                        NULL );

    } else if( strcasecmp(name, "StatsdTimeUnit") == 0 ) {

        // Timing is in microseconds, so we may have to convert
        // to some other unit.
        cfg->divider =
            strcasecmp( value, "seconds"      ) == 0 ? 1000 * 1000  :
            strcasecmp( value, "milliseconds" ) == 0 ? 1000         :
            strcasecmp( value, "microseconds" ) == 0 ? 1            :
            1000;   // default back to milliseconds if you gave us garbage.

    /* Regexes of path parts that will not be part of the stat */
    } else if( strcasecmp(name, "StatsdExclude") == 0 ) {

        // following tutorial here:
        // http://dev.ariel-networks.com/apr/apr-tutorial/html/apr-tutorial-19.html
        ap_regex_t *regex = ap_pregcomp(
                                cmd->pool,
                                apr_pstrdup(cmd->pool, value),
                                AP_REG_EXTENDED | AP_REG_ICASE
                             );

        *(ap_regex_t**)apr_array_push(cfg->exclude_regex) = regex;

    } else {
        return apr_psprintf(cmd->pool, "No such variable %s", name);
    }

    return NULL;
}

/* Set the value of a config variabe, ints/booleans only */
static const char *set_config_enable(cmd_parms *cmd, void *mconfig,
                                    int value)
{
    settings_rec *cfg;

    cfg = (settings_rec *) mconfig;

    char name[50];
    sprintf( name, "%s", cmd->cmd->name );

    if( strcasecmp(name, "Statsd") == 0 ) {
        cfg->enabled     = value;

    } else {
        return apr_psprintf(cmd->pool, "No such variable %s", name);
    }

    return NULL;
}

/* ********************************************

    Configuration options

   ******************************************** */

static const command_rec commands[] = {
    AP_INIT_FLAG(   "Statsd",          set_config_enable,  NULL, OR_FILEINFO,
                    "Whether or not to enable Statsd module"),
    AP_INIT_TAKE1(  "StatsdHost",      set_config_value,   NULL, OR_FILEINFO,
                    "The address of your Statsd server"),
    AP_INIT_TAKE1(  "StatsdPort",      set_config_value,   NULL, OR_FILEINFO,
                    "The port of your Statsd server" ),
    AP_INIT_TAKE1(  "StatsdTimeUnit",  set_config_value,   NULL, OR_FILEINFO,
                    "The unit for timers sent to Statsd" ),
    AP_INIT_TAKE1(  "StatsdPrefix",    set_config_value,   NULL, OR_FILEINFO,
                    "A string to prefix to all the stats sent" ),
    AP_INIT_TAKE1(  "StatsdSuffix",    set_config_value,   NULL, OR_FILEINFO,
                    "A string to suffix to all the stats sent" ),
    AP_INIT_TAKE1(  "StatsdStat",      set_config_value,   NULL, OR_FILEINFO,
                    "The stat itself to send"),
    AP_INIT_ITERATE("StatsdExclude",   set_config_value,   NULL, OR_FILEINFO,
                    "A list of regexes of path parts to exclude from stats" ),
    {NULL}
};

/* ********************************************

    Register module to Apache

   ******************************************** */

static void register_hooks(apr_pool_t *p)
{   // A fixup hook is invoked just before the content part, and the
    // response code isn't set yet, so we can't use that. We'll use
    // a log hook instead, and for testing, check the notes set.
    ap_hook_log_transaction( request_hook, NULL, NULL, APR_HOOK_FIRST );
}

module AP_MODULE_DECLARE_DATA statsd_module = {
    STANDARD20_MODULE_STUFF,
    init_settings,              /* dir config creater */
    NULL,                       /* dir merger --- default is to override */
    NULL,                       /* server config */
    NULL,                       /* merge server configs */
    commands,                   /* command apr_table_t */
    register_hooks              /* register hooks */
};


