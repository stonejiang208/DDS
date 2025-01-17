/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "DCPS/DdsDcps_pch.h" //Only the _pch include should start with DCPS/
#include "DomainParticipantImpl.h"
#include "FeatureDisabledQosCheck.h"
#include "Service_Participant.h"
#include "Qos_Helper.h"
#include "GuidConverter.h"
#include "PublisherImpl.h"
#include "SubscriberImpl.h"
#include "Marked_Default_Qos.h"
#include "Registered_Data_Types.h"
#include "Transient_Kludge.h"
#include "DomainParticipantFactoryImpl.h"
#include "Util.h"
#include "MonitorFactory.h"
#include "dds/DdsDcpsGuidC.h"
#include "BitPubListenerImpl.h"
#include "ContentFilteredTopicImpl.h"
#include "MultiTopicImpl.h"
#include "dds/DCPS/transport/framework/TransportRegistry.h"
#include "dds/DCPS/transport/framework/TransportExceptions.h"

#include "RecorderImpl.h"
#include "ReplayerImpl.h"
#include <sstream>

#if !defined (DDS_HAS_MINIMUM_BIT)
#include "BuiltInTopicUtils.h"
#include "dds/DdsDcpsInfrastructureTypeSupportImpl.h"
#endif // !defined (DDS_HAS_MINIMUM_BIT)

#include "tao/debug.h"

namespace Util {

template <typename Key>
int find(
  OpenDDS::DCPS::DomainParticipantImpl::TopicMap& c,
  const Key& key,
  OpenDDS::DCPS::DomainParticipantImpl::TopicMap::mapped_type*& value)
{
  OpenDDS::DCPS::DomainParticipantImpl::TopicMap::iterator iter =
    c.find(key);

  if (iter == c.end()) {
    return -1;
  }

  value = &iter->second;
  return 0;
}

} // namespace Util

namespace OpenDDS {
namespace DCPS {

//TBD - add check for enabled in most methods.
//      Currently this is not needed because auto_enable_created_entities
//      cannot be false.

// Implementation skeleton constructor
DomainParticipantImpl::DomainParticipantImpl(DomainParticipantFactoryImpl *       factory,
                                             const DDS::DomainId_t&             domain_id,
                                             const RepoId&                        dp_id,
                                             const DDS::DomainParticipantQos &  qos,
                                             DDS::DomainParticipantListener_ptr a_listener,
                                             const DDS::StatusMask &            mask,
                                             bool                                 federated)
  : factory_(factory),
    default_topic_qos_(TheServiceParticipant->initial_TopicQos()),
    default_publisher_qos_(TheServiceParticipant->initial_PublisherQos()),
    default_subscriber_qos_(TheServiceParticipant->initial_SubscriberQos()),
    qos_(qos),
    domain_id_(domain_id),
    dp_id_(dp_id),
    federated_(federated),
    monitor_(0),
    pub_id_gen_(dp_id_)
{
  (void) this->set_listener(a_listener, mask);
  monitor_ = TheServiceParticipant->monitor_factory_->create_dp_monitor(this);
}

// Implementation skeleton destructor
DomainParticipantImpl::~DomainParticipantImpl()
{
}

DDS::Publisher_ptr
DomainParticipantImpl::create_publisher(
  const DDS::PublisherQos & qos,
  DDS::PublisherListener_ptr a_listener,
  DDS::StatusMask mask)
{
  ACE_UNUSED_ARG(mask);

  DDS::PublisherQos pub_qos = qos;

  if (! this->validate_publisher_qos(pub_qos))
    return DDS::Publisher::_nil();

  PublisherImpl* pub = 0;
  ACE_NEW_RETURN(pub,
                 PublisherImpl(participant_handles_.next(),
                               pub_id_gen_.next(),
                               pub_qos,
                               a_listener,
                               mask,
                               this),
                 DDS::Publisher::_nil());

  if ((enabled_ == true) && (qos_.entity_factory.autoenable_created_entities == 1)) {
    pub->enable();
  }

  DDS::Publisher_ptr pub_obj(pub);

  // this object will also act as the guard for leaking Publisher Impl
  Publisher_Pair pair(pub, pub_obj, NO_DUP);

  ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex,
                   tao_mon,
                   this->publishers_protector_,
                   DDS::Publisher::_nil());

  if (OpenDDS::DCPS::insert(publishers_, pair) == -1) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("(%P|%t) ERROR: DomainParticipantImpl::create_publisher, ")
               ACE_TEXT("%p\n"),
               ACE_TEXT("insert")));
    return DDS::Publisher::_nil();
  }

  return DDS::Publisher::_duplicate(pub_obj);
}

DDS::ReturnCode_t
DomainParticipantImpl::delete_publisher(
  DDS::Publisher_ptr p)
{
  // The servant's ref count should be 2 at this point,
  // one referenced by poa, one referenced by the subscriber
  // set.
  PublisherImpl* the_servant = dynamic_cast<PublisherImpl*>(p);

  if (the_servant->is_clean() == 0) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("(%P|%t) ERROR: ")
               ACE_TEXT("DomainParticipantImpl::delete_publisher, ")
               ACE_TEXT("The publisher is not empty.\n")));
    return DDS::RETCODE_PRECONDITION_NOT_MET;
  }

  ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex,
                   tao_mon,
                   this->publishers_protector_,
                   DDS::RETCODE_ERROR);

  Publisher_Pair pair(the_servant, p, DUP);

  if (OpenDDS::DCPS::remove(publishers_, pair) == -1) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("(%P|%t) ERROR: DomainParticipantImpl::delete_publisher, ")
               ACE_TEXT("%p\n"),
               ACE_TEXT("remove")));
    return DDS::RETCODE_ERROR;

  } else {
    return DDS::RETCODE_OK;
  }
}

DDS::Subscriber_ptr
DomainParticipantImpl::create_subscriber(
  const DDS::SubscriberQos & qos,
  DDS::SubscriberListener_ptr a_listener,
  DDS::StatusMask mask)
{
  DDS::SubscriberQos sub_qos = qos;

  if (! this->validate_subscriber_qos(sub_qos)) {
    return DDS::Subscriber::_nil();
  }

  SubscriberImpl* sub = 0 ;
  ACE_NEW_RETURN(sub,
                 SubscriberImpl(participant_handles_.next(),
                                sub_qos,
                                a_listener,
                                mask,
                                this),
                 DDS::Subscriber::_nil());

  if ((enabled_ == true) && (qos_.entity_factory.autoenable_created_entities == 1)) {
    sub->enable();
  }

  DDS::Subscriber_ptr sub_obj(sub);

  Subscriber_Pair pair(sub, sub_obj, NO_DUP);

  ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex,
                   tao_mon,
                   this->subscribers_protector_,
                   DDS::Subscriber::_nil());

  if (OpenDDS::DCPS::insert(subscribers_, pair) == -1) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("(%P|%t) ERROR: DomainParticipantImpl::create_subscriber, ")
               ACE_TEXT("%p\n"),
               ACE_TEXT("insert")));
    return DDS::Subscriber::_nil();
  }

  return DDS::Subscriber::_duplicate(sub_obj);
}

DDS::ReturnCode_t
DomainParticipantImpl::delete_subscriber(
  DDS::Subscriber_ptr s)
{
  // The servant's ref count should be 2 at this point,
  // one referenced by poa, one referenced by the subscriber
  // set.
  SubscriberImpl* the_servant = dynamic_cast<SubscriberImpl*>(s);

  if (the_servant->is_clean() == 0) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("(%P|%t) ERROR: ")
               ACE_TEXT("DomainParticipantImpl::delete_subscriber, ")
               ACE_TEXT("The subscriber is not empty.\n")));
    return DDS::RETCODE_PRECONDITION_NOT_MET;
  }

  DDS::ReturnCode_t ret
  = the_servant->delete_contained_entities();

  if (ret != DDS::RETCODE_OK) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("(%P|%t) ERROR: ")
               ACE_TEXT("DomainParticipantImpl::delete_subscriber, ")
               ACE_TEXT("Failed to delete contained entities.\n")));
    return DDS::RETCODE_ERROR;
  }

  ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex,
                   tao_mon,
                   this->subscribers_protector_,
                   DDS::RETCODE_ERROR);

  Subscriber_Pair pair(the_servant, s, DUP);

  if (OpenDDS::DCPS::remove(subscribers_, pair) == -1) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("(%P|%t) ERROR: DomainParticipantImpl::delete_subscriber, ")
               ACE_TEXT("%p\n"),
               ACE_TEXT("remove")));
    return DDS::RETCODE_ERROR;

  } else {
    return DDS::RETCODE_OK;
  }
}

