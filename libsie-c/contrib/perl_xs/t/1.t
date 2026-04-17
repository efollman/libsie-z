# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl 1.t'

#########################

# change 'tests => 1' to 'tests => last_test_to_print';

use Test::More tests => 223;
BEGIN { use_ok('SoMat::SIE') };

#########################

my $sie_file = "../../t/data/sie_comprehensive2_VBM_20050908.sie";

my $sie = SoMat::SIE->new();
ok($sie, 'new');

#print STDERR "sie=$sie\n";

my $siefh = $sie->file_open($sie_file);
ok($siefh, 'file_open');

my $tags = $siefh->tags;
ok($tags, 'tags');

for my $k (sort keys %$tags) {
    #print STDERR "File Tag $k => $tags->{$k}\n";
    ok($k, 'file_tag');
}

undef $tags;

my $channels = $siefh->get_channels;
ok($channels, 'channels');

while (my $ch = $channels->get_next) {
    ok($ch, 'channels->get_next');

    my $name = $ch->get_name;
    ok($name, 'ch->get_name');

    my $tags = $ch->tags;
    for my $k (sort keys %$tags) {
        #print STDERR "Channel Tag $k => $tags->{$k}\n";
    }

    my $spigot = $ch->attach_spigot;
    ok($spigot, 'ch->attach_spigot');

    my $i = 0;
    while (my $out = $spigot->get_output) {
        my $nscans = $out->num_scans;
        for (my $si = 0; $si < $nscans; $si++) {
            my @scan = $out->scan($si);
            #print STDERR "Scan[$i]: " . join(', ', @scan) . "\n";
            ++$i;
        }
    }
    #print STDERR "\n";
}

undef $channels;

undef $siefh;

$siefh = $sie->file_open("../../t/data/sie_seek_test.sie");
ok($siefh, 'file_open 2');

{
    my $ch = $siefh->get_channel(1);
    ok($ch, 'get_channel');

    my $spigot = $ch->attach_spigot;
    ok($ch, 'attach_spigot');

    my ($block, $scan) = $spigot->lower_bound(0, -1);
    ok($block == 0 && $scan == 0, 'lower_bound before beginning');

    ($block, $scan) = $spigot->lower_bound(0, 1e30);
    ok(!defined $block, 'lower_bound past end');

    ($block, $scan) = $spigot->upper_bound(0, -1);
    ok(!defined $block, 'upper_bound before beginning');

    ($block, $scan) = $spigot->upper_bound(0, 1e30);
    ok($block == 81 && $scan == 3199, 'upper_bound past end');

    ($block, $scan) = $spigot->lower_bound(0, 3.14159);
    ok($block == 0 && $scan == 3142, 'lower_bound 3.14159');

    ($block, $scan) = $spigot->upper_bound(0, 3.14159);
    ok($block == 0 && $scan == 3141, 'upper_bound 3.14159');

    ($block, $scan) = $spigot->lower_bound(0, 15.9999);
    ok($block == 5 && $scan == 0, 'lower_bound 15.9999');

    ($block, $scan) = $spigot->upper_bound(0, 15.9999);
    ok($block == 4 && $scan == 3199, 'upper_bound 15.9999');

    my $end = $spigot->seek(-1);
    ok($end == 82, 'seek end');

    $pos = $spigot->tell;
    ok($pos == 82, 'tell');

    my $output = $spigot->get_output;
    ok(!$output, 'output past end');

    my $pos = $spigot->seek(5);
    ok($pos == 5, 'seek 5');

    $pos = $spigot->tell;
    ok($pos == 5, 'tell');

    $output = $spigot->get_output;
    my ($time) = $output->scan(0);
    ok($time == 16, 'output block 5');

    $pos = $spigot->seek(0);
    ok($pos == 0, 'seek 0');

    $pos = $spigot->tell;
    ok($pos == 0, 'tell');

    $output = $spigot->get_output;
    ($time) = $output->scan(0);
    ok($time == 0, 'output block 0');

    # transforms
    $spigot->disable_transforms(1);

    $pos = $spigot->seek(5);
    ok($pos == 5, 'seek 5');
    $output = $spigot->get_output;
    ($time) = $output->scan(0);
    ok($time == 16000, 'unscaled block 5');

    $spigot->transform_output($output);
    ($time) = $output->scan(0);
    ok($time == 16, 'rescaled block 5');

    $spigot->disable_transforms(0);
    $pos = $spigot->seek(5);
    ok($pos == 5, 'seek 5');
    $output = $spigot->get_output;
    ($time) = $output->scan(0);
    ok($time == 16, 'scaled block 5');
}

{
    my $stream = $sie->new_stream;
    open(my $streamfh, "../../t/data/sie_stream.sie");
    binmode $streamfh;
    my $next = 0;
    my @ch;
    my @spigot;
    my @got_blocks;
    my @got_rows;
    my @done;
    my @expected_blocks =
        (108, 109, 109, 109, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
         1, 1, 1, 1, 109, 109, 109, 109, 109, 109, 109);
    my @expected_rows =
        (2160, 2180, 2180, 2180, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
         1, 1, 1, 1, 1, 1, 2180, 2180, 2180, 2180, 2180, 2180, 2180);
    while (read($streamfh, my $data, 103)) {
        $stream->add_stream_data($data);
        while (my $ch = $stream->get_channel($next)) {
            $ch[$next] = $ch;
            $spigot[$next] = $ch->attach_spigot;
            ok($spigot[$next], "channel $next");
            ++$next;
        }
        for (my $i = 0; $i < $next; ++$i) {
            if (!$done[$i]) {
                while (my $output = $spigot[$i]->get_output) {
                    ++$got_blocks[$i];
                    $got_rows[$i] += $output->num_rows;
                }
            }
            if ($spigot[$i]->done && !$done[$i]) {
                ok(1, "done $i");
                $done[$i] = 1;
            }
        }
    }
    for (my $i = 0; $i < $next; ++$i) {
        ok($got_blocks[$i] == $expected_blocks[$i],
           "blocks $i: $got_blocks[$i] == $expected_blocks[$i]");
        ok($got_rows[$i] == $expected_rows[$i],
           "rows $i: $got_rows[$i] == $expected_rows[$i]");
    }
}

undef $siefh;

undef $sie;

#print "sie=undef\n";
