// -*- C++ -*-
//
// $Id$

#include "DcpsInfo_pch.h"
#include "FederatorManagerImpl.h"
#include "dds/DCPS/SubscriberImpl.h"
#include "dds/DCPS/Service_Participant.h"
#include "dds/DCPS/Marked_Default_Qos.h"
#include "dds/DCPS/transport/framework/TheTransportFactory.h"
#include "dds/DCPS/transport/framework/TransportImpl.h"
#include "dds/DCPS/transport/simpleTCP/SimpleTcpConfiguration.h"
#include "dds/DCPS/transport/simpleTCP/SimpleTcp.h"
#include "ace/Log_Priority.h"
#include "ace/Log_Msg.h"

#include "FederatorTypeSupportC.h"
#include "FederatorTypeSupportImpl.h"

#include <string>

#if !defined (__ACE_INLINE__)
# include "FederatorManagerImpl.inl"
#endif /* ! __ACE_INLINE__ */

namespace { // Anonymous namespace for file scope.

  // Starting key value for transport keys to use.
  enum { BASE_TRANSPORT_KEY_VALUE = 30};

} // End of anonymous namespace

namespace OpenDDS { namespace Federator {

ManagerImpl::ManagerImpl( Config& config)
 : joining_( this->lock_),
   joiner_( NIL_REPOSITORY),
   config_( config),
   topicListener_( *this),
   participantListener_( *this),
   publicationListener_( *this),
   subscriptionListener_( *this)
{
  if( ::OpenDDS::DCPS::DCPS_debug_level > 0) {
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) INFO: Federator::ManagerImpl::ManagerImpl()\n")
    ));
  }
}

ManagerImpl::~ManagerImpl()
{
  if( ::OpenDDS::DCPS::DCPS_debug_level > 0) {
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) INFO: Federator::ManagerImpl::~ManagerImpl()\n")
    ));
  }

  // Remove our local participant and contained entities.
  if( 0 == CORBA::is_nil( this->federationParticipant_)) {
    if( ::DDS::RETCODE_PRECONDITION_NOT_MET
         == this->federationParticipant_->delete_contained_entities()
      ) {
      ACE_ERROR ((LM_ERROR,
        ACE_TEXT("(%P|%t) ERROR: Federator::Manager ")
        ACE_TEXT("unable to release resources for repository %d.\n"),
        this->id()
      ));

    } else if( ::DDS::RETCODE_PRECONDITION_NOT_MET
               == TheParticipantFactory->delete_participant( this->federationParticipant_)
             ) {
      ACE_ERROR ((LM_ERROR,
        ACE_TEXT("(%P|%t) ERROR: Federator::Manager ")
        ACE_TEXT("unable to release the participant for repository %d.\n"),
        this->id()));
    }
  }
}