DDS::Subscriber_ptr
DomainParticipantImpl::get_builtin_subscriber()
{
  return DDS::Subscriber::_duplicate(bit_subscriber_.in());
}

DDS::Topic_ptr
DomainParticipantImpl::create_topic(
  const char * topic_name,
  const char * type_name,
  const DDS::TopicQos & qos,
  DDS::TopicListener_ptr a_listener,
  DDS::StatusMask mask)
{
  return create_topic_i(topic_name,
                        type_name,
                        qos,
                        a_listener,
                        mask,
                        0);
}

DDS::Topic_ptr
DomainParticipantImpl::create_typeless_topic(
  const char * topic_name,
  const char * type_name,
  bool type_has_keys,
  const DDS::TopicQos & qos,
  DDS::TopicListener_ptr a_listener,
  DDS::StatusMask mask)
{
  int topic_mask = (type_has_keys ? TOPIC_TYPE_HAS_KEYS : 0 ) | TOPIC_TYPELESS;

  return create_topic_i(topic_name,
                        type_name,
                        qos,
                        a_listener,
                        mask,
                        topic_mask);
}


DDS::Topic_ptr
DomainParticipantImpl::create_topic_i(
  const char * topic_name,
  const char * type_name,
  const DDS::TopicQos & qos,
  DDS::TopicListener_ptr a_listener,
  DDS::StatusMask mask,
  int topic_mask)
{
  DDS::TopicQos topic_qos;

  if (qos == TOPIC_QOS_DEFAULT) {
    this->get_default_topic_qos(topic_qos);

  } else {
    topic_qos = qos;
  }

  OPENDDS_NO_OWNERSHIP_KIND_EXCLUSIVE_COMPATIBILITY_CHECK(qos, DDS::Topic::_nil());
  OPENDDS_NO_OWNERSHIP_PROFILE_COMPATIBILITY_CHECK(qos, DDS::Topic::_nil());
  OPENDDS_NO_DURABILITY_SERVICE_COMPATIBILITY_CHECK(qos, DDS::Topic::_nil());
  OPENDDS_NO_DURABILITY_KIND_TRANSIENT_PERSISTENT_COMPATIBILITY_CHECK(qos, DDS::Topic::_nil());

  if (!Qos_Helper::valid(topic_qos)) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("(%P|%t) ERROR: ")
               ACE_TEXT("DomainParticipantImpl::create_topic, ")
               ACE_TEXT("invalid qos.\n")));
    return DDS::Topic::_nil();
  }

  if (!Qos_Helper::consistent(topic_qos)) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("(%P|%t) ERROR: ")
               ACE_TEXT("DomainParticipantImpl::create_topic, ")
               ACE_TEXT("inconsistent qos.\n")));
    return DDS::Topic::_nil();
  }

  TopicMap::mapped_type* entry = 0;
  bool found = false;
  {
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex,
                     tao_mon,
                     this->topics_protector_,
                     DDS::Topic::_nil());

#if !defined(OPENDDS_NO_CONTENT_FILTERED_TOPIC) || !defined(OPENDDS_NO_MULTI_TOPIC)
    if (topic_descrs_.count(topic_name)) {
      if (DCPS_debug_level > 3) {
        ACE_ERROR((LM_ERROR, ACE_TEXT("(%P|%t) ERROR: ")
          ACE_TEXT("DomainParticipantImpl::create_topic, ")
          ACE_TEXT("can't create a Topic due to name \"%C\" already in use ")
          ACE_TEXT("by a TopicDescription.\n"), topic_name));
      }
      return 0;
    }
#endif

    if (Util::find(topics_, topic_name, entry) == 0) {
      found = true;
    }
  }

  if (found) {
    CORBA::String_var found_type
    = entry->pair_.svt_->get_type_name();

    if (ACE_OS::strcmp(type_name, found_type) == 0) {
      DDS::TopicQos found_qos;
      entry->pair_.svt_->get_qos(found_qos);

      if (topic_qos == found_qos) { // match type name, qos
        {
          ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex,
                           tao_mon,
                           this->topics_protector_,
                           DDS::Topic::_nil());
          entry->client_refs_ ++;
        }
        return DDS::Topic::_duplicate(entry->pair_.obj_.in());

      } else {
        if (DCPS_debug_level >= 1) {
          ACE_DEBUG((LM_DEBUG,
                     ACE_TEXT("(%P|%t) DomainParticipantImpl::create_topic, ")
                     ACE_TEXT("qos not match: topic_name=%C type_name=%C\n"),
                     topic_name, type_name));
        }

        return DDS::Topic::_nil();
      }

    } else { // no match
      if (DCPS_debug_level >= 1) {
        ACE_DEBUG((LM_DEBUG,
                   ACE_TEXT("(%P|%t) DomainParticipantImpl::create_topic, ")
                   ACE_TEXT(" not match: topic_name=%C type_name=%C\n"),
                   topic_name, type_name));
      }

      return DDS::Topic::_nil();
    }

  } else {

    OpenDDS::DCPS::TypeSupport_ptr type_support=0;
    bool has_keys = (topic_mask & TOPIC_TYPE_HAS_KEYS);

    if (0 == topic_mask){
      // creating a topic with compile time type
       type_support= Registered_Data_Types->lookup(this, type_name);
       has_keys = type_support->has_dcps_key();
    }
    RepoId topic_id;

    Discovery_rch disco = TheServiceParticipant->get_discovery(domain_id_);
    TopicStatus status = disco->assert_topic(topic_id,
                                             domain_id_,
                                             dp_id_,
                                             topic_name,
                                             type_name,
                                             topic_qos,
                                             has_keys);

    if (status == CREATED || status == FOUND) {
      DDS::Topic_ptr new_topic = create_new_topic(topic_id,
                                                topic_name,
                                                type_name,
                                                topic_qos,
                                                a_listener,
                                                mask,
                                                type_support);
      if (this->monitor_) {
        this->monitor_->report();
      }
      return new_topic;

    } else {
      ACE_ERROR((LM_ERROR,
                 ACE_TEXT("(%P|%t) ERROR: DomainParticipantImpl::create_topic, ")
                 ACE_TEXT("assert_topic failed.\n")));
      return DDS::Topic::_nil();
    }
  }
}

DDS::ReturnCode_t
DomainParticipantImpl::delete_topic(
  DDS::Topic_ptr a_topic)
{
  return delete_topic_i(a_topic, false);
}

