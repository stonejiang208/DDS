// -*- C++ -*-
//
// $Id$

#include "DcpsInfo_pch.h"
#include "FederatorManagerImpl.h"
#include "DCPSInfo_i.h"
#include "DCPS_IR_Domain.h"
#include "DCPS_IR_Participant.h"

#include <sstream>

namespace OpenDDS { namespace Federator {

void
ManagerImpl::unregisterCallback()
{
  /* This method intentionally left unimplemented. */
}

void
ManagerImpl::requestImage()
{
  /* This method intentionally left unimplemented. */
}

////////////////////////////////////////////////////////////////////////
//
// The following methods publish updates to the remainder of the
// federation.
//

void
ManagerImpl::create( const Update::UTopic& topic)
{
  if( CORBA::is_nil( this->topicWriter_.in())) {
    // Decline to publish data until we can.
    return;
  }

  TopicUpdate sample;
  sample.sender      = this->id();
  sample.action      = CreateEntity;

  sample.id          = topic.topicId;
  sample.domain      = topic.domainId;
  sample.participant = topic.participantId;
  sample.topic       = topic.name.c_str();
  sample.datatype    = topic.dataType.c_str();
  sample.qos         = topic.topicQos;

  this->topicWriter_->write( sample, ::DDS::HANDLE_NIL);
}

void
ManagerImpl::create( const Update::UParticipant& participant)
{
  if( CORBA::is_nil( this->participantWriter_.in())) {
    // Decline to publish data until we can.
    return;
  }

  ParticipantUpdate sample;
  sample.sender = this->id();
  sample.action = CreateEntity;

  sample.domain = participant.domainId;
  sample.id     = participant.participantId;
  sample.qos    = participant.participantQos;

  this->participantWriter_->write( sample, ::DDS::HANDLE_NIL);
}

void
ManagerImpl::create( const Update::URActor& reader)
{
  if( CORBA::is_nil( this->subscriptionWriter_.in())) {
    // Decline to publish data until we can.
    return;
  }

  SubscriptionUpdate sample;
  sample.sender         = this->id();
  sample.action         = CreateEntity;

  sample.domain         = reader.domainId;
  sample.participant    = reader.participantId;
  sample.topic          = reader.topicId;
  sample.id             = reader.actorId;
  sample.callback       = reader.callback.c_str();
  sample.transport_id   = reader.transportInterfaceInfo.transport_id;
  sample.transport_blob = reader.transportInterfaceInfo.data;
  sample.datareader_qos = reader.drdwQos;
  sample.subscriber_qos = reader.pubsubQos;

  this->subscriptionWriter_->write( sample, ::DDS::HANDLE_NIL);
}

void
ManagerImpl::create( const Update::UWActor& writer)
{
  if( CORBA::is_nil( this->publicationWriter_.in())) {
    // Decline to publish data until we can.
    return;
  }

  PublicationUpdate sample;
  sample.sender         = this->id();
  sample.action         = CreateEntity;

  sample.domain         = writer.domainId;
  sample.participant    = writer.participantId;
  sample.topic          = writer.topicId;
  sample.id             = writer.actorId;
  sample.callback       = writer.callback.c_str();
  sample.transport_id   = writer.transportInterfaceInfo.transport_id;
  sample.transport_blob = writer.transportInterfaceInfo.data;
  sample.datawriter_qos = writer.drdwQos;
  sample.publisher_qos  = writer.pubsubQos;

  this->publicationWriter_->write( sample, ::DDS::HANDLE_NIL);
}

void
ManagerImpl::create( const Update::OwnershipData& data)
{
  if( CORBA::is_nil( this->ownerWriter_.in())) {
    // Decline to publish data until we can.
    return;
  }

  OwnerUpdate sample;
  sample.sender      = this->id();
  sample.action      = CreateEntity;

  sample.domain      = data.domain;
  sample.participant = data.participant;
  sample.owner       = data.owner;

  this->ownerWriter_->write( sample, ::DDS::HANDLE_NIL);
}

void
ManagerImpl::destroy(
  const Update::IdPath& id,
  Update::ItemType      type,
  Update::ActorType     actor
)
{
  switch( type) {
    case Update::Topic:
      {
        if( CORBA::is_nil( this->topicWriter_.in())) {
          // Decline to publish data until we can.
          return;
        }

        TopicUpdate sample;
        sample.sender      = this->id();
        sample.action      = DestroyEntity;

        sample.id          = id.id;
        sample.domain      = id.domain;
        sample.participant = id.participant;

        this->topicWriter_->write( sample, ::DDS::HANDLE_NIL);
      }
      break;

    case Update::Participant:
      {
        if( CORBA::is_nil( this->participantWriter_.in())) {
          // Decline to publish data until we can.
          return;
        }

        ParticipantUpdate sample;
        sample.sender = this->id();
        sample.action = DestroyEntity;

        sample.domain = id.domain;
        sample.id     = id.id;

        this->participantWriter_->write( sample, ::DDS::HANDLE_NIL);
      }
      break;

    case Update::Actor:
      // This is VERY annoying.
      switch( actor) {
        case Update::DataWriter:
          {
            if( CORBA::is_nil( this->publicationWriter_.in())) {
              // Decline to publish data until we can.
              return;
            }

            PublicationUpdate sample;
            sample.sender         = this->id();
            sample.action         = DestroyEntity;

            sample.domain         = id.domain;
            sample.participant    = id.participant;
            sample.id             = id.id;

            this->publicationWriter_->write( sample, ::DDS::HANDLE_NIL);
          }
          break;

        case Update::DataReader:
          {
            if( CORBA::is_nil( this->subscriptionWriter_.in())) {
              // Decline to publish data until we can.
              return;
            }

            SubscriptionUpdate sample;
            sample.sender         = this->id();
            sample.action         = DestroyEntity;

            sample.domain         = id.domain;
            sample.participant    = id.participant;
            sample.id             = id.id;

            this->subscriptionWriter_->write( sample, ::DDS::HANDLE_NIL);
          }
          break;
      }
      break;
  }
}

void
ManagerImpl::update( const Update::IdPath& id, const ::DDS::DomainParticipantQos& qos)
{
  if( CORBA::is_nil( this->participantWriter_.in())) {
    // Decline to publish data until we can.
    return;
  }

  ParticipantUpdate sample;
  sample.sender = this->id();
  sample.action = UpdateQosValue1;

  sample.domain = id.domain;
  sample.id     = id.id;
  sample.qos    = qos;

  this->participantWriter_->write( sample, ::DDS::HANDLE_NIL);
}

void
ManagerImpl::update( const Update::IdPath& id, const ::DDS::TopicQos& qos)
{
  if( CORBA::is_nil( this->topicWriter_.in())) {
    // Decline to publish data until we can.
    return;
  }

  TopicUpdate sample;
  sample.sender      = this->id();
  sample.action      = UpdateQosValue1;

  sample.id          = id.id;
  sample.domain      = id.domain;
  sample.participant = id.participant;
  sample.qos         = qos;

  this->topicWriter_->write( sample, ::DDS::HANDLE_NIL);
}

void
ManagerImpl::update( const Update::IdPath& id, const ::DDS::DataWriterQos& qos)
{
  if( CORBA::is_nil( this->publicationWriter_.in())) {
    // Decline to publish data until we can.
    return;
  }

  PublicationUpdate sample;
  sample.sender         = this->id();
  sample.action         = UpdateQosValue1;

  sample.domain         = id.domain;
  sample.participant    = id.participant;
  sample.id             = id.id;
  sample.datawriter_qos = qos;

  this->publicationWriter_->write( sample, ::DDS::HANDLE_NIL);
}

void
ManagerImpl::update( const Update::IdPath& id, const ::DDS::PublisherQos& qos)
{
  if( CORBA::is_nil( this->publicationWriter_.in())) {
    // Decline to publish data until we can.
    return;
  }

  PublicationUpdate sample;
  sample.sender         = this->id();
  sample.action         = UpdateQosValue2;

  sample.domain         = id.domain;
  sample.participant    = id.participant;
  sample.id             = id.id;
  sample.publisher_qos  = qos;

  this->publicationWriter_->write( sample, ::DDS::HANDLE_NIL);
}

void
ManagerImpl::update( const Update::IdPath& id, const ::DDS::DataReaderQos& qos)
{
  if( CORBA::is_nil( this->subscriptionWriter_.in())) {
    // Decline to publish data until we can.
    return;
  }

  SubscriptionUpdate sample;
  sample.sender         = this->id();
  sample.action         = UpdateQosValue1;

  sample.domain         = id.domain;
  sample.participant    = id.participant;
  sample.id             = id.id;
  sample.datareader_qos = qos;

  this->subscriptionWriter_->write( sample, ::DDS::HANDLE_NIL);
}

void
ManagerImpl::update( const Update::IdPath& id, const ::DDS::SubscriberQos& qos)
{
  if( CORBA::is_nil( this->subscriptionWriter_.in())) {
    // Decline to publish data until we can.
    return;
  }

  SubscriptionUpdate sample;
  sample.sender         = this->id();
  sample.action         = UpdateQosValue2;

  sample.domain         = id.domain;
  sample.participant    = id.participant;
  sample.id             = id.id;
  sample.subscriber_qos = qos;

  this->subscriptionWriter_->write( sample, ::DDS::HANDLE_NIL);
}

////////////////////////////////////////////////////////////////////////
//
// The following methods process updates received from the remainder
// of the federation.
//

void
ManagerImpl::processCreate( const OwnerUpdate* sample, const ::DDS::SampleInfo* /* info */)
{
  if( ::OpenDDS::DCPS::DCPS_debug_level > 9) {
    std::stringstream buffer;
    long key = ::OpenDDS::DCPS::GuidConverter(
                 const_cast< ::OpenDDS::DCPS::RepoId*>( &sample->participant)
               );
    buffer << sample->participant << "(" << std::hex << key << ")";
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) INFO: Federator::ManagerImpl::processCreate( OwnerUpdate): ")
      ACE_TEXT("repo %d - [ domain %d/ participant %s/ sender %d/ owner %d ]\n"),
      this->id(),
      sample->domain,
      buffer.str().c_str(),
      sample->sender,
      sample->owner
    ));
  }

  // We could generate an error message here.  Instead we let action be irrelevant.
  this->info_->changeOwnership(
    sample->domain,
    sample->participant,
    sample->sender,
    sample->owner
  );
}

