/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "DataReaderListenerImpl.h"
#include "Boilerplate.h"

#include <dds/DCPS/Service_Participant.h>
#include <model/Sync.h>

#ifdef ACE_AS_STATIC_LIBS
#include <dds/DCPS/transport/tcp/Tcp.h>
#endif

int
ACE_TMAIN(int argc, ACE_TCHAR *argv[])
{
  try {
    // Initialize DomainParticipantFactory, handling command line args
    DDS::DomainParticipantFactory_var dpf =
      TheParticipantFactoryWithArgs(argc, argv);

    // Create domain participant
    DDS::DomainParticipant_var participant = createParticipant(dpf);

    // Register type support and create topic
    DDS::Topic_var topic = createTopic(participant);

    // Create subscriber
    DDS::Subscriber_var subscriber = createSubscriber(participant);

    // Create Listener
    DDS::DataReaderListener_var listener(new DataReaderListenerImpl);

    // Create DataReader with the listener attached
    DDS::DataReader_var reader = createDataReader(subscriber,
                                                  topic,
                                                  listener);

    {
      // Block until reader has associated with a writer
      // but is no longer associated with any writer
      OpenDDS::Model::ReaderSync rs(reader);
    }

    // Clean-up!
    cleanup(participant, dpf);

  } catch (const CORBA::Exception& e) {
    e._tao_print_exception("Exception caught in main():");
    return -1;
  } catch (std::string& msg) {
    ACE_ERROR_RETURN((LM_ERROR, ACE_TEXT("ERROR: main() - %s\n"),
                      msg.c_str()), -1);
  }

  return 0;
}