DDS::ReturnCode_t
DomainParticipantImpl::delete_topic_i(
  DDS::Topic_ptr a_topic,
  bool             remove_objref)
{

  DDS::ReturnCode_t ret = DDS::RETCODE_OK;

  try {
    // The servant's ref count should be greater than 2 at this point,
    // one referenced by poa, one referenced by the topic map and
    // others referenced by the datareader/datawriter.
    TopicImpl* the_topic_servant = dynamic_cast<TopicImpl*>(a_topic);

    CORBA::String_var topic_name = the_topic_servant->get_name();

    DDS::DomainParticipant_var dp = the_topic_servant->get_participant();

    DomainParticipantImpl* the_dp_servant =
      dynamic_cast<DomainParticipantImpl*>(dp.in());

    if (the_dp_servant != this ||
        (!remove_objref && the_topic_servant->entity_refs())) {
      // If entity_refs is true (nonzero), then some reader or writer is using
      // this topic and the spec requires delete_topic() to fail with the error:
      return DDS::RETCODE_PRECONDITION_NOT_MET;
    }

    {

      ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex,
                       tao_mon,
                       this->topics_protector_,
                       DDS::RETCODE_ERROR);

      TopicMap::mapped_type* entry = 0;

      if (Util::find(topics_, topic_name.in(), entry) == -1) {
        ACE_ERROR_RETURN((LM_ERROR,
                          ACE_TEXT("(%P|%t) ERROR: DomainParticipantImpl::delete_topic_i, ")
                          ACE_TEXT("%p\n"),
                          ACE_TEXT("find")),
                         DDS::RETCODE_ERROR);
      }

      entry->client_refs_ --;

      if (remove_objref == true ||
          0 == entry->client_refs_) {
        //TBD - mark the TopicImpl as deleted and make it
        //      reject calls to the TopicImpl.
        Discovery_rch disco = TheServiceParticipant->get_discovery(domain_id_);
        TopicStatus status
        = disco->remove_topic(the_dp_servant->get_domain_id(),
                              the_dp_servant->get_id(),
                              the_topic_servant->get_id());

        if (status != REMOVED) {
          ACE_ERROR_RETURN((LM_ERROR,
                            ACE_TEXT("(%P|%t) ERROR: DomainParticipantImpl::delete_topic_i, ")
                            ACE_TEXT("remove_topic failed\n")),
                           DDS::RETCODE_ERROR);
        }

        // note: this will destroy the TopicImpl if there are no
        // client object reference to it.
        if (topics_.erase(topic_name.in()) == 0) {
          ACE_ERROR_RETURN((LM_ERROR,
                            ACE_TEXT("(%P|%t) ERROR: DomainParticipantImpl::delete_topic_i, ")
                            ACE_TEXT("%p \n"),
                            ACE_TEXT("unbind")),
                           DDS::RETCODE_ERROR);

        } else
          return DDS::RETCODE_OK;

      }
    }

  } catch (...) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("(%P|%t) ERROR: DomainParticipantImpl::delete_topic_i, ")
               ACE_TEXT(" Caught Unknown Exception \n")));
    ret = DDS::RETCODE_ERROR;
  }

  return ret;
}

//Note: caller should NOT assign to Topic_var (without _duplicate'ing)
//      because it will steal the framework's reference.
DDS::Topic_ptr
DomainParticipantImpl::find_topic(
  const char * topic_name,
  const DDS::Duration_t & timeout)
{
  ACE_Time_Value timeout_tv
  = ACE_OS::gettimeofday() + ACE_Time_Value(timeout.sec, timeout.nanosec/1000);

  int first_time = 1;

  while (first_time || ACE_OS::gettimeofday() < timeout_tv) {
    if (first_time) {
      first_time = 0;
    }

    TopicMap::mapped_type* entry = 0;
    {
      ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex,
                       tao_mon,
                       this->topics_protector_,
                       DDS::Topic::_nil());

      if (Util::find(topics_, topic_name, entry) == 0) {
        entry->client_refs_ ++;
        return DDS::Topic::_duplicate(entry->pair_.obj_.in());
      }
    }

    RepoId topic_id;
    CORBA::String_var type_name;
    DDS::TopicQos_var qos;

    Discovery_rch disco = TheServiceParticipant->get_discovery(domain_id_);
    TopicStatus status = disco->find_topic(domain_id_,
                                           topic_name,
                                           type_name.out(),
                                           qos.out(),
                                           topic_id);


    if (status == FOUND) {
      OpenDDS::DCPS::TypeSupport_ptr type_support = Registered_Data_Types->lookup(this, type_name.in());
      if (0 == type_support) {
        if (DCPS_debug_level) {
            ACE_ERROR((LM_ERROR, ACE_TEXT("(%P|%t) ERROR: ")
                       ACE_TEXT("DomainParticipantImpl::find_topic, ")
                       ACE_TEXT("can't create a Topic: type_name \"%C\"")
                       ACE_TEXT("is not registered.\n"), type_name.in()));
        }

        return DDS::Topic::_nil();
      }

      DDS::Topic_ptr new_topic = create_new_topic(topic_id,
                                                  topic_name,
                                                  type_name,
                                                  qos,
                                                  DDS::TopicListener::_nil(),
                                                  OpenDDS::DCPS::DEFAULT_STATUS_MASK,
                                                  type_support);
      return new_topic;

    } else if (status == INTERNAL_ERROR) {
      ACE_ERROR((LM_ERROR,
                 ACE_TEXT("(%P|%t) ERROR: DomainParticipantImpl::find_topic - ")
                 ACE_TEXT("topic not found, discovery returned INTERNAL_ERROR!\n")));
      return DDS::Topic::_nil();
    } else {
      ACE_Time_Value now = ACE_OS::gettimeofday();

      if (now < timeout_tv) {
        ACE_Time_Value remaining = timeout_tv - now;

        if (remaining.sec() >= 1) {
          ACE_OS::sleep(1);

        } else {
          ACE_OS::sleep(remaining);
        }
      }
    }
  }

  if (DCPS_debug_level >= 1) {
    // timed out
    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("(%P|%t) DomainParticipantImpl::find_topic, ")
               ACE_TEXT("timed out. \n")));
  }

  return DDS::Topic::_nil();
}

DDS::TopicDescription_ptr
DomainParticipantImpl::lookup_topicdescription(const char* name)
{
  ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex,
                   tao_mon,
                   this->topics_protector_,
                   DDS::Topic::_nil());

  TopicMap::mapped_type* entry = 0;

  if (Util::find(topics_, name, entry) == -1) {
#if !defined(OPENDDS_NO_CONTENT_FILTERED_TOPIC) || !defined(OPENDDS_NO_MULTI_TOPIC)
    TopicDescriptionMap::iterator iter = topic_descrs_.find(name);
    if (iter != topic_descrs_.end()) {
      return DDS::TopicDescription::_duplicate(iter->second);
    }
#endif
    return DDS::TopicDescription::_nil();

  } else {
    return DDS::TopicDescription::_duplicate(entry->pair_.obj_.in());
  }
}

#ifndef OPENDDS_NO_CONTENT_FILTERED_TOPIC

DDS::ContentFilteredTopic_ptr
DomainParticipantImpl::create_contentfilteredtopic(
  const char* name,
  DDS::Topic_ptr related_topic,
  const char* filter_expression,
  const DDS::StringSeq& expression_parameters)
{
  if (CORBA::is_nil(related_topic)) {
    if (DCPS_debug_level > 3) {
      ACE_ERROR((LM_ERROR, ACE_TEXT("(%P|%t) ERROR: ")
        ACE_TEXT("DomainParticipantImpl::create_contentfilteredtopic, ")
        ACE_TEXT("can't create a content-filtered topic due to null related ")
        ACE_TEXT("topic.\n")));
    }
    return 0;
  }

  ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, topics_protector_, 0);

  if (topics_.count(name)) {
    if (DCPS_debug_level > 3) {
      ACE_ERROR((LM_ERROR, ACE_TEXT("(%P|%t) ERROR: ")
        ACE_TEXT("DomainParticipantImpl::create_contentfilteredtopic, ")
        ACE_TEXT("can't create a content-filtered topic due to name \"%C\" ")
        ACE_TEXT("already in use by a Topic.\n"), name));
    }
    return 0;
  }

  if (topic_descrs_.count(name)) {
    if (DCPS_debug_level > 3) {
      ACE_ERROR((LM_ERROR, ACE_TEXT("(%P|%t) ERROR: ")
        ACE_TEXT("DomainParticipantImpl::create_contentfilteredtopic, ")
        ACE_TEXT("can't create a content-filtered topic due to name \"%C\" ")
        ACE_TEXT("already in use by a TopicDescription.\n"), name));
    }
    return 0;
  }

  DDS::ContentFilteredTopic_var cft;
  try {
    cft = new ContentFilteredTopicImpl(name,
      related_topic, filter_expression, expression_parameters, this);
  } catch (const std::exception& e) {
    if (DCPS_debug_level) {
      ACE_ERROR((LM_ERROR, ACE_TEXT("(%P|%t) ERROR: ")
        ACE_TEXT("DomainParticipantImpl::create_contentfilteredtopic, ")
        ACE_TEXT("can't create a content-filtered topic due to runtime error: ")
        ACE_TEXT("%C.\n"), e.what()));
    }
    return 0;
  }
  DDS::TopicDescription_var td = DDS::TopicDescription::_duplicate(cft);
  topic_descrs_[name] = td;
  return cft._retn();
}