void
ManagerImpl::processCreate( const PublicationUpdate* sample, const ::DDS::SampleInfo* /* info */)
{
  if( ::OpenDDS::DCPS::DCPS_debug_level > 9) {
    std::stringstream participantBuffer;
    std::stringstream buffer;
    long key = ::OpenDDS::DCPS::GuidConverter(
                 const_cast< ::OpenDDS::DCPS::RepoId*>( &sample->participant)
               );
    participantBuffer << sample->participant << "(" << std::hex << key << ")";
    key = ::OpenDDS::DCPS::GuidConverter(
            const_cast< ::OpenDDS::DCPS::RepoId*>( &sample->id)
          );
    buffer << sample->id << "(" << std::hex << key << ")";
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) INFO: Federator::ManagerImpl::processCreate( PublicationUpdate): ")
      ACE_TEXT("repo %d - [ domain %d/ participant %s/ publication %s ]\n"),
      this->id(),
      sample->domain,
      participantBuffer.str().c_str(),
      buffer.str().c_str()
    ));
  }

  ::OpenDDS::DCPS::TransportInterfaceInfo transportInfo;
  transportInfo.transport_id = sample->transport_id;
  transportInfo.data         = sample->transport_blob;

  this->info_->add_publication(
    sample->domain,
    sample->participant,
    sample->topic,
    sample->id,
    sample->callback,
    sample->datawriter_qos,
    transportInfo,
    sample->publisher_qos
  );
}

