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

/* ********************************************

    Structs & Defines

   ******************************************** */

#ifdef DEBUG                    // To print diagnostics to the error log
#define _DEBUG 1                // enable through gcc -DDEBUG
#else
#define _DEBUG 0
#endif


// module configuration - this is basically a global struct
typedef struct {
    int enabled;    // module enabled?
    char *host;     // statsd host
    char *port;     // statsd port
    char *prefix;   // prefix for stats
    char *suffix;   // suffix for stats
} settings_rec;

module AP_MODULE_DECLARE_DATA statsd_module;

// See here for the structure of request_rec:
// http://ci.apache.org/projects/httpd/trunk/doxygen/structrequest__rec.html
static int hook(request_rec *r)
{
    settings_rec *cfg = ap_get_module_config( r->per_dir_config,
                                              &statsd_module );

    /* Do not run in subrequests, don't run if not enabled */
    if( !(cfg->enabled || r->main) ) {
        return DECLINED;
    }

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
    cfg->enabled = 0;
    cfg->host    = "localhost";
    cfg->port    = "8125";
    cfg->prefix  = "";
    cfg->suffix  = "";

    return cfg;
}

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
        cfg->prefix = apr_pstrdup(cmd->pool, value);

    } else if( strcasecmp(name, "StatsdSuffix") == 0 ) {
        cfg->suffix = apr_pstrdup(cmd->pool, value);

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
        cfg->enabled           = value;

    } else {
        return apr_psprintf(cmd->pool, "No such variable %s", name);
    }

    return NULL;
}

/* ********************************************

    Configuration options

   ******************************************** */

static const command_rec commands[] = {
    AP_INIT_FLAG( "Statsd",         set_config_enable,  NULL, OR_FILEINFO,
                  "whether or not to enable Statsd module"),
    AP_INIT_TAKE1("StatsdHost",     set_config_value,   NULL, OR_FILEINFO,
                   "The address of your Statsd server"),
    AP_INIT_TAKE1( "StatsdPort",    set_config_value,   NULL, OR_FILEINFO,
                   "The port of your Statsd server" ),
    AP_INIT_TAKE1( "StatsdPrefix",  set_config_value,   NULL, OR_FILEINFO,
                   "A string to prefix to all the stats sent" ),
    AP_INIT_TAKE1( "StatsdSuffix",  set_config_value,   NULL, OR_FILEINFO,
                   "A string to suffix to all the stats sent" ),

    {NULL}
};

/* ********************************************

    Register module to Apache

   ******************************************** */

static void register_hooks(apr_pool_t *p)
{   // Because this is a /handler/, be sure to use ap_hook_handler, and not
    // ap_hook_fixups: http://www.apachetutor.org/dev/request
    ap_hook_handler( hook, NULL, NULL, APR_HOOK_MIDDLE );
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