DDS::ReturnCode_t DomainParticipantImpl::delete_contentfilteredtopic(
  DDS::ContentFilteredTopic_ptr a_contentfilteredtopic)
{
  ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, topics_protector_,
                   DDS::RETCODE_OUT_OF_RESOURCES);
  DDS::ContentFilteredTopic_var cft =
    DDS::ContentFilteredTopic::_duplicate(a_contentfilteredtopic);
  CORBA::String_var name = cft->get_name();
  TopicDescriptionMap::iterator iter = topic_descrs_.find(name.in());
  if (iter == topic_descrs_.end()) {
    if (DCPS_debug_level > 3) {
      ACE_ERROR((LM_ERROR, ACE_TEXT("(%P|%t) ERROR: ")
        ACE_TEXT("DomainParticipantImpl::delete_contentfilteredtopic, ")
        ACE_TEXT("can't delete a content-filtered topic \"%C\" ")
        ACE_TEXT("because it is not in the set.\n"), name.in ()));
    }
    return DDS::RETCODE_PRECONDITION_NOT_MET;
  }
  if (dynamic_cast<TopicDescriptionImpl*>(iter->second.in())->has_reader()) {
    if (DCPS_debug_level > 3) {
      ACE_ERROR((LM_ERROR, ACE_TEXT("(%P|%t) ERROR: ")
        ACE_TEXT("DomainParticipantImpl::delete_contentfilteredtopic, ")
        ACE_TEXT("can't delete a content-filtered topic \"%C\" ")
        ACE_TEXT("because it still is used by a reader.\n"), name.in ()));
    }
    return DDS::RETCODE_PRECONDITION_NOT_MET;
  }
  topic_descrs_.erase(iter);
  return DDS::RETCODE_OK;
}

#endif // OPENDDS_NO_CONTENT_FILTERED_TOPIC

#ifndef OPENDDS_NO_MULTI_TOPIC

DDS::MultiTopic_ptr DomainParticipantImpl::create_multitopic(
  const char* name, const char* type_name,
  const char* subscription_expression,
  const DDS::StringSeq& expression_parameters)
{
  ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, topics_protector_, 0);

  if (topics_.count(name)) {
    if (DCPS_debug_level > 3) {
      ACE_ERROR((LM_ERROR, ACE_TEXT("(%P|%t) ERROR: ")
        ACE_TEXT("DomainParticipantImpl::create_multitopic, ")
        ACE_TEXT("can't create a multi topic due to name \"%C\" ")
        ACE_TEXT("already in use by a Topic.\n"), name));
    }
    return 0;
  }

  if (topic_descrs_.count(name)) {
    if (DCPS_debug_level > 3) {
      ACE_ERROR((LM_ERROR, ACE_TEXT("(%P|%t) ERROR: ")
        ACE_TEXT("DomainParticipantImpl::create_multitopic, ")
        ACE_TEXT("can't create a multi topic due to name \"%C\" ")
        ACE_TEXT("already in use by a TopicDescription.\n"), name));
    }
    return 0;
  }

  DDS::MultiTopic_var mt;
  try {
    mt = new MultiTopicImpl(name, type_name, subscription_expression,
      expression_parameters, this);
  } catch (const std::exception& e) {
    if (DCPS_debug_level) {
      ACE_ERROR((LM_ERROR, ACE_TEXT("(%P|%t) ERROR: ")
        ACE_TEXT("DomainParticipantImpl::create_multitopic, ")
        ACE_TEXT("can't create a multi topic due to runtime error: ")
        ACE_TEXT("%C.\n"), e.what()));
    }
    return 0;
  }
  DDS::TopicDescription_var td = DDS::TopicDescription::_duplicate(mt);
  topic_descrs_[name] = td;
  return mt._retn();
}

DDS::ReturnCode_t DomainParticipantImpl::delete_multitopic(
  DDS::MultiTopic_ptr a_multitopic)
{
  ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, topics_protector_,
                   DDS::RETCODE_OUT_OF_RESOURCES);
  DDS::MultiTopic_var mt = DDS::MultiTopic::_duplicate(a_multitopic);
  CORBA::String_var mt_name = mt->get_name();
  TopicDescriptionMap::iterator iter = topic_descrs_.find(mt_name.in());
  if (iter == topic_descrs_.end()) {
    return DDS::RETCODE_PRECONDITION_NOT_MET;
  }
  if (dynamic_cast<TopicDescriptionImpl*>(iter->second.in())->has_reader()) {
    return DDS::RETCODE_PRECONDITION_NOT_MET;
  }
  topic_descrs_.erase(iter);
  return DDS::RETCODE_OK;
}

#endif // OPENDDS_NO_MULTI_TOPIC

#ifndef OPENDDS_NO_CONTENT_SUBSCRIPTION_PROFILE

RcHandle<FilterEvaluator>
DomainParticipantImpl::get_filter_eval(const char* filter)
{
  ACE_GUARD_RETURN(ACE_Thread_Mutex, guard, filter_cache_lock_,
                   RcHandle<FilterEvaluator>());
  typedef std::map<std::string, RcHandle<FilterEvaluator> > Map;
  Map::iterator iter = filter_cache_.find(filter);
  if (iter == filter_cache_.end()) {
    return filter_cache_[filter] = new FilterEvaluator(filter, false);
  }
  return iter->second;
}

void
DomainParticipantImpl::deref_filter_eval(const char* filter)
{
  ACE_GUARD(ACE_Thread_Mutex, guard, filter_cache_lock_);
  typedef std::map<std::string, RcHandle<FilterEvaluator> > Map;
  Map::iterator iter = filter_cache_.find(filter);
  if (iter != filter_cache_.end()) {
    if (iter->second->ref_count() == 1) {
      filter_cache_.erase(iter);
    }
  }
}

#endif

DDS::ReturnCode_t
DomainParticipantImpl::delete_contained_entities()
{
  // mark that the entity is being deleted
  set_deleted(true);

  // BIT subsciber and data readers will be deleted with the
  // rest of the entities, so need to report to discovery that
  // BIT is no longer available
  Discovery_rch disc = TheServiceParticipant->get_discovery(this->domain_id_);
  disc->fini_bit(this);

  // delete publishers
  {
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex,
                     tao_mon,
                     this->publishers_protector_,
                     DDS::RETCODE_ERROR);

    PublisherSet::iterator pubIter = publishers_.begin();
    DDS::Publisher_ptr pubPtr;
    size_t pubsize = publishers_.size();

    while (pubsize > 0) {
      pubPtr = (*pubIter).obj_.in();
      ++pubIter;

      DDS::ReturnCode_t result
      = pubPtr->delete_contained_entities();

      if (result != DDS::RETCODE_OK) {
        return result;
      }

      result = delete_publisher(pubPtr);

      if (result != DDS::RETCODE_OK) {
        return result;
      }

      pubsize--;
    }

  }

  // delete subscribers
  {
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex,
                     tao_mon,
                     this->subscribers_protector_,
                     DDS::RETCODE_ERROR);

    SubscriberSet::iterator subIter = subscribers_.begin();
    DDS::Subscriber_ptr subPtr;
    size_t subsize = subscribers_.size();

    while (subsize > 0) {
      subPtr = (*subIter).obj_.in();
      ++subIter;

      DDS::ReturnCode_t result = subPtr->delete_contained_entities();

      if (result != DDS::RETCODE_OK) {
        return result;
      }

      result = delete_subscriber(subPtr);

      if (result != DDS::RETCODE_OK) {
        return result;
      }

      subsize--;
    }
  }

  DDS::ReturnCode_t ret = DDS::RETCODE_OK;
  // delete topics
  {
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex,
                     tao_mon,
                     this->topics_protector_,
                     DDS::RETCODE_ERROR);

    while (1) {
      if (topics_.begin() == topics_.end()) {
        break;
      }

      // Delete the topic the reference count.
      DDS::ReturnCode_t result = this->delete_topic_i(
                                     topics_.begin()->second.pair_.obj_.in(), true);

      if (result != DDS::RETCODE_OK) {
        ret = result;
      }
    }
  }

  {
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex,
                     tao_mon,
                     this->recorders_protector_,
                     DDS::RETCODE_ERROR);

    RecorderSet::iterator it = recorders_.begin();
    for (; it != recorders_.end(); ++it ){
      RecorderImpl* impl = static_cast<RecorderImpl* >(it->in());
      DDS::ReturnCode_t result = DDS::RETCODE_ERROR;
      if (impl) result = impl->cleanup();
      if (result != DDS::RETCODE_OK) ret = result;
    }
    recorders_.clear();
  }

  {
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex,
                     tao_mon,
                     this->replayers_protector_,
                     DDS::RETCODE_ERROR);

    ReplayerSet::iterator it = replayers_.begin();
    for (; it != replayers_.end(); ++it ){
      ReplayerImpl* impl = static_cast<ReplayerImpl* >(it->in());
      DDS::ReturnCode_t result = DDS::RETCODE_ERROR;
      if (impl) result = impl->cleanup();
      if (result != DDS::RETCODE_OK) ret = result;

    }

    replayers_.clear();
  }


  bit_subscriber_ = DDS::Subscriber::_nil();

  OpenDDS::DCPS::Registered_Data_Types->unregister_participant(this);
  participant_objref_ = DDS::DomainParticipant::_nil();

  // the participant can now start creating new contained entities
  set_deleted(false);

  return ret;
}