void
ManagerImpl::processCreate( const SubscriptionUpdate* sample, const ::DDS::SampleInfo* /* info */)
{
  if( ::OpenDDS::DCPS::DCPS_debug_level > 9) {
    std::stringstream participantBuffer;
    std::stringstream buffer;
    long key = ::OpenDDS::DCPS::GuidConverter(
                 const_cast< ::OpenDDS::DCPS::RepoId*>( &sample->participant)
               );
    participantBuffer << sample->participant << "(" << std::hex << key << ")";
    key = ::OpenDDS::DCPS::GuidConverter(
            const_cast< ::OpenDDS::DCPS::RepoId*>( &sample->id)
          );
    buffer << sample->id << "(" << std::hex << key << ")";
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) INFO: Federator::ManagerImpl::processCreate( SubscriptionUpdate): ")
      ACE_TEXT("repo %d - [ domain %d/ participant %s/ publication %s ]\n"),
      this->id(),
      sample->domain,
      participantBuffer.str().c_str(),
      buffer.str().c_str()
    ));
  }

  ::OpenDDS::DCPS::TransportInterfaceInfo transportInfo;
  transportInfo.transport_id = sample->transport_id;
  transportInfo.data         = sample->transport_blob;

  this->info_->add_subscription(
    sample->domain,
    sample->participant,
    sample->topic,
    sample->id,
    sample->callback,
    sample->datareader_qos,
    transportInfo,
    sample->subscriber_qos
  );
}