void
ManagerImpl::initialize()
{
  if( ::OpenDDS::DCPS::DCPS_debug_level > 0) {
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) INFO: Federation::ManagerImpl::initialize()\n")
    ));
  }

  // Add participant for Federation domain
  this->federationParticipant_
    = TheParticipantFactory->create_participant(
        this->config_.federationDomain(),
        PARTICIPANT_QOS_DEFAULT,
        ::DDS::DomainParticipantListener::_nil()
      );
  if( CORBA::is_nil( this->federationParticipant_.in())) {
    ACE_ERROR((LM_ERROR,
      ACE_TEXT("(%P|%t) ERROR: create_participant failed for ")
      ACE_TEXT( "repository %d in federation domain %d.\n"),
      this->id(),
      this->config_.federationDomain()
    ));
    throw Incomplete();
  }

  //
  // Add type support for update topics
  //

  ParticipantUpdateTypeSupportImpl* participantUpdate = new ParticipantUpdateTypeSupportImpl();
  if( ::DDS::RETCODE_OK != participantUpdate->register_type(
                             this->federationParticipant_,
                             PARTICIPANTUPDATETYPENAME
                           )
    ) {
    ACE_ERROR((LM_ERROR,
      ACE_TEXT("(%P|%t) ERROR: Unable to install ")
      ACE_TEXT("ParticipantUpdate type support for repository %d.\n"),
      this->id()
    ));
    throw Incomplete();
  }

  TopicUpdateTypeSupportImpl* topicUpdate = new TopicUpdateTypeSupportImpl();
  if( ::DDS::RETCODE_OK != topicUpdate->register_type(
                             this->federationParticipant_,
                             TOPICUPDATETYPENAME
                           )
    ) {
    ACE_ERROR((LM_ERROR,
      ACE_TEXT("(%P|%t) ERROR: Unable to install ")
      ACE_TEXT("TopicUpdate type support for repository %d.\n"),
      this->id()
    ));
    throw Incomplete();
  }

  PublicationUpdateTypeSupportImpl* publicationUpdate = new PublicationUpdateTypeSupportImpl();
  if( ::DDS::RETCODE_OK != publicationUpdate->register_type(
                             this->federationParticipant_,
                             PUBLICATIONUPDATETYPENAME
                           )
    ) {
    ACE_ERROR((LM_ERROR,
      ACE_TEXT("(%P|%t) ERROR: Unable to install ")
      ACE_TEXT("PublicationUpdate type support for repository %d.\n"),
      this->id()
    ));
    throw Incomplete();
  }

  SubscriptionUpdateTypeSupportImpl* subscriptionUpdate = new SubscriptionUpdateTypeSupportImpl();
  if( ::DDS::RETCODE_OK != subscriptionUpdate->register_type(
                             this->federationParticipant_,
                             SUBSCRIPTIONUPDATETYPENAME
                           )
    ) {
    ACE_ERROR((LM_ERROR,
      ACE_TEXT("(%P|%t) ERROR: Unable to install ")
      ACE_TEXT("SubscriptionUpdate type support for repository %d.\n"),
      this->id()
    ));
    throw Incomplete();
  }

  //
  // Create the transport for the update topics.
  //

  ::OpenDDS::DCPS::TransportImpl_rch transport
    = TheTransportFactory->create_transport_impl(
        this->config_.federationDomain(),
        ACE_TString("SimpleTcp"),
        ::OpenDDS::DCPS::DONT_AUTO_CONFIG
      );

  ::OpenDDS::DCPS::TransportConfiguration_rch transportConfig
    = TheTransportFactory->create_configuration(
        this->config_.federationDomain(),
        ACE_TString("SimpleTcp")
      );

  if( transport->configure( transportConfig.in()) != 0) {
    ACE_ERROR((LM_ERROR,
      ACE_TEXT("(%P|%t) ERROR: Federator::ManagerImpl::initialize() - ")
      ACE_TEXT("repository %d failed to initialize subscription transport.\n"),
      this->id()
    ));
    throw Incomplete();
  }

  //
  // Create the subscriber for the update topics.
  //

  ::DDS::Subscriber_var subscriber
    = this->federationParticipant_->create_subscriber(
        SUBSCRIBER_QOS_DEFAULT,
        ::DDS::SubscriberListener::_nil()
      );
  if( CORBA::is_nil( subscriber.in())) {
    ACE_ERROR((LM_ERROR,
      ACE_TEXT("(%P|%t) ERROR: Federator::ManagerImpl::initialize() - ")
      ACE_TEXT("failed to create subscriber for repository %d\n"),
      this->id()
    ));
    throw Incomplete();

  } else if( ::OpenDDS::DCPS::DCPS_debug_level > 4) {
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) INFO: Federator::ManagerImpl::initialize() - ")
      ACE_TEXT("created federation subscriber for repository %d\n"),
      this->id()
    ));

  }

  // Attach the transport to it.
  ::OpenDDS::DCPS::SubscriberImpl* subscriberServant
    = dynamic_cast< ::OpenDDS::DCPS::SubscriberImpl*>(
        subscriber.in()
      );
  if( 0 == subscriberServant) {
    ACE_ERROR((LM_ERROR,
      ACE_TEXT("(%P|%t) INFO: Federator::ManagerImpl::initialize() - ")
      ACE_TEXT("failed to extract servant for federation subscriber.\n")
    ));
    throw Incomplete();
  }

  switch( subscriberServant->attach_transport( transport.in())) {
    case OpenDDS::DCPS::ATTACH_OK:
         if( OpenDDS::DCPS::DCPS_debug_level > 4) {
           ACE_DEBUG((LM_DEBUG,
             ACE_TEXT("(%P|%t) INFO: Federator::ManagerImpl::initialize() - ")
             ACE_TEXT("attached transport to federation subscriber.\n")
           ));
         }
         break;

    case OpenDDS::DCPS::ATTACH_BAD_TRANSPORT:
    case OpenDDS::DCPS::ATTACH_ERROR:
    case OpenDDS::DCPS::ATTACH_INCOMPATIBLE_QOS:
    default:
         ACE_ERROR((LM_ERROR,
           ACE_TEXT("(%P|%t) INFO: Federator::ManagerImpl::initialize() - ")
           ACE_TEXT("failed to attach transport to federation subscriber.\n")
         ));
         throw Incomplete();
  }

  //
  // Create the publisher for the update topics.
  //

  ::DDS::Publisher_var publisher
    = this->federationParticipant_->create_publisher(
        PUBLISHER_QOS_DEFAULT,
        ::DDS::PublisherListener::_nil()
      );
  if( CORBA::is_nil( publisher.in())) {
    ACE_ERROR((LM_ERROR,
      ACE_TEXT("(%P|%t) ERROR: Federator::ManagerImpl::initialize() - ")
      ACE_TEXT("failed to create publisher for repository %d\n"),
      this->id()
    ));
    throw Incomplete();

  } else if( ::OpenDDS::DCPS::DCPS_debug_level > 4) {
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) INFO: Federator::ManagerImpl::initialize() - ")
      ACE_TEXT("created federation publisher for repository %d\n"),
      this->id()
    ));

  }

  // Attach the transport to it.
  ::OpenDDS::DCPS::PublisherImpl* publisherServant
    = dynamic_cast< ::OpenDDS::DCPS::PublisherImpl*>(
        publisher.in()
      );
  if( 0 == publisherServant) {
    ACE_ERROR((LM_ERROR,
      ACE_TEXT("(%P|%t) INFO: Federator::ManagerImpl::initialize() - ")
      ACE_TEXT("failed to extract servant for federation publisher.\n")
    ));
    throw Incomplete();
  }

  switch( publisherServant->attach_transport( transport.in())) {
    case OpenDDS::DCPS::ATTACH_OK:
         if( OpenDDS::DCPS::DCPS_debug_level > 4) {
           ACE_DEBUG((LM_DEBUG,
             ACE_TEXT("(%P|%t) INFO: Federator::ManagerImpl::initialize() - ")
             ACE_TEXT("attached transport to federation publisher.\n")
           ));
         }
         break;

    case OpenDDS::DCPS::ATTACH_BAD_TRANSPORT:
    case OpenDDS::DCPS::ATTACH_ERROR:
    case OpenDDS::DCPS::ATTACH_INCOMPATIBLE_QOS:
    default:
         ACE_ERROR((LM_ERROR,
           ACE_TEXT("(%P|%t) INFO: Federator::ManagerImpl::initialize() - ")
           ACE_TEXT("failed to attach transport to federation publisher.\n")
         ));
         throw Incomplete();
  }

  //
  // Some useful items for adding the subscriptions.
  //
  ::DDS::Topic_var            topic;
  ::DDS::TopicDescription_var description;
  ::DDS::DataReader_var       dataReader;
  ::DDS::DataWriter_var       dataWriter;

  ::DDS::DataReaderQos readerQos;
  readerQos.durability.kind                          = ::DDS::TRANSIENT_LOCAL_DURABILITY_QOS;
  readerQos.reliability.kind                         = ::DDS::RELIABLE_RELIABILITY_QOS;
  readerQos.reliability.max_blocking_time.sec        = 0;
  readerQos.reliability.max_blocking_time.nanosec    = 0;
  readerQos.history.kind                             = ::DDS::KEEP_ALL_HISTORY_QOS;
  readerQos.resource_limits.max_samples_per_instance = ::DDS::LENGTH_UNLIMITED;

  ::DDS::DataWriterQos writerQos;
  writerQos.durability.kind                          = ::DDS::TRANSIENT_LOCAL_DURABILITY_QOS;
  writerQos.reliability.kind                         = ::DDS::RELIABLE_RELIABILITY_QOS;
  writerQos.reliability.max_blocking_time.sec        = 0;
  writerQos.reliability.max_blocking_time.nanosec    = 0;
  writerQos.history.kind                             = ::DDS::KEEP_ALL_HISTORY_QOS;
  writerQos.resource_limits.max_samples_per_instance = ::DDS::LENGTH_UNLIMITED;

  //
  // Add update subscriptions
  //
  // NOTE: It ok to lose the references to the objects here since they
  //       are not needed after this point.  The only thing we will do
  //       with them is to destroy them, and that will be done via a
  //       cascade delete from the participant.  The listeners will
  //       survive and can be used within other participants as well,
  //       since the only state they retain is the manager, which is the
  //       same for all.
  //

  topic = this->federationParticipant_->create_topic(
            TOPICUPDATETOPICNAME,
            TOPICUPDATETYPENAME,
            TOPIC_QOS_DEFAULT,
            ::DDS::TopicListener::_nil()
          );
  dataWriter = publisher->create_datawriter(
                 topic.in(),
                 writerQos,
                 ::DDS::DataWriterListener::_nil()
               );
  if( CORBA::is_nil( dataWriter.in())) {
    ACE_ERROR((LM_ERROR,
      ACE_TEXT("(%P|%t) ERROR: Federator::ManagerImpl::initialize() - ")
      ACE_TEXT("failed to create Topic update writer for repository %d\n"),
      this->id()
    ));
    throw Incomplete();
  }

  this->topicWriter_
    = dynamic_cast< TopicUpdateDataWriter*>( dataWriter.in());
  if( 0 == this->topicWriter_) {
    ACE_ERROR((LM_ERROR,
      ACE_TEXT("(%P|%t) INFO: Federator::ManagerImpl::initialize() - ")
      ACE_TEXT("failed to extract typed Topic update writer.\n")
    ));
    throw Incomplete();

  } else if( ::OpenDDS::DCPS::DCPS_debug_level > 4) {
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) INFO: Federator::ManagerImpl::initialize() - ")
      ACE_TEXT("created federation Topic update writer for repository %d\n"),
      this->id()
    ));
  }

  description = this->federationParticipant_->lookup_topicdescription( TOPICUPDATETOPICNAME);
  dataReader  = subscriber->create_datareader(
                  description.in(),
                  readerQos,
                  &this->topicListener_
                );
  if( CORBA::is_nil( dataReader.in())) {
    ACE_ERROR((LM_ERROR,
      ACE_TEXT("(%P|%t) ERROR: Federator::ManagerImpl::initialize() - ")
      ACE_TEXT("failed to create Topic update reader for repository %d\n"),
      this->id()
    ));
    throw Incomplete();

  } else if( ::OpenDDS::DCPS::DCPS_debug_level > 4) {
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) INFO: Federator::ManagerImpl::initialize() - ")
      ACE_TEXT("created federation Topic update reader for repository %d\n"),
      this->id()
    ));
  }

  topic = this->federationParticipant_->create_topic(
            PARTICIPANTUPDATETOPICNAME,
            PARTICIPANTUPDATETYPENAME,
            TOPIC_QOS_DEFAULT,
            ::DDS::TopicListener::_nil()
          );
  dataWriter = publisher->create_datawriter(
                 topic.in(),
                 writerQos,
                 ::DDS::DataWriterListener::_nil()
               );
  if( CORBA::is_nil( dataWriter.in())) {
    ACE_ERROR((LM_ERROR,
      ACE_TEXT("(%P|%t) ERROR: Federator::ManagerImpl::initialize() - ")
      ACE_TEXT("failed to create Participant update writer for repository %d\n"),
      this->id()
    ));
    throw Incomplete();
  }

  this->participantWriter_
    = dynamic_cast< ParticipantUpdateDataWriter*>( dataWriter.in());
  if( 0 == this->participantWriter_) {
    ACE_ERROR((LM_ERROR,
      ACE_TEXT("(%P|%t) INFO: Federator::ManagerImpl::initialize() - ")
      ACE_TEXT("failed to extract typed Participant update writer.\n")
    ));
    throw Incomplete();

  } else if( ::OpenDDS::DCPS::DCPS_debug_level > 4) {
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) INFO: Federator::ManagerImpl::initialize() - ")
      ACE_TEXT("created federation Participant update writer for repository %d\n"),
      this->id()
    ));
  }

  description = this->federationParticipant_->lookup_topicdescription( PARTICIPANTUPDATETOPICNAME);
  dataReader  = subscriber->create_datareader(
                  description.in(),
                  readerQos,
                  &this->participantListener_
                );
  if( CORBA::is_nil( dataReader.in())) {
    ACE_ERROR((LM_ERROR,
      ACE_TEXT("(%P|%t) ERROR: Federator::ManagerImpl::initialize() - ")
      ACE_TEXT("failed to create Participant update reader for repository %d\n"),
      this->id()
    ));
    throw Incomplete();

  } else if( ::OpenDDS::DCPS::DCPS_debug_level > 4) {
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) INFO: Federator::ManagerImpl::initialize() - ")
      ACE_TEXT("created federation Participant update reader for repository %d\n"),
      this->id()
    ));
  }

  topic = this->federationParticipant_->create_topic(
            PUBLICATIONUPDATETOPICNAME,
            PUBLICATIONUPDATETYPENAME,
            TOPIC_QOS_DEFAULT,
            ::DDS::TopicListener::_nil()
          );
  dataWriter = publisher->create_datawriter(
                 topic.in(),
                 writerQos,
                 ::DDS::DataWriterListener::_nil()
               );
  if( CORBA::is_nil( dataWriter.in())) {
    ACE_ERROR((LM_ERROR,
      ACE_TEXT("(%P|%t) ERROR: Federator::ManagerImpl::initialize() - ")
      ACE_TEXT("failed to create Publication update writer for repository %d\n"),
      this->id()
    ));
    throw Incomplete();
  }

  this->publicationWriter_
    = dynamic_cast< PublicationUpdateDataWriter*>( dataWriter.in());
  if( 0 == this->publicationWriter_) {
    ACE_ERROR((LM_ERROR,
      ACE_TEXT("(%P|%t) INFO: Federator::ManagerImpl::initialize() - ")
      ACE_TEXT("failed to extract typed Publication update writer.\n")
    ));
    throw Incomplete();

  } else if( ::OpenDDS::DCPS::DCPS_debug_level > 4) {
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) INFO: Federator::ManagerImpl::initialize() - ")
      ACE_TEXT("created federation Publication update writer for repository %d\n"),
      this->id()
    ));
  }

  description = this->federationParticipant_->lookup_topicdescription( PUBLICATIONUPDATETOPICNAME);
  dataReader  = subscriber->create_datareader(
                  description.in(),
                  readerQos,
                  &this->publicationListener_
                );
  if( CORBA::is_nil( dataReader.in())) {
    ACE_ERROR((LM_ERROR,
      ACE_TEXT("(%P|%t) ERROR: Federator::ManagerImpl::initialize() - ")
      ACE_TEXT("failed to create Publication update reader for repository %d\n"),
      this->id()
    ));
    throw Incomplete();

  } else if( ::OpenDDS::DCPS::DCPS_debug_level > 4) {
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) INFO: Federator::ManagerImpl::initialize() - ")
      ACE_TEXT("created federation Publication update reader for repository %d\n"),
      this->id()
    ));
  }

  topic = this->federationParticipant_->create_topic(
            SUBSCRIPTIONUPDATETOPICNAME,
            SUBSCRIPTIONUPDATETYPENAME,
            TOPIC_QOS_DEFAULT,
            ::DDS::TopicListener::_nil()
          );
  dataWriter = publisher->create_datawriter(
                 topic.in(),
                 writerQos,
                 ::DDS::DataWriterListener::_nil()
               );
  if( CORBA::is_nil( dataWriter.in())) {
    ACE_ERROR((LM_ERROR,
      ACE_TEXT("(%P|%t) ERROR: Federator::ManagerImpl::initialize() - ")
      ACE_TEXT("failed to create Subscription update writer for repository %d\n"),
      this->id()
    ));
    throw Incomplete();
  }

  this->subscriptionWriter_
    = dynamic_cast< SubscriptionUpdateDataWriter*>( dataWriter.in());
  if( 0 == this->subscriptionWriter_) {
    ACE_ERROR((LM_ERROR,
      ACE_TEXT("(%P|%t) INFO: Federator::ManagerImpl::initialize() - ")
      ACE_TEXT("failed to extract typed Subscription update writer.\n")
    ));
    throw Incomplete();

  } else if( ::OpenDDS::DCPS::DCPS_debug_level > 4) {
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) INFO: Federator::ManagerImpl::initialize() - ")
      ACE_TEXT("created federation Subscription update writer for repository %d\n"),
      this->id()
    ));
  }

  description = this->federationParticipant_->lookup_topicdescription( SUBSCRIPTIONUPDATETOPICNAME);
  dataReader  = subscriber->create_datareader(
                  description.in(),
                  readerQos,
                  &this->subscriptionListener_
                );
  if( CORBA::is_nil( dataReader.in())) {
    ACE_ERROR((LM_ERROR,
      ACE_TEXT("(%P|%t) ERROR: Federator::ManagerImpl::initialize() - ")
      ACE_TEXT("failed to create Subscription update reader for repository %d\n"),
      this->id()
    ));
    throw Incomplete();

  } else if( ::OpenDDS::DCPS::DCPS_debug_level > 4) {
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) INFO: Federator::ManagerImpl::initialize() - ")
      ACE_TEXT("created federation Subscription update reader for repository %d\n"),
      this->id()
    ));
  }
}

