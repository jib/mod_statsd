#!/usr/bin/perl

### also test with:
### curl -v -H 'Cookie: a=1' -H 'Cookie: b=2; c=3' 'http://localhost:7000/basic?x=y'

### XXX run out of semaphores? Can happen in testing:
### ipcs  | grep 0x0 | awk '{print $2}' | xargs -I% ipcrm -s %

use strict;
use warnings;
use FindBin;
use Test::More      'no_plan';
use HTTP::Date      qw[str2time];
use Getopt::Long;
use Data::Dumper;
use LWP::UserAgent;

my $Base        = "http://localhost:7000";
my $Debug       = 0;
my $Statsd      = 0;    # is statsd running on the default port?
my $LogFile     = "$FindBin::Bin/diag.log";
my $HTTPResp    = 200;
my $StartTime   = time;

GetOptions(
    'base=s'    => \$Base,
    'debug'     => \$Debug,
    'statsd'    => \$Statsd,
    'logfile=s' => \$LogFile,
);


### XXX note - any numbers in the URL will be response
### codes from the node service, so pick wisely and adjust
### the return value stat accordingly
my %Map     = (
    ### module is not turned on
    none                    => { qs => '-' },
    basic                   => { qs => 'basic.GET.200' },
    '///basic////'          => { qs => 'basic.GET.200' },
    'basic/404'             => { qs => 'basic.404.GET.404', resp => 404 },
    '///basic///foo///'     => { qs => 'basic.foo.GET.200' },
    regex                   => { qs => 'regex.GET.200' },
    'regex/200'             => { qs => 'regex.GET.200' },
    'regex/404/bar/210'     => { qs => 'regex.bar.GET.404', resp => 404 },
    'regex/exclude/bar/200' => { qs => 'regex.bar.GET.200' },
    'predefined'            => { qs => 'predefined.GET.200' },
    'predefined/foo'        => { qs => 'predefined.GET.200' },
    'predefined/404'        => { qs => 'predefined.GET.404', resp => 404 },
    presuf                  => { qs => 'prefix.presuf.suffix.GET.200' },
    'presuf/foo'            => { qs => 'prefix.presuf.foo.suffix.GET.200' },
);

### This does all the requests
for my $endpoint ( sort keys %Map ) {

    ### build the test
    my $url     = "$Base/$endpoint?";
    my $ua      = LWP::UserAgent->new();
    my $conf    = $Map{ $endpoint };
    my $code    = $conf->{resp} || $HTTPResp;

    ### make the request
    my $res     = $ua->get($url . $conf->{qs});

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
        my $path  = $line->{'PATH'};
        my $qs    = $line->{'QS'};
        my $note  = $line->{'NOTE'};
        my @parts = split / /, $note;

        ### if we didn't disable the module, the note field looks something like:
        ### prefix.keyname.suffix.GET.200 1234 45
        if( $note ne '-' ) {
            like( $parts[0], qr/GET.\d{3}$/, "  Key is a valid looking stat" );
            like( $parts[1], qr/^\d+$/,      "  Resp time is a valid looking number" );
            like( $parts[2], qr/^\d+$/,      "  Chars sent is a valid looking number" );

            ### depending on if statsd is on or not, the results vary for the last
            ### part of the header; -1 indicates a failure to send, so no statsd
            cmp_ok( $parts[2], ($Statsd ? '>' : '<'), 0,
                                            "    Chars sent is expected: $parts[2]" );
        }

        ### the stat sent
        is( "?$parts[0]", $qs,              "  Stat sent as expected: $parts[0]" );
    }
}