void
ManagerImpl::processCreate( const ParticipantUpdate* sample, const ::DDS::SampleInfo* /* info */)
{
  if( ::OpenDDS::DCPS::DCPS_debug_level > 9) {
    std::stringstream buffer;
    long key = ::OpenDDS::DCPS::GuidConverter(
                 const_cast< ::OpenDDS::DCPS::RepoId*>( &sample->id)
               );
    buffer << sample->id << "(" << std::hex << key << ")";
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) INFO: Federator::ManagerImpl::processCreate( ParticipantUpdate): ")
      ACE_TEXT("repo %d - [ domain %d/ participant %s ]\n"),
      this->id(),
      sample->domain,
      buffer.str().c_str()
    ));
  }

  this->info_->add_domain_participant(
    sample->domain,
    sample->id,
    sample->qos
  );
}

void
ManagerImpl::processCreate( const TopicUpdate* sample, const ::DDS::SampleInfo* /* info */)
{
  if( ::OpenDDS::DCPS::DCPS_debug_level > 9) {
    std::stringstream participantBuffer;
    std::stringstream buffer;
    long key = ::OpenDDS::DCPS::GuidConverter(
                 const_cast< ::OpenDDS::DCPS::RepoId*>( &sample->participant)
               );
    participantBuffer << sample->participant << "(" << std::hex << key << ")";
    key = ::OpenDDS::DCPS::GuidConverter(
            const_cast< ::OpenDDS::DCPS::RepoId*>( &sample->id)
          );
    buffer << sample->id << "(" << std::hex << key << ")";
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) INFO: Federator::ManagerImpl::processCreate( TopicUpdate): ")
      ACE_TEXT("repo %d - [ domain %d/ participant %s/ topic %s ]\n"),
      this->id(),
      sample->domain,
      participantBuffer.str().c_str(),
      buffer.str().c_str()
    ));
  }

  this->info_->add_topic(
    sample->id,
    sample->domain,
    sample->participant,
    sample->topic,
    sample->datatype,
    sample->qos
  );
}

void
ManagerImpl::processUpdateQos1( const OwnerUpdate* sample, const ::DDS::SampleInfo* /* info */)
{
  if( ::OpenDDS::DCPS::DCPS_debug_level > 9) {
    std::stringstream buffer;
    long key = ::OpenDDS::DCPS::GuidConverter(
                 const_cast< ::OpenDDS::DCPS::RepoId*>( &sample->participant)
               );
    buffer << sample->participant << "(" << std::hex << key << ")";
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) INFO: Federator::ManagerImpl::processUpdateQos1( OwnerUpdate): ")
      ACE_TEXT("repo %d - [ domain %d/ participant %s/ sender %d/ owner %d ]\n"),
      this->id(),
      sample->domain,
      buffer.str().c_str(),
      sample->sender,
      sample->owner
    ));
  }

  this->info_->changeOwnership(
    sample->domain,
    sample->participant,
    sample->sender,
    sample->owner
  );
}