CORBA::Boolean
DomainParticipantImpl::contains_entity(DDS::InstanceHandle_t a_handle)
{
  /// Check top-level containers for Topic, Subscriber,
  /// and Publisher instances.
  {
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex,
                     guard,
                     this->topics_protector_,
                     false);

    for (TopicMap::iterator it(topics_.begin());
         it != topics_.end(); ++it) {
      if (a_handle == it->second.pair_.svt_->get_instance_handle())
        return true;
    }
  }

  {
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex,
                     guard,
                     this->subscribers_protector_,
                     false);

    for (SubscriberSet::iterator it(subscribers_.begin());
         it != subscribers_.end(); ++it) {
      if (a_handle == it->svt_->get_instance_handle())
        return true;
    }
  }

  {
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex,
                     guard,
                     this->publishers_protector_,
                     false);

    for (PublisherSet::iterator it(publishers_.begin());
         it != publishers_.end(); ++it) {
      if (a_handle == it->svt_->get_instance_handle())
        return true;
    }
  }

  /// Recurse into SubscriberImpl and PublisherImpl for
  /// DataReader and DataWriter instances respectively.
  for (SubscriberSet::iterator it(subscribers_.begin());
       it != subscribers_.end(); ++it) {
    if (it->svt_->contains_reader(a_handle))
      return true;
  }

  for (PublisherSet::iterator it(publishers_.begin());
       it != publishers_.end(); ++it) {
    if (it->svt_->contains_writer(a_handle))
      return true;
  }

  return false;
}

DDS::ReturnCode_t
DomainParticipantImpl::set_qos(
  const DDS::DomainParticipantQos & qos)
{
  if (Qos_Helper::valid(qos) && Qos_Helper::consistent(qos)) {
    if (qos_ == qos)
      return DDS::RETCODE_OK;

    // for the not changeable qos, it can be changed before enable
    if (!Qos_Helper::changeable(qos_, qos) && enabled_ == true) {
      return DDS::RETCODE_IMMUTABLE_POLICY;

    } else {
      qos_ = qos;

      Discovery_rch disco = TheServiceParticipant->get_discovery(domain_id_);
      const bool status =
        disco->update_domain_participant_qos(domain_id_,
                                             dp_id_,
                                             qos_);

      if (!status) {
        ACE_ERROR_RETURN((LM_ERROR,
                          ACE_TEXT("(%P|%t) DomainParticipantImpl::set_qos, ")
                          ACE_TEXT("failed on compatiblity check. \n")),
                         DDS::RETCODE_ERROR);
      }
    }

    return DDS::RETCODE_OK;

  } else {
    return DDS::RETCODE_INCONSISTENT_POLICY;
  }
}

DDS::ReturnCode_t
DomainParticipantImpl::get_qos(
  DDS::DomainParticipantQos & qos)
{
  qos = qos_;
  return DDS::RETCODE_OK;
}

DDS::ReturnCode_t
DomainParticipantImpl::set_listener(
  DDS::DomainParticipantListener_ptr a_listener,
  DDS::StatusMask mask)
{
  listener_mask_ = mask;
  //note: OK to duplicate  a nil object ref
  listener_ = DDS::DomainParticipantListener::_duplicate(a_listener);
  return DDS::RETCODE_OK;
}

DDS::DomainParticipantListener_ptr
DomainParticipantImpl::get_listener()
{
  return DDS::DomainParticipantListener::_duplicate(listener_.in());
}

DDS::ReturnCode_t
DomainParticipantImpl::ignore_participant(
  DDS::InstanceHandle_t handle)
{
#if !defined (DDS_HAS_MINIMUM_BIT)

  if (enabled_ == false) {
    ACE_ERROR_RETURN((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: DomainParticipantImpl::ignore_participant, ")
                      ACE_TEXT(" Entity is not enabled. \n")),
                     DDS::RETCODE_NOT_ENABLED);
  }

  RepoId ignoreId = get_repoid(handle);
  HandleMap::const_iterator location = this->ignored_participants_.find(ignoreId);

  if (location == this->ignored_participants_.end()) {
    this->ignored_participants_[ ignoreId] = handle;
  }
  else {// ignore same participant again, just return ok.
    return DDS::RETCODE_OK;
  }

  if (DCPS_debug_level >= 4) {
    GuidConverter converter(dp_id_);
    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("(%P|%t) DomainParticipantImpl::ignore_participant: ")
               ACE_TEXT("%C ignoring handle %x.\n"),
               std::string(converter).c_str(),
               handle));
  }

  Discovery_rch disco = TheServiceParticipant->get_discovery(domain_id_);
  if (!disco->ignore_domain_participant(domain_id_,
                                        dp_id_,
                                        ignoreId)) {
    ACE_ERROR_RETURN((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: DomainParticipantImpl::ignore_participant, ")
                      ACE_TEXT(" Could not ignore domain participant.\n")),
                     DDS::RETCODE_NOT_ENABLED);
    return DDS::RETCODE_ERROR;
  }


  if (DCPS_debug_level >= 4) {
    GuidConverter converter(dp_id_);
    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("(%P|%t) DomainParticipantImpl::ignore_participant: ")
               ACE_TEXT("%C repo call returned.\n"),
               std::string(converter).c_str()));
  }

  return DDS::RETCODE_OK;
#else
  ACE_UNUSED_ARG(handle);
  return DDS::RETCODE_UNSUPPORTED;
#endif // !defined (DDS_HAS_MINIMUM_BIT)
}

DDS::ReturnCode_t
DomainParticipantImpl::ignore_topic(
  DDS::InstanceHandle_t handle)
{
#if !defined (DDS_HAS_MINIMUM_BIT)

  if (enabled_ == false) {
    ACE_ERROR_RETURN((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: DomainParticipantImpl::ignore_topic, ")
                      ACE_TEXT(" Entity is not enabled. \n")),
                     DDS::RETCODE_NOT_ENABLED);
  }

  RepoId ignoreId = get_repoid(handle);
  HandleMap::const_iterator location = this->ignored_topics_.find(ignoreId);

  if (location == this->ignored_topics_.end()) {
    this->ignored_topics_[ ignoreId] = handle;
  }
  else { // ignore same topic again, just return ok.
    return DDS::RETCODE_OK;
  }

  if (DCPS_debug_level >= 4) {
    GuidConverter converter(dp_id_);
    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("(%P|%t) DomainParticipantImpl::ignore_topic: ")
               ACE_TEXT("%C ignoring handle %x.\n"),
               std::string(converter).c_str(),
               handle));
  }

  Discovery_rch disco = TheServiceParticipant->get_discovery(domain_id_);
  if (!disco->ignore_topic(domain_id_,
                           dp_id_,
                           ignoreId)) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("(%P|%t) ERROR: DomainParticipantImpl::ignore_topic, ")
               ACE_TEXT(" Could not ignore topic.\n")));
  }

  return DDS::RETCODE_OK;
#else
  ACE_UNUSED_ARG(handle);
  return DDS::RETCODE_UNSUPPORTED;
#endif // !defined (DDS_HAS_MINIMUM_BIT)
}

