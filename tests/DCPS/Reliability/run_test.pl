eval '(exit $?0)' && eval 'exec perl -S $0 ${1+"$@"}'
    & eval 'exec perl -S $0 $argv:q'
    if 0;

# $Id$
# -*- perl -*-

use Env (DDS_ROOT);
use lib "$DDS_ROOT/bin";
use Env (ACE_ROOT);
use lib "$ACE_ROOT/bin";
use PerlDDS::Run_Test;

PerlDDS::add_lib_path('./IDL');

$status = 0;

$pub_opts = "-DCPSTransportDebugLevel 1";
$sub_opts = "";


if (($ARGV[0] eq 'shmem') ||  ($ARGV[1] eq 'shmem')) {
  $pub_opts = "$pub_opts -DCPSConfigFile shmem.ini ";
  $sub_opts = "$sub_opts -DCPSConfigFile shmem.ini ";
}
elsif (($ARGV[0] eq 'rtps') ||  ($ARGV[1] eq 'rtps')) {
  $pub_opts = "$pub_opts -DCPSConfigFile rtps.ini ";
  $sub_opts = "$sub_opts -DCPSConfigFile rtps.ini ";
}
elsif (($ARGV[0] eq 'mcast') ||  ($ARGV[1] eq 'mcast')) {
  $pub_opts = "$pub_opts -DCPSConfigFile multicast.ini ";
  $sub_opts = "$sub_opts -DCPSConfigFile multicast.ini ";
}

if (($ARGV[0] eq 'take-next') ||  ($ARGV[1] eq 'take-next')) {
  $sub_opts .= "$sub_opts -take-next ";
}
elsif (($ARGV[0] eq 'take') ||  ($ARGV[1] eq 'take')) {
  $sub_opts .= "$sub_opts -take ";
}
elsif (($ARGV[0] eq 'zero-copy') ||  ($ARGV[1] eq 'zero-copy')) {
  $sub_opts .= "$sub_opts -zero-copy ";
}

$dcpsrepo_ior = "repo.ior";

unlink $dcpsrepo_ior;

$DCPSREPO = PerlDDS::create_process ("$ENV{DDS_ROOT}/bin/DCPSInfoRepo",
                                     "-o $dcpsrepo_ior ");
$Subscriber = PerlDDS::create_process ("subscriber", " $sub_opts");
$Publisher = PerlDDS::create_process ("publisher", " $pub_opts");

print $DCPSREPO->CommandLine() . "\n";
$DCPSREPO->Spawn ();
if (PerlACE::waitforfile_timed ($dcpsrepo_ior, 30) == -1) {
    print STDERR "ERROR: waiting for Info Repo IOR file\n";
    $DCPSREPO->Kill ();
    exit 1;
}

print $Publisher->CommandLine() . "\n";
$Publisher->Spawn ();

print $Subscriber->CommandLine() . "\n";
$Subscriber->Spawn ();


$PublisherResult = $Publisher->WaitKill (300);
if ($PublisherResult != 0) {
    print STDERR "ERROR: publisher returned $PublisherResult \n";
    $status = 1;
}

$SubscriberResult = $Subscriber->WaitKill (15);
if ($SubscriberResult != 0) {
    print STDERR "ERROR: subscriber returned $SubscriberResult \n";
    $status = 1;
}

$ir = $DCPSREPO->TerminateWaitKill(5);
if ($ir != 0) {
    print STDERR "ERROR: DCPSInfoRepo returned $ir\n";
    $status = 1;
}

unlink $dcpsrepo_ior;

if ($status == 0) {
  print "test PASSED.\n";
} else {
  print STDERR "test FAILED.\n";
}

exit $status;