void
ManagerImpl::processUpdateQos1( const PublicationUpdate* sample, const ::DDS::SampleInfo* /* info */)
{
  if( ::OpenDDS::DCPS::DCPS_debug_level > 9) {
    std::stringstream participantBuffer;
    std::stringstream buffer;
    long key = ::OpenDDS::DCPS::GuidConverter(
                 const_cast< ::OpenDDS::DCPS::RepoId*>( &sample->participant)
               );
    participantBuffer << sample->participant << "(" << std::hex << key << ")";
    key = ::OpenDDS::DCPS::GuidConverter(
            const_cast< ::OpenDDS::DCPS::RepoId*>( &sample->id)
          );
    buffer << sample->id << "(" << std::hex << key << ")";
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) INFO: Federator::ManagerImpl::processUpdateQos1( PublicationUpdate): ")
      ACE_TEXT("repo %d - [ domain %d/ participant %s/ publication %s ]\n"),
      this->id(),
      sample->domain,
      participantBuffer.str().c_str(),
      buffer.str().c_str()
    ));
  }

  this->info_->update_publication_qos(
    sample->domain,
    sample->participant,
    sample->id,
    sample->datawriter_qos
  );
}

void
ManagerImpl::processUpdateQos2( const PublicationUpdate* sample, const ::DDS::SampleInfo* /* info */)
{
  if( ::OpenDDS::DCPS::DCPS_debug_level > 9) {
    std::stringstream participantBuffer;
    std::stringstream buffer;
    long key = ::OpenDDS::DCPS::GuidConverter(
                 const_cast< ::OpenDDS::DCPS::RepoId*>( &sample->participant)
               );
    participantBuffer << sample->participant << "(" << std::hex << key << ")";
    key = ::OpenDDS::DCPS::GuidConverter(
            const_cast< ::OpenDDS::DCPS::RepoId*>( &sample->id)
          );
    buffer << sample->id << "(" << std::hex << key << ")";
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) INFO: Federator::ManagerImpl::processUpdateQos2( PublicationUpdate): ")
      ACE_TEXT("repo %d - [ domain %d/ participant %s/ publication %s ]\n"),
      this->id(),
      sample->domain,
      participantBuffer.str().c_str(),
      buffer.str().c_str()
    ));
  }

  this->info_->update_publication_qos(
    sample->domain,
    sample->participant,
    sample->id,
    sample->publisher_qos
  );
}

void
ManagerImpl::processUpdateQos1( const SubscriptionUpdate* sample, const ::DDS::SampleInfo* /* info */)
{
  if( ::OpenDDS::DCPS::DCPS_debug_level > 9) {
    std::stringstream participantBuffer;
    std::stringstream buffer;
    long key = ::OpenDDS::DCPS::GuidConverter(
                 const_cast< ::OpenDDS::DCPS::RepoId*>( &sample->participant)
               );
    participantBuffer << sample->participant << "(" << std::hex << key << ")";
    key = ::OpenDDS::DCPS::GuidConverter(
            const_cast< ::OpenDDS::DCPS::RepoId*>( &sample->id)
          );
    buffer << sample->id << "(" << std::hex << key << ")";
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) INFO: Federator::ManagerImpl::processUpdateQos1( SubscriptionUpdate): ")
      ACE_TEXT("repo %d - [ domain %d/ participant %s/ subscription %s ]\n"),
      this->id(),
      sample->domain,
      participantBuffer.str().c_str(),
      buffer.str().c_str()
    ));
  }

  this->info_->update_subscription_qos(
    sample->domain,
    sample->participant,
    sample->id,
    sample->datareader_qos
  );
}

void
ManagerImpl::processUpdateQos2( const SubscriptionUpdate* sample, const ::DDS::SampleInfo* /* info */)
{
  if( ::OpenDDS::DCPS::DCPS_debug_level > 9) {
    std::stringstream participantBuffer;
    std::stringstream buffer;
    long key = ::OpenDDS::DCPS::GuidConverter(
                 const_cast< ::OpenDDS::DCPS::RepoId*>( &sample->participant)
               );
    participantBuffer << sample->participant << "(" << std::hex << key << ")";
    key = ::OpenDDS::DCPS::GuidConverter(
            const_cast< ::OpenDDS::DCPS::RepoId*>( &sample->id)
          );
    buffer << sample->id << "(" << std::hex << key << ")";
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) INFO: Federator::ManagerImpl::processUpdateQos2( SubscriptionUpdate): ")
      ACE_TEXT("repo %d - [ domain %d/ participant %s/ subscription %s ]\n"),
      this->id(),
      sample->domain,
      participantBuffer.str().c_str(),
      buffer.str().c_str()
    ));
  }

  this->info_->update_subscription_qos(
    sample->domain,
    sample->participant,
    sample->id,
    sample->subscriber_qos
  );
}