DDS::ReturnCode_t
DomainParticipantImpl::ignore_publication(
  DDS::InstanceHandle_t handle)
{
#if !defined (DDS_HAS_MINIMUM_BIT)

  if (enabled_ == false) {
    ACE_ERROR_RETURN((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: DomainParticipantImpl::ignore_publication, ")
                      ACE_TEXT(" Entity is not enabled. \n")),
                     DDS::RETCODE_NOT_ENABLED);
  }

  if (DCPS_debug_level >= 4) {
    GuidConverter converter(dp_id_);
    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("(%P|%t) DomainParticipantImpl::ignore_publication: ")
               ACE_TEXT("%C ignoring handle %x.\n"),
               std::string(converter).c_str(),
               handle));
  }

  RepoId ignoreId = get_repoid(handle);
  Discovery_rch disco = TheServiceParticipant->get_discovery(domain_id_);
  if (!disco->ignore_publication(domain_id_,
                                 dp_id_,
                                 ignoreId)) {
    ACE_ERROR_RETURN((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: DomainParticipantImpl::ignore_publication, ")
                      ACE_TEXT(" could not ignore publication in discovery. \n")),
                     DDS::RETCODE_ERROR);
  }

  return DDS::RETCODE_OK;
#else
  ACE_UNUSED_ARG(handle);
  return DDS::RETCODE_UNSUPPORTED;
#endif // !defined (DDS_HAS_MINIMUM_BIT)
}

DDS::ReturnCode_t
DomainParticipantImpl::ignore_subscription(
  DDS::InstanceHandle_t handle)
{
#if !defined (DDS_HAS_MINIMUM_BIT)

  if (enabled_ == false) {
    ACE_ERROR_RETURN((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: DomainParticipantImpl::ignore_subscription, ")
                      ACE_TEXT(" Entity is not enabled. \n")),
                     DDS::RETCODE_NOT_ENABLED);
  }

  if (DCPS_debug_level >= 4) {
    GuidConverter converter(dp_id_);
    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("(%P|%t) DomainParticipantImpl::ignore_subscription: ")
               ACE_TEXT("%C ignoring handle %d.\n"),
               std::string(converter).c_str(),
               handle));
  }


  RepoId ignoreId = get_repoid(handle);
  Discovery_rch disco = TheServiceParticipant->get_discovery(domain_id_);
  if (!disco->ignore_subscription(domain_id_,
                                  dp_id_,
                                  ignoreId)) {
    ACE_ERROR_RETURN((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: DomainParticipantImpl::ignore_subscription, ")
                      ACE_TEXT(" could not ignore subscription in discovery. \n")),
                     DDS::RETCODE_ERROR);
  }

  return DDS::RETCODE_OK;
#else
  ACE_UNUSED_ARG(handle);
  return DDS::RETCODE_UNSUPPORTED;
#endif // !defined (DDS_HAS_MINIMUM_BIT)
}

DDS::DomainId_t
DomainParticipantImpl::get_domain_id()
{
  return domain_id_;
}

DDS::ReturnCode_t
DomainParticipantImpl::assert_liveliness()
{
  // This operation needs to only be used if the DomainParticipant contains
  // DataWriter entities with the LIVELINESS set to MANUAL_BY_PARTICIPANT and
  // it only affects the liveliness of those DataWriter entities. Otherwise,
  // it has no effect.
  // This will do nothing in current implementation since we only
  // support the AUTOMATIC liveliness qos for datawriter.
  // Add implementation here.

  ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex,
                   tao_mon,
                   this->publishers_protector_,
                   DDS::RETCODE_ERROR);

  for (PublisherSet::iterator it(publishers_.begin());
       it != publishers_.end(); ++it) {
    it->svt_->assert_liveliness_by_participant();
  }

  return DDS::RETCODE_OK;
}

DDS::ReturnCode_t
DomainParticipantImpl::set_default_publisher_qos(
  const DDS::PublisherQos & qos)
{
  if (Qos_Helper::valid(qos) && Qos_Helper::consistent(qos)) {
    default_publisher_qos_ = qos;
    return DDS::RETCODE_OK;

  } else {
    return DDS::RETCODE_INCONSISTENT_POLICY;
  }
}

DDS::ReturnCode_t
DomainParticipantImpl::get_default_publisher_qos(
  DDS::PublisherQos & qos)
{
  qos = default_publisher_qos_;
  return DDS::RETCODE_OK;
}

DDS::ReturnCode_t
DomainParticipantImpl::set_default_subscriber_qos(
  const DDS::SubscriberQos & qos)
{
  if (Qos_Helper::valid(qos) && Qos_Helper::consistent(qos)) {
    default_subscriber_qos_ = qos;
    return DDS::RETCODE_OK;

  } else {
    return DDS::RETCODE_INCONSISTENT_POLICY;
  }
}

DDS::ReturnCode_t
DomainParticipantImpl::get_default_subscriber_qos(
  DDS::SubscriberQos & qos)
{
  qos = default_subscriber_qos_;
  return DDS::RETCODE_OK;
}

DDS::ReturnCode_t
DomainParticipantImpl::set_default_topic_qos(
  const DDS::TopicQos & qos)
{
  if (Qos_Helper::valid(qos) && Qos_Helper::consistent(qos)) {
    default_topic_qos_ = qos;
    return DDS::RETCODE_OK;

  } else {
    return DDS::RETCODE_INCONSISTENT_POLICY;
  }
}

DDS::ReturnCode_t
DomainParticipantImpl::get_default_topic_qos(
  DDS::TopicQos & qos)
{
  qos = default_topic_qos_;
  return DDS::RETCODE_OK;
}

DDS::ReturnCode_t
DomainParticipantImpl::get_current_time(
  DDS::Time_t & current_time)
{
  current_time
  = OpenDDS::DCPS::time_value_to_time(
      ACE_OS::gettimeofday());
  return DDS::RETCODE_OK;
}

#if !defined (DDS_HAS_MINIMUM_BIT)

DDS::ReturnCode_t
DomainParticipantImpl::get_discovered_participants(
  DDS::InstanceHandleSeq & participant_handles)
{
  ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex,
                   guard,
                   this->handle_protector_,
                   DDS::RETCODE_ERROR);

  HandleMap::const_iterator itEnd = this->handles_.end();

  for (HandleMap::const_iterator iter = this->handles_.begin();
       iter != itEnd; ++iter) {
    GuidConverter converter(iter->first);

    if (converter.entityKind() == KIND_PARTICIPANT)
    {
      // skip itself and the ignored participant
      if (iter->first == this->dp_id_
      || (this->ignored_participants_.find(iter->first)
        != this->ignored_participants_.end ())) {
        continue;
      }

      push_back(participant_handles, iter->second);
    }
  }

  return DDS::RETCODE_OK;
}

DDS::ReturnCode_t
DomainParticipantImpl::get_discovered_participant_data(
  DDS::ParticipantBuiltinTopicData & participant_data,
  DDS::InstanceHandle_t participant_handle)
{
  {
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex,
                     guard,
                     this->handle_protector_,
                     DDS::RETCODE_ERROR);

    bool found = false;
    HandleMap::const_iterator itEnd = this->handles_.end();

    for (HandleMap::const_iterator iter = this->handles_.begin();
         iter != itEnd; ++iter) {
      GuidConverter converter(iter->first);

      if (participant_handle == iter->second
          && converter.entityKind() == KIND_PARTICIPANT) {
        found = true;
        break;
      }
    }

    if (!found)
      return DDS::RETCODE_PRECONDITION_NOT_MET;
  }

  DDS::SampleInfoSeq info;
  DDS::ParticipantBuiltinTopicDataSeq data;
  DDS::DataReader_var dr =
    this->bit_subscriber_->lookup_datareader(BUILT_IN_PARTICIPANT_TOPIC);
  DDS::ParticipantBuiltinTopicDataDataReader_var bit_part_dr =
    DDS::ParticipantBuiltinTopicDataDataReader::_narrow(dr);
  DDS::ReturnCode_t ret = bit_part_dr->read_instance(data,
                                                     info,
                                                     1,
                                                     participant_handle,
                                                     DDS::ANY_SAMPLE_STATE,
                                                     DDS::ANY_VIEW_STATE,
                                                     DDS::ANY_INSTANCE_STATE);

  if (ret == DDS::RETCODE_OK) {
    if (info[0].valid_data)
      participant_data = data[0];

    else
      return DDS::RETCODE_NO_DATA;
  }

  return ret;
}

