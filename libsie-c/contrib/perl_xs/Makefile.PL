use 5.008;
use ExtUtils::MakeMaker;
use Getopt::Long;

sub error {
    my ($msg) = @_;

    $msg = (defined $msg) ? "Error: $msg\n" : '';
    $msg .= <<'EOT';
Usage: perl Makefile.PL [--sie-prefix=PFX|--sie-include=INC --sie-lib=LIB]
    [--apr-prefix=PFX|--apr-include=INC --apr-lib=LIB]
EOT
    die($msg);
}

my $opts = { # Defaults
    #'apr-prefix' => '/usr/local',
    'sie-prefix' => '/usr/local',
};

GetOptions($opts,
    'sie-prefix=s', 'sie-include=s', 'sie-lib=s',
    'apr-prefix=s', 'apr-include=s', 'apr-lib=s',
    'apr-emulation',
) or error();

my @inc;
#@inc = ('-g'); # Debugging
my @lib;
my @def;

for my $comp (qw(sie apr)) {
    my $pfx = $opts->{"$comp-prefix"};
    if (defined $pfx) {
        $opts->{"$comp-lib"} ||= "$pfx/lib";
        $opts->{"$comp-include"} ||= "$pfx/include/sie";
    }
    push @lib, qq{-L$opts->{"$comp-lib"}} if $opts->{"$comp-lib"};
    push @inc, $opts->{"$comp-include"} if $opts->{"$comp-include"};
}

push @def, '-DSIE_LIBAPR_EMULATION=1' if $opts->{'apr-emulation'};
push @def, '-D_LARGEFILE64_SOURCE=1' if $opts->{'apr-emulation'};
push @lib, '-lapr-1' if $opts->{'apr-lib'};
push @lib, '-lsie', '-lm';

#error("Unknown argument(s): " . join(' ', @ARGV)) if @ARGV;

# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.

WriteMakefile(
    'NAME'		=> 'SoMat::SIE',
    'VERSION_FROM'	=> 'SIE.pm',
    'PREREQ_PM'		=> {}, # e.g., Module::Name => 1.1
    ($] >= 5.005 ?    ## Add these new keywords supported since 5.005
      (ABSTRACT_FROM => 'SIE.pm',
       AUTHOR     => 'Chris LaReau <lareau@somat.com>') : ()),
    #'LIBS'		=> ['-L../libsie -lsie -L../../build/apr-1.2.2/.libs -lapr-1 -lm'],
    'LIBS'		=> [join(' ', @lib)],
    'DEFINE'		=> join(' ', @def),
    #'INC'		=> "-I. -I../../include -I../../build/apr-1.2.2/include",
    'INC'		=> join(' ', map { "-I$_" } @inc),
	# Un-comment this if you add C files to link with later:
    # 'OBJECT'		=> '$(O_FILES)', # link all the C files too
);
