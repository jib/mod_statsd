#!/usr/bin/perl

### also test with:
### curl -v -H 'Cookie: a=1' -H 'Cookie: b=2; c=3' 'http://localhost:7000/basic?x=y'

### XXX run out of semaphores? Can happen in testing:
### ipcs  | grep 0x0 | awk '{print $2}' | xargs -I% ipcrm -s %

use strict;
use warnings;
use Test::More      'no_plan';
use HTTP::Date      qw[str2time];
use Getopt::Long;
use Data::Dumper;
use HTTP::Cookies;
use LWP::UserAgent;

my $Base                = "http://localhost:7000";
my $Debug               = 0;
my $DefaultCookies      = [ Cookie => 'a=1', Cookie => 'b=2; c=3, d' ];
my $DefaultBody         = '{ "a": "1", "b": "2", "c": "3" }';

GetOptions(
    'base=s'            => \$Base,
    'debug'             => \$Debug,
);

my %Map     = (
    ### module is not turned on
    "none/200"  => {
        tests   => [ '' ],  # we expect an empty body
    },

    ### straight forward conversion
    basic       => { },

    ### callback
    callback    => {
        query_string    => "foo=bar&callback=cb&baz=zot",
        tests           => [
            qr/^cb\({\n/,
            qr/status: 200,\n/,
            qr/body: $DefaultBody\n/,
            qr/}\);$/,
        ],
    },

    ### callback missing the callback parameter
    ### this should NOT return a jsonp callback
    "callback/missing_query_string" => { },

    ### callback with a different query string
    ### this should NOT return a jsonp callback
    "callback/non_matching_query_string" => {
        query_string    => "foo=bar&baz=zot",
    },

    ### whitelist - this only allows 'a' and 'b' prefixes
    whitelist => {
        tests   => [ '{ "a": "1", "b": "2" }' ],
    },

    ### There was a bug that stopped headers from being set
    ### using the "Header" directive when C2JSON was enabled.
    ### Check for that here
    headers => {
        tests   => [
            $DefaultBody,
            sub {
                my $res     = shift;
                my @header  = $res->header( 'X-C2JSON-Header' );

                is( scalar(@header), 1,              "  Found header: @header" );
                is( $header[0],      "Not Filtered", "    Header as expected: @header" );
            },
        ],
    }
);

for my $endpoint ( sort keys %Map ) {

    my $cfg = $Map{ $endpoint };

    ### Defaults in case not provided
    my $headers     = $cfg->{headers}       || [ ];
    my $cookies     = $cfg->{cookies}       || $DefaultCookies;
    my $tests       = $cfg->{tests}         || [ $DefaultBody ];
    my $qs          = $cfg->{query_string}  || '';

    ### build the test
    my $url     = "$Base/$endpoint";
    $url       .= "?$qs" if $qs;
    my $ua      = LWP::UserAgent->new();
    my @req     = ($url, @$cookies, @$headers );

    diag "Sending: @req" if $Debug;

    ### make the request
    my $res     = $ua->get( @req );
    diag $res->as_string if $Debug;

    ### inspect
    ok( $res,                   "Got /$endpoint" );
    is( $res->code, "200",      "  HTTP Response = 200" );

    ### run the individual tests
    my $body    = $res->content;

    ### If we have any body tests, that means we got a response, which should be text/javascript:
    my $ct = length $tests->[0] ? "Text/Javascript" : "Text/Plain";
    is( lc($res->header('Content-Type')), lc( $ct ), "  Content-Type = $ct" );

    for my $test (@$tests) {
        local *isa = *UNIVERSAL::isa;

        isa( $test,  'Regexp' ) ? like( $body, $test, "  Body matches $test" ) :
        isa( \$test, 'SCALAR' ) ? is( $body,   $test, "  Body = ". ($test || '<none>') ) :
        isa( $test,  'CODE'   ) ? $test->( $res ) :
        '';
    }


}