void
ManagerImpl::processUpdateQos1( const ParticipantUpdate* sample, const ::DDS::SampleInfo* /* info */)
{
  if( ::OpenDDS::DCPS::DCPS_debug_level > 9) {
    std::stringstream buffer;
    long key = ::OpenDDS::DCPS::GuidConverter(
                 const_cast< ::OpenDDS::DCPS::RepoId*>( &sample->id)
               );
    buffer << sample->id << "(" << std::hex << key << ")";
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) INFO: Federator::ManagerImpl::processUpdateQos1( ParticipantUpdate): ")
      ACE_TEXT("repo %d - [ domain %d/ participant %s ]\n"),
      this->id(),
      sample->domain,
      buffer.str().c_str()
    ));
  }

  this->info_->update_domain_participant_qos(
    sample->domain,
    sample->id,
    sample->qos
  );
}

void
ManagerImpl::processUpdateQos1( const TopicUpdate* sample, const ::DDS::SampleInfo* /* info */)
{
  if( ::OpenDDS::DCPS::DCPS_debug_level > 9) {
    std::stringstream participantBuffer;
    std::stringstream buffer;
    long key = ::OpenDDS::DCPS::GuidConverter(
                 const_cast< ::OpenDDS::DCPS::RepoId*>( &sample->participant)
               );
    participantBuffer << sample->participant << "(" << std::hex << key << ")";
    key = ::OpenDDS::DCPS::GuidConverter(
            const_cast< ::OpenDDS::DCPS::RepoId*>( &sample->id)
          );
    buffer << sample->id << "(" << std::hex << key << ")";
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) INFO: Federator::ManagerImpl::processUpdateQos1( TopicUpdate): ")
      ACE_TEXT("repo %d - [ domain %d/ participant %s/ topic %s ]\n"),
      this->id(),
      sample->domain,
      participantBuffer.str().c_str(),
      buffer.str().c_str()
    ));
  }

  this->info_->update_topic_qos(
    sample->id,
    sample->domain,
    sample->participant,
    sample->qos
  );
}

void
ManagerImpl::processDelete( const OwnerUpdate* sample, const ::DDS::SampleInfo* /* info */)
{
  if( ::OpenDDS::DCPS::DCPS_debug_level > 9) {
    std::stringstream buffer;
    long key = ::OpenDDS::DCPS::GuidConverter(
                 const_cast< ::OpenDDS::DCPS::RepoId*>( &sample->participant)
               );
    buffer << sample->participant << "(" << std::hex << key << ")";
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) INFO: Federator::ManagerImpl::processDelete( OwnerUpdate): ")
      ACE_TEXT("repo %d - [ domain %d/ participant %s/ sender %d/ owner %d ]\n"),
      this->id(),
      sample->domain,
      buffer.str().c_str(),
      sample->sender,
      sample->owner
    ));
  }

  // We could generate an error message here.  Instead we let action be irrelevant.
  this->info_->changeOwnership(
    sample->domain,
    sample->participant,
    sample->sender,
    sample->owner
  );
}

void
ManagerImpl::processDelete( const PublicationUpdate* sample, const ::DDS::SampleInfo* /* info */)
{
  if( ::OpenDDS::DCPS::DCPS_debug_level > 9) {
    std::stringstream participantBuffer;
    std::stringstream buffer;
    long key = ::OpenDDS::DCPS::GuidConverter(
                 const_cast< ::OpenDDS::DCPS::RepoId*>( &sample->participant)
               );
    participantBuffer << sample->participant << "(" << std::hex << key << ")";
    key = ::OpenDDS::DCPS::GuidConverter(
            const_cast< ::OpenDDS::DCPS::RepoId*>( &sample->id)
          );
    buffer << sample->id << "(" << std::hex << key << ")";
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) INFO: Federator::ManagerImpl::processDelete( PublicationUpdate): ")
      ACE_TEXT("repo %d - [ domain %d/ participant %s/ publication %s ]\n"),
      this->id(),
      sample->domain,
      participantBuffer.str().c_str(),
      buffer.str().c_str()
    ));
  }

  this->info_->remove_publication(
    sample->domain,
    sample->participant,
    sample->id
  );
}