DDS::ReturnCode_t
DomainParticipantImpl::get_discovered_topics(
  DDS::InstanceHandleSeq & topic_handles)
{
  ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex,
                   guard,
                   this->handle_protector_,
                   DDS::RETCODE_ERROR);

  HandleMap::const_iterator itEnd = this->handles_.end();

  for (HandleMap::const_iterator iter = this->handles_.begin();
       iter != itEnd; ++iter) {
    GuidConverter converter(iter->first);

    if (converter.entityKind() == KIND_TOPIC) {

      // skip the ignored topic
      if (this->ignored_topics_.find(iter->first)
          != this->ignored_topics_.end ()) {
        continue;
      }

      push_back(topic_handles, iter->second);
    }
  }

  return DDS::RETCODE_OK;
}

DDS::ReturnCode_t
DomainParticipantImpl::get_discovered_topic_data(
  DDS::TopicBuiltinTopicData & topic_data,
  DDS::InstanceHandle_t topic_handle)
{
  {
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex,
                     guard,
                     this->handle_protector_,
                     DDS::RETCODE_ERROR);

    bool found = false;
    HandleMap::const_iterator itEnd = this->handles_.end();

    for (HandleMap::const_iterator iter = this->handles_.begin();
         iter != itEnd; ++iter) {
      GuidConverter converter(iter->first);

      if (topic_handle == iter->second
          && converter.entityKind() == KIND_TOPIC) {
        found = true;
        break;
      }
    }

    if (!found)
      return DDS::RETCODE_PRECONDITION_NOT_MET;
  }

  DDS::DataReader_var dr =
    bit_subscriber_->lookup_datareader(BUILT_IN_TOPIC_TOPIC);
  DDS::TopicBuiltinTopicDataDataReader_var bit_topic_dr =
    DDS::TopicBuiltinTopicDataDataReader::_narrow(dr);

  DDS::SampleInfoSeq info;
  DDS::TopicBuiltinTopicDataSeq data;
  DDS::ReturnCode_t ret =
    bit_topic_dr->read_instance(data,
                                info,
                                1,
                                topic_handle,
                                DDS::ANY_SAMPLE_STATE,
                                DDS::ANY_VIEW_STATE,
                                DDS::ANY_INSTANCE_STATE);

  if (ret == DDS::RETCODE_OK) {
    if (info[0].valid_data)
      topic_data = data[0];

    else
      return DDS::RETCODE_NO_DATA;
  }

  return ret;
}

#endif

DDS::ReturnCode_t
DomainParticipantImpl::enable()
{
  //According spec:
  // - Calling enable on an already enabled Entity returns OK and has no
  // effect.
  // - Calling enable on an Entity whose factory is not enabled will fail
  // and return PRECONDITION_NOT_MET.

  if (this->is_enabled()) {
    return DDS::RETCODE_OK;
  }

  DDS::DomainParticipantFactoryQos qos;

  if (this->factory_->get_qos(qos) != DDS::RETCODE_OK) {
    ACE_ERROR((LM_ERROR, ACE_TEXT("(%P|%t)DomainParticipantImpl::enable failed to")
               ACE_TEXT(" get factory qos\n")));
    return DDS::RETCODE_ERROR;
  }

  if (qos.entity_factory.autoenable_created_entities == 0) {
    return DDS::RETCODE_PRECONDITION_NOT_MET;
  }

  DDS::ReturnCode_t ret = this->set_enabled();

  if (monitor_) {
    monitor_->report();
  }
  if (TheServiceParticipant->monitor_) {
    TheServiceParticipant->monitor_->report();
  }

  if (ret == DDS::RETCODE_OK && !TheTransientKludge->is_enabled()) {
    Discovery_rch disc = TheServiceParticipant->get_discovery(this->domain_id_);
    this->bit_subscriber_ = disc->init_bit(this);
  }

  return ret;
}

RepoId
DomainParticipantImpl::get_id()
{
  return dp_id_;
}

std::string
DomainParticipantImpl::get_unique_id()
{
  return GuidConverter(dp_id_).uniqueId();
}


DDS::InstanceHandle_t
DomainParticipantImpl::get_instance_handle()
{
  return this->get_handle(this->dp_id_);
}

DDS::InstanceHandle_t
DomainParticipantImpl::get_handle(const RepoId& id)
{
  if (id == GUID_UNKNOWN) {
    return this->participant_handles_.next();
  }

  ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex,
                   guard,
                   this->handle_protector_,
                   HANDLE_UNKNOWN);

  HandleMap::const_iterator location = this->handles_.find(id);
  DDS::InstanceHandle_t result;

  if (location == this->handles_.end()) {
    // Map new handle in both directions
    result = this->participant_handles_.next();
    this->handles_[id] = result;
    this->repoIds_[result] = id;
  } else {
    result = location->second;
  }

  return result;
}

RepoId
DomainParticipantImpl::get_repoid(const DDS::InstanceHandle_t& handle)
{
  RepoId result = GUID_UNKNOWN;
  ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex,
                   guard,
                   this->handle_protector_,
                   GUID_UNKNOWN);
  RepoIdMap::const_iterator location = this->repoIds_.find(handle);
  if (location != this->repoIds_.end()) {
    result = location->second;
  }
  return result;
}

DDS::Topic_ptr
DomainParticipantImpl::create_new_topic(
  const RepoId topic_id,
  const char * topic_name,
  const char * type_name,
  const DDS::TopicQos & qos,
  DDS::TopicListener_ptr a_listener,
  const DDS::StatusMask & mask,
  OpenDDS::DCPS::TypeSupport_ptr type_support)
{
  ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex,
                   tao_mon,
                   this->topics_protector_,
                   DDS::Topic::_nil());

  /*
  TopicMap::mapped_type* entry = 0;

  if (Util::find(topics_, topic_name, entry) == 0) {
    entry->client_refs_ ++;
    return DDS::Topic::_duplicate(entry->pair_.obj_.in());
  }
  */

  TopicImpl* topic_servant = 0;

  ACE_NEW_RETURN(topic_servant,
                 TopicImpl(topic_id,
                           topic_name,
                           type_name,
                           type_support,
                           qos,
                           a_listener,
                           mask,
                           this),
                 DDS::Topic::_nil());

  if ((enabled_ == true)
      && (qos_.entity_factory.autoenable_created_entities == 1)) {
    topic_servant->enable();
  }

  DDS::Topic_ptr obj(topic_servant);

  // this object will also act as a guard against leaking the new TopicImpl
  RefCounted_Topic refCounted_topic(Topic_Pair(topic_servant, obj, NO_DUP));

  if (OpenDDS::DCPS::bind(topics_, topic_name, refCounted_topic) == -1) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("(%P|%t) ERROR: DomainParticipantImpl::create_topic, ")
               ACE_TEXT("%p \n"),
               ACE_TEXT("bind")));
    return DDS::Topic::_nil();
  }

  if (this->monitor_) {
    this->monitor_->report();
  }

  // the topics_ map has one reference and we duplicate to give
  // the caller another reference.
  return DDS::Topic::_duplicate(refCounted_topic.pair_.obj_.in());
}

int
DomainParticipantImpl::is_clean() const
{
  int sub_is_clean = subscribers_.empty();
  int topics_is_clean = topics_.size() == 0;

  if (!TheTransientKludge->is_enabled()) {
    // There are four topics and builtin topic subscribers
    // left.

    sub_is_clean = sub_is_clean == 0 ? subscribers_.size() == 1 : 1;
    topics_is_clean = topics_is_clean == 0 ? topics_.size() == 4 : 1;
  }

  return (publishers_.empty()
          && sub_is_clean == 1
          && topics_is_clean == 1);
}

void
DomainParticipantImpl::set_object_reference(const DDS::DomainParticipant_ptr& dp)
{
  if (!CORBA::is_nil(participant_objref_.in())) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("(%P|%t) ERROR: DomainParticipantImpl::set_object_reference, ")
               ACE_TEXT("This participant is already activated. \n")));
    return;
  }

  participant_objref_ = DDS::DomainParticipant::_duplicate(dp);
}

