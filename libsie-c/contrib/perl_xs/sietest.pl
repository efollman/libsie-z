#
# perl -Iblib/lib -Iblib/arch sietest.pl

use SoMat::SIE;

my $sie_file = "../../t/data/sie_comprehensive2_VBM_20050908.sie";

my $sie = SoMat::SIE->new();

print STDERR "sie=$sie\n";

my $siefh = $sie->file_open($sie_file);

my $tags = $siefh->tags;

for my $k (sort keys %$tags) {
    print STDERR "File Tag $k => $tags->{$k}\n";
}

my $channels = $siefh->get_channels;

while (my $ch = $channels->get_next) {
    my $name = $ch->get_name;

    my $tags = $ch->tags;
    for my $k (sort keys %$tags) {
        print STDERR "Channel Tag $k => $tags->{$k}\n";
    }

    my $spigot = $ch->attach_spigot;

    my $i = 0;
    while (my $out = $spigot->get_output) {
        my $nscans = $out->num_scans;
        for (my $si = 0; $si < $nscans; $si++) {
            my @scan = $out->scan($si);
            #print STDERR "Scan[$i]: " . join(', ', @scan) . "\n";
            ++$i;
        }
    }
    print STDERR "\n";
}

undef $channels;

undef $siefh;

undef $sie;

print "sie=undef\n";