void
ManagerImpl::processDelete( const SubscriptionUpdate* sample, const ::DDS::SampleInfo* /* info */)
{
  if( ::OpenDDS::DCPS::DCPS_debug_level > 9) {
    std::stringstream participantBuffer;
    std::stringstream buffer;
    long key = ::OpenDDS::DCPS::GuidConverter(
                 const_cast< ::OpenDDS::DCPS::RepoId*>( &sample->participant)
               );
    participantBuffer << sample->participant << "(" << std::hex << key << ")";
    key = ::OpenDDS::DCPS::GuidConverter(
            const_cast< ::OpenDDS::DCPS::RepoId*>( &sample->id)
          );
    buffer << sample->id << "(" << std::hex << key << ")";
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) INFO: Federator::ManagerImpl::processDelete( SubscriptionUpdate): ")
      ACE_TEXT("repo %d - [ domain %d/ participant %s/ subscription %s ]\n"),
      this->id(),
      sample->domain,
      participantBuffer.str().c_str(),
      buffer.str().c_str()
    ));
  }

  this->info_->remove_subscription(
    sample->domain,
    sample->participant,
    sample->id
  );
}

void
ManagerImpl::processDelete( const ParticipantUpdate* sample, const ::DDS::SampleInfo* /* info */)
{
  if( ::OpenDDS::DCPS::DCPS_debug_level > 9) {
    std::stringstream buffer;
    long key = ::OpenDDS::DCPS::GuidConverter(
                 const_cast< ::OpenDDS::DCPS::RepoId*>( &sample->id)
               );
    buffer << sample->id << "(" << std::hex << key << ")";
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) INFO: Federator::ManagerImpl::processDelete( ParticipantUpdate): ")
      ACE_TEXT("repo %d - [ domain %d/ participant %s ]\n"),
      this->id(),
      sample->domain,
      buffer.str().c_str()
    ));
  }

  this->info_->remove_domain_participant(
    sample->domain,
    sample->id
  );
}

void
ManagerImpl::processDelete( const TopicUpdate* sample, const ::DDS::SampleInfo* /* info */)
{
  if( ::OpenDDS::DCPS::DCPS_debug_level > 9) {
    std::stringstream participantBuffer;
    std::stringstream buffer;
    long key = ::OpenDDS::DCPS::GuidConverter(
                 const_cast< ::OpenDDS::DCPS::RepoId*>( &sample->participant)
               );
    participantBuffer << sample->participant << "(" << std::hex << key << ")";
    key = ::OpenDDS::DCPS::GuidConverter(
            const_cast< ::OpenDDS::DCPS::RepoId*>( &sample->id)
          );
    buffer << sample->id << "(" << std::hex << key << ")";
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) INFO: Federator::ManagerImpl::processDelete( TopicUpdate): ")
      ACE_TEXT("repo %d - [ domain %d/ participant %s/ topic %s ]\n"),
      this->id(),
      sample->domain,
      participantBuffer.str().c_str(),
      buffer.str().c_str()
    ));
  }

  this->info_->remove_topic(
    sample->domain,
    sample->participant,
    sample->id
  );
}

