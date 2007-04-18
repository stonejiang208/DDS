eval '(exit $?0)' && eval 'exec perl -S $0 ${1+"$@"}'
    & eval 'exec perl -S $0 $argv:q'
    if 0;

# $Id$
# -*- perl -*-

use Env (ACE_ROOT);
use lib "$ACE_ROOT/bin";
use PerlACE::Run_Test;

$status = 0;

$repo_bit_opt = "-ORBSvcConf tcp.conf";

if ($ARGV[0] eq 'udp') {
  $svc_conf = " -ORBSvcConf udp.conf ";
}
else {
    $svc_conf = " -ORBSvcConf tcp.conf";
}


my($port1) = 10001 + PerlACE::uniqueid() ;
$domains_file = PerlACE::LocalFile ("domain_ids");
$ns_ior = PerlACE::LocalFile ("ns.ior");
$dcpsrepo_ior = PerlACE::LocalFile ("repo.ior");
$arg_ns_ref = "-ORBInitRef NameService=file://$ns_ior";
$common_args = "$arg_ns_ref -DCPSInfoRepo corbaname:rir:#InfoRepo $svc_conf";

unlink $ns_ior;
unlink $dcpsrepo_ior;

$NS = new PerlACE::Process ("$ENV{TAO_ROOT}/orbsvcs/Naming_Service/Naming_Service",
                            "-o $ns_ior");
$DCPSREPO = new PerlACE::Process ("$ENV{DDS_ROOT}/bin/DCPSInfoRepo",
				  "$repo_bit_opt -o $dcpsrepo_ior -d $domains_file "
                                . "-ORBEndpoint iiop://localhost:$port1");
$Subscriber = new PerlACE::Process ("subscriber",
                                    "-DCPSConfigFile sub.ini $common_args");
$Publisher = new PerlACE::Process ("publisher",
                                   "-DCPSConfigFile pub.ini $common_args");

#print $NS->CommandLine() . "\n";
$NS->Spawn();
if (PerlACE::waitforfile_timed ($ns_ior, 5) == -1) {
    print STDERR "ERROR: cannot find file <$ns_ior>\n";
    $NS->Kill();
    exit 1;
}

#print $DCPSREPO->CommandLine() . "\n";
$DCPSREPO->Spawn ();
if (PerlACE::waitforfile_timed ($dcpsrepo_ior, 30) == -1) {
    print STDERR "ERROR: waiting for DCPSInfo IOR file\n";
    $DCPSREPO->Kill ();
    $NS->Kill();
    exit 1;
}

$NSADD = new PerlACE::Process("$ENV{ACE_ROOT}/bin/nsadd",
                              "$arg_ns_ref --name InfoRepo --ior file://$dcpsrepo_ior");
$NSADD->IgnoreExeSubDir(1);
$NSADD->SpawnWaitKill(5);

unlink $dcpsrepo_ior;

#print $Publisher->CommandLine() . "\n";
$Publisher->Spawn ();

#print $Subscriber->CommandLine() . "\n";
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

$ns = $NS->TerminateWaitKill(5);
if ($ns != 0) {
    print STDERR "ERROR: Naming_Service returned $ns\n";
    $status = 1;
}

unlink $ns_ior;

if ($status == 0) {
  print "test PASSED.\n";
} else {
  print STDERR "test FAILED.\n";
}

exit $status;