// IDL methods.

RepoKey
ManagerImpl::federation_id( void)
ACE_THROW_SPEC (( ::CORBA::SystemException))
{
  if( ::OpenDDS::DCPS::DCPS_debug_level > 0) {
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) INFO: ManagerImpl::federation_id()\n")
    ));
  }
  return this->id();
}

::OpenDDS::DCPS::DCPSInfo_ptr
ManagerImpl::repository( void )
ACE_THROW_SPEC (( ::CORBA::SystemException))
{
  if( ::OpenDDS::DCPS::DCPS_debug_level > 0) {
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) INFO: ManagerImpl::repository()\n")
    ));
  }
  return TheServiceParticipant->get_repository( this->id());
}

::CORBA::Boolean
ManagerImpl::discover_federation ( const char * ior )
ACE_THROW_SPEC (( ::CORBA::SystemException, Incomplete))
{
  if( ::OpenDDS::DCPS::DCPS_debug_level > 0) {
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) INFO: ManagerImpl::discover_federation( %s)\n"),
      ior
    ));
  }
  ///@TODO: Implement this.
  return false;
}

::CORBA::Boolean
ManagerImpl::join_federation(
  Manager_ptr peer,
  FederationDomain federation

) ACE_THROW_SPEC (( ::CORBA::SystemException, Incomplete))
{
  if( ::OpenDDS::DCPS::DCPS_debug_level > 0) {
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) INFO: ManagerImpl::join_federation( peer, %x)\n"),
      federation
    ));
  }
  RepoKey remote = NIL_REPOSITORY;

  try {
    // Obtain the remote repository federator Id value.
    remote = peer->federation_id();
    if( ::OpenDDS::DCPS::DCPS_debug_level > 0) {
      ACE_DEBUG((LM_DEBUG,
        ACE_TEXT("(%P|%t) INFO: Federator::ManagerImpl::join_federation() - ")
        ACE_TEXT("repo id %d connecting to repository with id %d.\n"),
        this->id(),
        remote
      ));
    }

  } catch( const CORBA::Exception& ex) {
    ex._tao_print_exception(
      ACE_TEXT("ERROR: Federator::ManagerImpl::join_federation() - ")
      ACE_TEXT("unable to obtain remote federation Id value: ")
    );
    throw Incomplete();
  }

  // If we are recursing, then we are done.
  if( this->joiner_ == remote) {
    return true;

  } else {
    // Block while any different repository is joining.
    ACE_GUARD_RETURN( ACE_SYNCH_MUTEX, guard, this->lock_, false);
    while( this->joiner_ != NIL_REPOSITORY) {
      // This releases the lock while we block.
      this->joining_.wait();

      // We are now recursing - curses!
      if( this->joiner_ == remote) {
        return true;
      }
    }

    // Note that we are joining the remote repository now.
    this->joiner_ = remote;
  }

  //
  // We only reach this point if:
  //   1) No other repository is processing past this point;
  //   2) We are not recursing.
  //

  // Check if we already have Federation repository.
  ::OpenDDS::DCPS::DCPSInfo_var federationRepo
    = TheServiceParticipant->get_repository( this->config_.federationDomain());
  if( CORBA::is_nil(federationRepo.in())) {
    // Go ahead and add the joining repository as our Federation
    // repository.
    try {
      // Obtain a reference to the remote repository.
      ::OpenDDS::DCPS::DCPSInfo_var remoteRepo = peer->repository();
      if( ::OpenDDS::DCPS::DCPS_debug_level > 4) {
        CORBA::ORB_var orb = TheServiceParticipant->get_ORB();
        CORBA::String_var remoteRepoIor = orb->object_to_string( remoteRepo.in());
        ACE_DEBUG((LM_DEBUG,
          ACE_TEXT("(%P|%t) INFO: FederatorManagerImpl::join_federation() - ")
          ACE_TEXT("id %d obtained reference to id %d:\n")
          ACE_TEXT("\t%s\n"),
          this->id(),
          remote,
          remoteRepoIor.in()
        ));
      }

      // Add remote repository to Service_Participant in the Federation domain
      TheServiceParticipant->set_repo_domain( this->config_.federationDomain(), remote);
      TheServiceParticipant->set_repo( remoteRepo.in(), remote);

    } catch( const CORBA::Exception& ex) {
      ex._tao_print_exception(
        "ERROR: Federator::ManagerImpl::join_federation() - Unable to join with remote: "
      );
      throw Incomplete();
    }

    //
    // At this point, we are joining for the first time (previously we
    // did not have a federation domain repository) and so we establish
    // our publications and subscriptions here.
    //

    this->initialize();
  }

  // Symmetrical joining behavior.
  try {
    peer->join_federation( this->_this(), this->config_.federationDomain());

  } catch( const CORBA::Exception& ex) {
    ex._tao_print_exception(
      "ERROR: Federator::ManagerImpl::join_federation() - unsuccsessful call to remote->join: "
    );
    throw Incomplete();
  }

  this->joiner_ = NIL_REPOSITORY;
  this->joining_.signal();
  return true;
}

}} // End namespace OpenDDS::Federator

