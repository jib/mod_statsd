#!/usr/bin/perl

### ipcs  | grep 0x0 | awk '{print $2}' | xargs -I% ipcrm -s %

use strict;
use warnings;
use FindBin;
use Test::More      'no_plan';
use Getopt::Long;
use Data::Dumper;
use LWP::UserAgent;

my $Base        = "http://localhost:7000";
my $Debug       = 0;
my $Statsd      = 0;    # is statsd running on the default port?
my $LogFile     = "$FindBin::Bin/diag.log";
my $HTTPResp    = 200;
my $StartTime   = time;
my $TestPhp     = 0;

GetOptions(
    'base=s'    => \$Base,
    'debug'     => \$Debug,
    'statsd'    => \$Statsd,
    'php'       => \$TestPhp,
    'logfile=s' => \$LogFile,
);


### XXX note - any numbers in the URL will be response
### codes from the node service, so pick wisely and adjust
### the return value stat accordingly
my %Map     = (
    ### module is not turned on
    none                    => { expect => '-' },
    basic                   => { expect => 'basic.GET.200' },
    '///basic////'          => { expect => 'basic.GET.200' },
    '/basic/x.y'            => { expect => 'basic.x_y.GET.200' },
    '/basic/x:y|z'          => { expect => 'basic.x_y_z.GET.200' },
    'basic/404'             => { expect => 'basic.404.GET.404', resp => 404 },
    '///basic///foo///'     => { expect => 'basic.foo.GET.200' },
    regex                   => { expect => 'regex.GET.200' },
    'regex/200'             => { expect => 'regex.GET.200' },
    'regex/404/bar/210'     => { expect => 'regex.bar.GET.404', resp => 404 },
    'regex/exclude/bar/200' => { expect => 'regex.bar.GET.200' },
    'regex/alsoexcluded/'   => { expect => 'regex.GET.200' },
    'predefined'            => { expect => 'predefined.GET.200' },
    'predefined/foo'        => { expect => 'predefined.GET.200' },
    'predefined/404'        => { expect => 'predefined.GET.404', resp => 404 },
    presuf                  => { expect => 'prefix.presuf.GET.200.suffix' },
    'presuf/foo'            => { expect => 'prefix.presuf.foo.GET.200.suffix' },
    nodot_presuf            => { expect => 'prefix.nodot_presuf.GET.200.suffix' },
    'nodot_presuf/foo'      => { expect => 'prefix.nodot_presuf.foo.GET.200.suffix' },
    auth                    => { expect => 'auth.GET.403', resp => 403 },
    'header/stat'           => { expect => 'set.via.header.GET.200' },
);

### Only add the tests if requested
if( $TestPhp ) {
    %Map = (
        %Map,
        'php/resp.php'          => { expect => 'php.resp_php.GET.200' },
        'php/resp.php?resp=403' => { expect => 'php.resp_php.GET.403', resp => 403 },
        'php/resp.php?resp=503' => { expect => 'php.resp_php.GET.503', resp => 503},
        'php/syntax_error.php'  => { expect => 'php.syntax_error_php.GET.500', resp => 500 },
        'php/note.php'          => { expect => 'set.via.note.GET.200' }
    );
}

### This does all the requests
for my $endpoint ( sort keys %Map ) {

    ### build the test
    my $url     = "$Base/$endpoint";
    my $ua      = LWP::UserAgent->new();
    my $conf    = $Map{ $endpoint };
    my $code    = $conf->{resp} || $HTTPResp;

    ### make the request
    my $res     = $ua->get($url, 'X-Expect' => $conf->{expect});

    diag $res->as_string if $Debug;

    ### inspect
    ok( $res,                   "Got /$endpoint" );
    is( $res->code, $code,      "  HTTP Response = $code" );
}

### Now the logs are filled, and we'll evaluate the notes that
### were added after we started this script.
{   open my $fh, $LogFile or die "Could not open $LogFile: $!";

    ### every line is a perl hash:
    ### '{ TS => "%{%s}t", PATH => "%U", NOTE => "%{statsd}n", QS => "%q" }'

    while( <$fh> ) {
        chomp;
        my $line = eval $_;

        ### some error?
        if( $@ ) {
            ok( 0, "Could not parse $_: $@" );
            next;
        }

        ### Old log lines
        next if $line->{'TS'} < $StartTime;

        diag $_ if $Debug;

        ### Now, let's look at the line and test it.
        my $path   = $line->{'PATH'};
        my $expect = $line->{'HEADER'};
        my $note   = $line->{'NOTE'};
        my @parts  = split / /, $note;

        ### if we didn't disable the module, the note field looks something like:
        ### prefix.keyname.suffix.GET.200 1234 45
        if( $note ne '-' ) {
            like( $parts[0], qr/GET.\d{3}(?:\.|$)/,
                                        "  Key is a valid looking stat" );
            like( $parts[1], qr/^\d+$/, "  Resp time is a valid looking number" );
            like( $parts[2], qr/^\d+$/, "  Chars sent is a valid looking number" );

            ### depending on if statsd is on or not, the results vary for the last
            ### part of the header; -1 indicates a failure to send, so no statsd
            cmp_ok( $parts[2], ($Statsd ? '>' : '<'), 0,
                                            "    Chars sent is expected: $parts[2]" );
        }

        ### the stat sent
        is( $parts[0], $expect,             "  Stat sent as expected: $parts[0]" );
    }
}
