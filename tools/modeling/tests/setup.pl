eval '(exit $?0)' && eval 'exec perl -S $0 ${1+"$@"}'
    & eval 'exec perl -S $0 $argv:q'
    if 0;

# Test setup for OpenDDS Modeling SDK tests that require code generation steps
# before they have .mpc files available for mwc.pl to see.
# This should be run after ../build.pl and before MPC.

use strict;
use Cwd;
use Env qw(ACE_ROOT DDS_ROOT JAVA_HOME);
use lib "$ACE_ROOT/bin", "$DDS_ROOT/bin";
use PerlDDS::Run_Test;

my $dir = 'tools/modeling/tests';
my $test_lst = "$DDS_ROOT/$dir/modeling_tests.lst";

sub get_dirs {
  if ($#ARGV >= 0) {
    print "Overriding dir list\n";
    return @ARGV;
  }

  my $config_list = new PerlACE::ConfigList;
  $config_list->load($test_lst);
  $config_list->add_one_config('COMPILE_ONLY');
  return map {s/^$dir\/(.*)\/[^\/]*$/$1/; $_} $config_list->valid_entries();
}

my $javapkg = 'org.opendds.modeling.sdk'; # holds the .xsl files
my $plugin = 'org.opendds.modeling.sdk.model.editor';

sub generate {
  my $base = shift;
  my $status;
  print "Running code generation on: $base\n";
  $status = system("\"$JAVA_HOME/bin/java\" -classpath " .
                   "$DDS_ROOT/tools/modeling/plugins/$plugin/bin $javapkg." .
                   "model.GeneratorSpecification.Generator.SdkGenerator " .
                   "$base\n");
  if ($status > 0) {
    print "ERROR: Java SdkGenerator invocation failed with $status\n";
    exit($status >> 8);
  }
}

my $cwd = getcwd();
foreach my $dir (get_dirs()) {
  chdir $cwd . '/' . $dir or die "Can't change to $dir\n";
  my @ddsfiles = glob '*.gen';
  if ($#ddsfiles == -1) {
    die "Can't find a .gen file in " . getcwd() . "\n";
  }

  foreach my $base (@ddsfiles) {
    #print "Considering $cwd/$dir/$base\n";
    generate($base);
  }
}