void
ManagerImpl::pushState( Manager_ptr peer)
{
  // foreach DCPS_IR_Domain
  //   foreach DCPS_IR_Participant
  //     peer->initializeParticipant(...)
  //     peer->initializeOwner(...)
  //     foreach DCPS_IR_Topic
  //       peer->initializeTopic(...)
  //     foreach DCPS_IR_Publication
  //       peer->initializePublication(...)
  //     foreach DCPS_IR_Subscription
  //       peer->initializeSubscription(...)

  // Process each domain within the repository.
  for( DCPS_IR_Domain_Map::const_iterator currentDomain
         = this->info_->domains().begin();
       currentDomain != this->info_->domains().end();
       ++currentDomain) {

    if( currentDomain->second->get_id() == this->config_.federationDomain()) {
      // Do not push the Federation domain publications.
      //continue;
    }

    // Process each participant within the current domain.
    for( DCPS_IR_Participant_Map::const_iterator currentParticipant
           = currentDomain->second->participants().begin();
         currentParticipant != currentDomain->second->participants().end();
         ++currentParticipant) {

      if( currentParticipant->second->isBitPublisher() == true) {
        // Do not push the built-in topic publications.
        continue;
      }

      // Initialize the participant on the peer.
      ParticipantUpdate participantSample;
      participantSample.sender = this->id();
      participantSample.action = CreateEntity;

      participantSample.domain =  currentDomain->second->get_id();
      participantSample.id     =  currentParticipant->second->get_id();
      participantSample.qos    = *currentParticipant->second->get_qos();

      peer->initializeParticipant( participantSample);

      // Initialize the ownership of the participant on the peer.
      OwnerUpdate ownerSample;
      ownerSample.sender      = this->id();
      ownerSample.action      = CreateEntity;

      ownerSample.domain      = currentDomain->second->get_id();
      ownerSample.participant = currentParticipant->second->get_id();
      ownerSample.owner       = currentParticipant->second->owner();

      peer->initializeOwner( ownerSample);

      // Process each topic within the current particpant.
      for( DCPS_IR_Topic_Map::const_iterator currentTopic
             = currentParticipant->second->topics().begin();
           currentTopic != currentParticipant->second->topics().end();
           ++currentTopic) {
        TopicUpdate topicSample;
        topicSample.sender      = this->id();
        topicSample.action      = CreateEntity;

        topicSample.id          = currentTopic->second->get_id();
        topicSample.domain      = currentDomain->second->get_id();
        topicSample.participant = currentTopic->second->get_participant_id();
        topicSample.topic       = currentTopic->second->get_topic_description()->get_name();
        topicSample.datatype    = currentTopic->second->get_topic_description()->get_dataTypeName();
        topicSample.qos         = *currentTopic->second->get_topic_qos();

        peer->initializeTopic( topicSample);
      }

      // Process each publication within the current particpant.
      for( DCPS_IR_Publication_Map::const_iterator currentPublication
             = currentParticipant->second->publications().begin();
           currentPublication != currentParticipant->second->publications().end();
           ++currentPublication) {
        PublicationUpdate publicationSample;
        publicationSample.sender         = this->id();
        publicationSample.action         = CreateEntity;

        DCPS_IR_Publication* p = currentPublication->second;
        CORBA::ORB_var orb = this->info_->orb();
        CORBA::String_var callback = orb->object_to_string( p->writer());

        publicationSample.domain         = currentDomain->second->get_id();
        publicationSample.participant    = p->get_participant_id();
        publicationSample.topic          = p->get_topic_id();
        publicationSample.id             = p->get_id();
        publicationSample.callback       = callback.in();
        publicationSample.transport_id   = p->get_transportInterfaceInfo().transport_id;
        publicationSample.transport_blob = p->get_transportInterfaceInfo().data;
        publicationSample.datawriter_qos = *p->get_datawriter_qos();
        publicationSample.publisher_qos  = *p->get_publisher_qos();

        peer->initializePublication( publicationSample);
      }

      // Process each subscription within the current particpant.
      for( DCPS_IR_Subscription_Map::const_iterator currentSubscription
             = currentParticipant->second->subscriptions().begin();
           currentSubscription != currentParticipant->second->subscriptions().end();
           ++currentSubscription) {
        SubscriptionUpdate subscriptionSample;
        subscriptionSample.sender         = this->id();
        subscriptionSample.action         = CreateEntity;

        DCPS_IR_Subscription* s = currentSubscription->second;
        CORBA::ORB_var orb = this->info_->orb();
        CORBA::String_var callback = orb->object_to_string( s->reader());

        subscriptionSample.domain         = currentDomain->second->get_id();
        subscriptionSample.participant    = s->get_participant_id();
        subscriptionSample.topic          = s->get_topic_id();
        subscriptionSample.id             = s->get_id();
        subscriptionSample.callback       = callback.in();
        subscriptionSample.transport_id   = s->get_transportInterfaceInfo().transport_id;
        subscriptionSample.transport_blob = s->get_transportInterfaceInfo().data;
        subscriptionSample.datareader_qos = *s->get_datareader_qos();
        subscriptionSample.subscriber_qos = *s->get_subscriber_qos();

        peer->initializeSubscription( subscriptionSample);
      }
    }
  }
}

}} // End namespace OpenDDS::Federator

