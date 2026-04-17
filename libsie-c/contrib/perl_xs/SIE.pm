package SoMat::SIE;

use 5.008;
use strict;
use warnings 'all';

our $VERSION = '0.02';

require XSLoader;
XSLoader::load('SoMat::SIE', $VERSION);

package SoMat::SIE::Ref;
our @ISA = qw(SoMat::SIE::Object);

package SoMat::SIE::Intake;
our @ISA = qw(SoMat::SIE::Ref);

package SoMat::SIE::File;
our @ISA = qw(SoMat::SIE::Intake);

package SoMat::SIE::Stream;
our @ISA = qw(SoMat::SIE::Intake);

package SoMat::SIE::Iterator;
our @ISA = qw(SoMat::SIE::Object);

sub get_next {
    my ($s) = @_;
    my $next = $s->_get_next
        or return undef;
    (my $class = $next->class_name) =~ s/^sie_//;

    #printf "Iterator get_next, next=$next, class=$class\n";
    bless $next, "SoMat::SIE::$class";
    return $next;
}

package SoMat::SIE::Channel;
our @ISA = qw(SoMat::SIE::Ref);

sub dimensions {
    my ($s) = @_;
    my $iter = $s->get_dimensions; # KLUDGE
    my @dims;
    if ($iter) {
        while (my $dim = $iter->get_next) {
            my $tags = $dim->tags;
            # KLUDGE add non-tags stuff here
            push @dims, $tags;
        }
    }
    return \@dims;
}

package SoMat::SIE::Test;
our @ISA = qw(SoMat::SIE::Ref);

package SoMat::SIE::Dimension;
our @ISA = qw(SoMat::SIE::Ref);

package SoMat::SIE::Spigot;
our @ISA = qw(SoMat::SIE::Object);

package SoMat::SIE::Tag;
our @ISA = qw(SoMat::SIE::Ref);

package SoMat::SIE::Output;
our @ISA = qw(SoMat::SIE::Object);

package SoMat::SIE::PlotCrusher;
our @ISA = qw(SoMat::SIE::Object);

package SoMat::SIE::Writer;
our @ISA = qw(SoMat::SIE::Object);

package SoMat::SIE::Sifter;
our @ISA = qw(SoMat::SIE::Object);

# Preloaded methods go here.

1;
__END__

=head1 NAME

SoMat::SIE - Perl extension to read SoMat SIE data files.

=head1 SYNOPSIS

  use SoMat::SIE;

  sub dump_hashref {
      my ($h, $header) = @_;
      print "$header\n" if defined $header;
      for my $k (sort keys %$h) {
          print "$k => $h->{$k}\n";
      }
      print "\n";
  }

  my $sie = SoMat::SIE->new();
  my $siefh = $sie->file_open('data_file.sie');

  dump_hashref($siefh->tags, 'File Tags');

  my $channels = $siefh->get_channels;

  while (my $ch = $channels->get_next) {
      dump_hashref($ch->tags, "Channel: " . $ch->get_name);

      my $spigot = $ch->attach_spigot;

      my $i = 0;
      while (my $out = $spigot->get_output) {
          my $nscans = $out->num_scans;
          for (my $si = 0; $si < $nscans; $si++) {
              my @scan = $out->scan($si);
              print "Scan[$i]: " . join(', ', @scan) . "\n";
              ++$i;
          }
      }
      print "\n";
  }
  undef $channels;
  undef $siefh;
  undef $sie;


=head1 ABSTRACT

  Abstract for SoMat::SIE goes here.

=head1 DESCRIPTION

Documentation for SoMat::SIE goes here.

=head2 EXPORT

None.



=head1 SEE ALSO

Mention other useful documentation such as the documentation of
related modules or operating system documentation (such as man pages
in UNIX), or any relevant external documentation such as RFCs or
standards.

If you have a mailing list set up for your module, mention it here.

If you have a web site set up for your module, mention it here.

=head1 AUTHOR

Chris LaReau, E<lt>lareau@somat.comE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright 2006-2015 by SoMat Corporation

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself. 

=cut