DDS::DomainParticipantListener_ptr
DomainParticipantImpl::listener_for(DDS::StatusKind kind)
{
  if (CORBA::is_nil(listener_.in()) || (listener_mask_ & kind) == 0) {
    return DDS::DomainParticipantListener::_nil ();
  } else {
    return DDS::DomainParticipantListener::_duplicate(listener_.in());
  }
}

void
DomainParticipantImpl::get_topic_ids(TopicIdVec& topics)
{
  ACE_GUARD(ACE_Recursive_Thread_Mutex,
            guard,
            this->topics_protector_);

  topics.reserve(topics_.size());
  for (TopicMap::iterator it(topics_.begin());
       it != topics_.end(); ++it) {
    topics.push_back(it->second.pair_.svt_->get_id());
  }
}

#ifndef OPENDDS_NO_OWNERSHIP_KIND_EXCLUSIVE

OwnershipManager*
DomainParticipantImpl::ownership_manager()
{
#if !defined (DDS_HAS_MINIMUM_BIT)

  DDS::DataReader_var dr =
    bit_subscriber_->lookup_datareader(BUILT_IN_PUBLICATION_TOPIC);
  DDS::PublicationBuiltinTopicDataDataReader_var bit_pub_dr =
    DDS::PublicationBuiltinTopicDataDataReader::_narrow(dr);

  if (!CORBA::is_nil(bit_pub_dr.in())) {
    DDS::DataReaderListener_var listener = bit_pub_dr->get_listener();
    if (CORBA::is_nil(listener.in())) {
      DDS::DataReaderListener_var bit_pub_listener =
        new BitPubListenerImpl(this);
      bit_pub_dr->set_listener(bit_pub_listener, DDS::DATA_AVAILABLE_STATUS);
      // Must call on_data_available when attaching a listener late - samples may be waiting
      bit_pub_listener->on_data_available(bit_pub_dr.in());
    }
  }

#endif
  return &this->owner_man_;
}

void
DomainParticipantImpl::update_ownership_strength (const PublicationId& pub_id,
                                                  const CORBA::Long& ownership_strength)
{
  ACE_GUARD(ACE_Recursive_Thread_Mutex,
            tao_mon,
            this->subscribers_protector_);

  if (this->get_deleted ())
    return;

  for (SubscriberSet::iterator it(this->subscribers_.begin());
      it != this->subscribers_.end(); ++it) {
    it->svt_->update_ownership_strength(pub_id, ownership_strength);
  }
}

#endif // OPENDDS_NO_OWNERSHIP_KIND_EXCLUSIVE

DomainParticipantImpl::RepoIdSequence::RepoIdSequence(RepoId& base) :
  base_(base),
  serial_(0),
  builder_(base_)
{
}

RepoId
DomainParticipantImpl::RepoIdSequence::next()
{
  builder_.entityKey(++serial_);
  return builder_;
}


////////////////////////////////////////////////////////////////


bool
DomainParticipantImpl::validate_publisher_qos(DDS::PublisherQos & pub_qos)
{
  if (pub_qos == PUBLISHER_QOS_DEFAULT) {
    this->get_default_publisher_qos(pub_qos);
  }

  OPENDDS_NO_OBJECT_MODEL_PROFILE_COMPATIBILITY_CHECK(pub_qos, false);

  if (!Qos_Helper::valid(pub_qos) || !Qos_Helper::consistent(pub_qos)) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("(%P|%t) ERROR: ")
               ACE_TEXT("DomainParticipantImpl::validate_publisher_qos, ")
               ACE_TEXT("invalid qos.\n")));
    return false;
  }

  return true;
}

bool
DomainParticipantImpl::validate_subscriber_qos(DDS::SubscriberQos & subscriber_qos)
{
  if (subscriber_qos == SUBSCRIBER_QOS_DEFAULT) {
    this->get_default_subscriber_qos(subscriber_qos);
  }

  OPENDDS_NO_OBJECT_MODEL_PROFILE_COMPATIBILITY_CHECK(subscriber_qos, false);

  if (!Qos_Helper::valid(subscriber_qos) || !Qos_Helper::consistent(subscriber_qos)) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("(%P|%t) ERROR: ")
               ACE_TEXT("DomainParticipantImpl::validate_subscriber_qos, ")
               ACE_TEXT("invalid qos.\n")));
    return false;
  }


  return true;
}

Recorder_rch DomainParticipantImpl::create_recorder(DDS::Topic_ptr a_topic,
                             const DDS::SubscriberQos & subscriber_qos,
                             const DDS::DataReaderQos & datareader_qos,
                             const RecorderListener_rch & a_listener,
                             DDS::StatusMask mask)
{
  if (CORBA::is_nil(a_topic)) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("(%P|%t) ERROR: ")
               ACE_TEXT("SubscriberImpl::create_datareader, ")
               ACE_TEXT("topic desc is nil.\n")));
    return Recorder_rch();
  }

  DDS::SubscriberQos sub_qos = subscriber_qos;
  DDS::DataReaderQos dr_qos;

  if (! this->validate_subscriber_qos(sub_qos) ||
      ! SubscriberImpl::validate_datareader_qos(datareader_qos,
                                                TheServiceParticipant->initial_DataReaderQos(),
                                                a_topic,
                                                dr_qos, false) ) {
    return Recorder_rch();
  }


  RecorderImpl* recorder = 0 ;
  ACE_NEW_RETURN(recorder,
                 RecorderImpl,
                 Recorder_rch());

  Recorder_rch result(recorder);

  recorder->init(dynamic_cast<TopicDescriptionImpl*>(a_topic),
    dr_qos, a_listener,
    mask, this, subscriber_qos);

  if ((enabled_ == true) && (qos_.entity_factory.autoenable_created_entities == 1)) {
    recorder->enable();
  }

  ACE_Guard<ACE_Recursive_Thread_Mutex> guard(recorders_protector_);
  recorders_.insert(result);

  return result;
}



Replayer_rch
DomainParticipantImpl::create_replayer(DDS::Topic_ptr a_topic,
                             const DDS::PublisherQos & publisher_qos,
                             const DDS::DataWriterQos & datawriter_qos,
                             const ReplayerListener_rch & a_listener,
                             DDS::StatusMask mask)
{

  if (CORBA::is_nil(a_topic)) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("(%P|%t) ERROR: ")
               ACE_TEXT("SubscriberImpl::create_datareader, ")
               ACE_TEXT("topic desc is nil.\n")));
    return Replayer_rch();
  }

  DDS::PublisherQos pub_qos = publisher_qos;
  DDS::DataWriterQos dw_qos;

  if (! this->validate_publisher_qos(pub_qos) ||
      ! PublisherImpl::validate_datawriter_qos(datawriter_qos,
                                               TheServiceParticipant->initial_DataWriterQos(),
                                               a_topic,
                                               dw_qos)) {
    return Replayer_rch();
  }

  TopicImpl* topic_servant = dynamic_cast<TopicImpl*>(a_topic);


  ReplayerImpl* replayer = 0;
  ACE_NEW_RETURN(replayer,
                 ReplayerImpl,
                 Replayer_rch());

  Replayer_rch result(replayer);
  replayer->init(a_topic,
                   topic_servant,
                   dw_qos,
                   a_listener,
                   mask,
                   this,
                   pub_qos);

  if (this->enabled_ == true
      && qos_.entity_factory.autoenable_created_entities == 1) {

    DDS::ReturnCode_t ret = replayer->enable();

    if (ret != DDS::RETCODE_OK) {
      ACE_ERROR((LM_ERROR,
                 ACE_TEXT("(%P|%t) ERROR: ")
                 ACE_TEXT("DomainParticipantImpl::create_replayer, ")
                 ACE_TEXT("enable failed.\n")));
      return Replayer_rch();
    }
  }

  ACE_Guard<ACE_Recursive_Thread_Mutex> guard(replayers_protector_);
  replayers_.insert(result);
  return result;
}

void
DomainParticipantImpl::delete_recorder(Recorder_rch recorder)
{
  ACE_Guard<ACE_Recursive_Thread_Mutex> guard(recorders_protector_);
  recorders_.erase(recorder);
}

void
DomainParticipantImpl::delete_replayer(Replayer_rch replayer)
{
  ACE_Guard<ACE_Recursive_Thread_Mutex> guard(replayers_protector_);
  replayers_.erase(replayer);
}

} // namespace DCPS
} // namespace OpenDDS
