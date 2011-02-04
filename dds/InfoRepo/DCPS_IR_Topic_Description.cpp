/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "DcpsInfo_pch.h"
#include /**/ "DCPS_IR_Topic_Description.h"

#include /**/ "DCPS_IR_Subscription.h"
#include /**/ "DCPS_IR_Publication.h"

#include /**/ "DCPS_IR_Topic.h"
#include /**/ "DCPS_IR_Domain.h"

#include /**/ "DCPS_Utils.h"

#include /**/ "tao/debug.h"

#include /**/ "dds/DCPS/RepoIdConverter.h"

DCPS_IR_Topic_Description::DCPS_IR_Topic_Description(DCPS_IR_Domain* domain,
                                                     const char* name,
                                                     const char* dataTypeName)
  : name_(name),
    dataTypeName_(dataTypeName),
    domain_(domain)
{
}

DCPS_IR_Topic_Description::~DCPS_IR_Topic_Description()
{
  // check for any subscriptions still referencing this description.
  if (0 != subscriptionRefs_.size()) {
    DCPS_IR_Subscription* subscription = 0;
    DCPS_IR_Subscription_Set::ITERATOR iter = subscriptionRefs_.begin();
    DCPS_IR_Subscription_Set::ITERATOR end = subscriptionRefs_.end();

    ACE_ERROR((LM_ERROR,
               ACE_TEXT("(%P|%t) ERROR: DCPS_IR_Topic_Description::~DCPS_IR_Topic_Description: ")
               ACE_TEXT("topic description name %C data type name %C has retained subscriptions.\n"),
               name_.c_str(),
               dataTypeName_.c_str()));

    while (iter != end) {
      subscription = *iter;
      ++iter;

      OpenDDS::DCPS::RepoIdConverter converter(subscription->get_id());
      ACE_ERROR((LM_ERROR,
                 ACE_TEXT("(%P|%t) ERROR: DCPS_IR_Topic_Description::~DCPS_IR_Topic_Description: ")
                 ACE_TEXT("topic description %C retains subscription %C.\n"),
                 this->name_.c_str(),
                 std::string(converter).c_str()));
    }
  }

  if (0 != topics_.size()) {
    DCPS_IR_Topic* topic = 0;
    DCPS_IR_Topic_Set::ITERATOR iter = topics_.begin();
    DCPS_IR_Topic_Set::ITERATOR end = topics_.end();

    ACE_ERROR((LM_ERROR,
               ACE_TEXT("(%P|%t) ERROR: DCPS_IR_Topic_Description::~DCPS_IR_Topic_Description: ")
               ACE_TEXT("topic description name %C data type name %C has retained topics.\n"),
               name_.c_str(),
               dataTypeName_.c_str()));

    while (iter != end) {
      topic = *iter;
      ++iter;

      OpenDDS::DCPS::RepoIdConverter converter(topic->get_id());
      ACE_ERROR((LM_ERROR,
                 ACE_TEXT("(%P|%t) ERROR: DCPS_IR_Topic_Description::~DCPS_IR_Topic_Description: ")
                 ACE_TEXT("topic description %C retains topic %C.\n"),
                 this->name_.c_str(),
                 std::string(converter).c_str()));
    }
  }
}

int DCPS_IR_Topic_Description::add_subscription_reference(DCPS_IR_Subscription* subscription
                                                          , bool associate)
{
  int status = subscriptionRefs_.insert(subscription);

  switch (status) {
  case 0:

    // Publish the BIT information
    domain_->publish_subscription_bit(subscription);

    if (associate) {
      try_associate_subscription(subscription);
      // Do not check incompatible qos here.  The check is done
      // in the DCPS_IR_Topic_Description::try_associate_subscription method
    }

    if (OpenDDS::DCPS::DCPS_debug_level > 0) {
      OpenDDS::DCPS::RepoIdConverter converter(subscription->get_id());
      ACE_DEBUG((LM_DEBUG,
                 ACE_TEXT("(%P|%t) DCPS_IR_Topic_Description::add_subscription_reference: ")
                 ACE_TEXT("topic description %C added subscription %C at %x\n"),
                 this->name_.c_str(),
                 std::string(converter).c_str(),
                 subscription));
    }

    break;

  case 1: {
    OpenDDS::DCPS::RepoIdConverter converter(subscription->get_id());
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("(%P|%t) ERROR: DCPS_IR_Topic_Description::add_subscription_reference: ")
               ACE_TEXT("topic description %C attempt to re-add subscription %C.\n"),
               this->name_.c_str(),
               std::string(converter).c_str()));
  }
  break;

  case -1: {
    OpenDDS::DCPS::RepoIdConverter converter(subscription->get_id());
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("(%P|%t) ERROR: DCPS_IR_Topic_Description::add_subscription_reference: ")
               ACE_TEXT("topic description %C failed to add subscription %C.\n"),
               this->name_.c_str(),
               std::string(converter).c_str()));
  }
  };

  return status;
}

int DCPS_IR_Topic_Description::remove_subscription_reference(DCPS_IR_Subscription* subscription)
{
  int status = subscriptionRefs_.remove(subscription);

  if (0 == status) {
    if (OpenDDS::DCPS::DCPS_debug_level > 0) {
      OpenDDS::DCPS::RepoIdConverter converter(subscription->get_id());
      ACE_DEBUG((LM_DEBUG,
                 ACE_TEXT("(%P|%t) DCPS_IR_Topic_Description::remove_subscription_reference: ")
                 ACE_TEXT("topic description %C removed subscription %C.\n"),
                 this->name_.c_str(),
                 std::string(converter).c_str()));
    }

  } else {
    OpenDDS::DCPS::RepoIdConverter converter(subscription->get_id());
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("(%P|%t) ERROR: DCPS_IR_Topic_Description::remove_subscription_reference: ")
               ACE_TEXT("topic description %C failed to remove subscription %C.\n"),
               this->name_.c_str(),
               std::string(converter).c_str()));
  } // if (0 == status)

  return status;
}

int DCPS_IR_Topic_Description::add_topic(DCPS_IR_Topic* topic)
{
  int status = topics_.insert(topic);

  switch (status) {
  case 0:

    if (OpenDDS::DCPS::DCPS_debug_level > 0) {
      OpenDDS::DCPS::RepoIdConverter converter(topic->get_id());
      ACE_DEBUG((LM_DEBUG,
                 ACE_TEXT("(%P|%t) DCPS_IR_Topic_Description::add_topic: ")
                 ACE_TEXT("topic description %C added topic %C at %x.\n"),
                 this->name_.c_str(),
                 std::string(converter).c_str(),
                 topic));
    }

    break;
  case 1:

    if (OpenDDS::DCPS::DCPS_debug_level > 0) {
      OpenDDS::DCPS::RepoIdConverter converter(topic->get_id());
      ACE_DEBUG((LM_WARNING,
                 ACE_TEXT("(%P|%t) WARNING: DCPS_IR_Topic_Description::add_topic: ")
                 ACE_TEXT("topic description %C attempt to re-add topic %C.\n"),
                 this->name_.c_str(),
                 std::string(converter).c_str()));
    }

    break;
  case -1: {
    OpenDDS::DCPS::RepoIdConverter converter(topic->get_id());
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("(%P|%t) ERROR: DCPS_IR_Topic_Description::add_topic: ")
               ACE_TEXT("topic description %C failed to add topic %C.\n"),
               this->name_.c_str(),
               std::string(converter).c_str()));
  }
  break;
  };

  return status;
}

int DCPS_IR_Topic_Description::remove_topic(DCPS_IR_Topic* topic)
{
  int status = topics_.remove(topic);

  if (0 == status) {
    if (OpenDDS::DCPS::DCPS_debug_level > 0) {
      OpenDDS::DCPS::RepoIdConverter converter(topic->get_id());
      ACE_DEBUG((LM_DEBUG,
                 ACE_TEXT("(%P|%t) DCPS_IR_Topic_Description::remove_topic: ")
                 ACE_TEXT("topic description %C removed topic %C.\n"),
                 this->name_.c_str(),
                 std::string(converter).c_str()));
    }

  } else {
    OpenDDS::DCPS::RepoIdConverter converter(topic->get_id());
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("(%P|%t) ERROR: DCPS_IR_Topic_Description::remove_topic: ")
               ACE_TEXT("topic description failed to remove topic %C.\n"),
               this->name_.c_str(),
               std::string(converter).c_str()));
  }

  return status;
}

DCPS_IR_Topic* DCPS_IR_Topic_Description::get_first_topic()
{
  DCPS_IR_Topic* topic = 0;

  if (0 < topics_.size()) {
    DCPS_IR_Topic_Set::ITERATOR iter = topics_.begin();
    topic = *iter;

    if (OpenDDS::DCPS::DCPS_debug_level > 0) {
      OpenDDS::DCPS::RepoIdConverter converter(topic->get_id());
      ACE_DEBUG((LM_DEBUG,
                 ACE_TEXT("(%P|%t) DCPS_IR_Topic_Description::get_first_topic: ")
                 ACE_TEXT("topic description %C first topic %C.\n"),
                 this->name_.c_str(),
                 std::string(converter).c_str()));
    }
  }

  return topic;
}

void DCPS_IR_Topic_Description::try_associate_publication(DCPS_IR_Publication* publication)
{
  // for each subscription check for compatiblity
  DCPS_IR_Subscription* subscription = 0;
  OpenDDS::DCPS::IncompatibleQosStatus* qosStatus = 0;

  DCPS_IR_Subscription_Set::ITERATOR iter = subscriptionRefs_.begin();
  DCPS_IR_Subscription_Set::ITERATOR end = subscriptionRefs_.end();

  while (iter != end) {
    subscription = *iter;
    ++iter;
    try_associate(publication, subscription);

    // Check the subscriptions QOS status
    qosStatus = subscription->get_incompatibleQosStatus();

    if (0 < qosStatus->count_since_last_send) {
      subscription->update_incompatible_qos();
    }
  }

  // Check the publications QOS status
  qosStatus = publication->get_incompatibleQosStatus();

  if (0 < qosStatus->count_since_last_send) {
    publication->update_incompatible_qos();
  }
}

void DCPS_IR_Topic_Description::try_associate_subscription(DCPS_IR_Subscription* subscription)
{
  // check all topics for compatible publications

  DCPS_IR_Topic* topic = 0;

  DCPS_IR_Topic_Set::ITERATOR iter = topics_.begin();
  DCPS_IR_Topic_Set::ITERATOR end = topics_.end();

  while (iter != end) {
    topic = *iter;
    ++iter;

    topic->try_associate(subscription);
  }

  // Check the subscriptions QOS status
  OpenDDS::DCPS::IncompatibleQosStatus* qosStatus =
    subscription->get_incompatibleQosStatus();

  if (0 < qosStatus->total_count) {
    if (OpenDDS::DCPS::DCPS_debug_level > 0) {
      OpenDDS::DCPS::RepoIdConverter converter(subscription->get_id());
      ACE_DEBUG((LM_DEBUG,
                 ACE_TEXT("(%P|%t) DCPS_IR_Topic_Description::try_associate_subscription: ")
                 ACE_TEXT("topic description %C has %d incompatible publications ")
                 ACE_TEXT("with subscription %C.\n"),
                 this->name_.c_str(),
                 qosStatus->total_count,
                 std::string(converter).c_str()));
    }

    subscription->update_incompatible_qos();
  }
}

bool
DCPS_IR_Topic_Description::try_associate(DCPS_IR_Publication* publication,
                                         DCPS_IR_Subscription* subscription)
{
  if (publication->is_subscription_ignored(subscription->get_participant_id(),
                                           subscription->get_topic_id(),
                                           subscription->get_id())) {
    if (OpenDDS::DCPS::DCPS_debug_level > 0) {
      OpenDDS::DCPS::RepoIdConverter pub_converter(publication->get_id());
      OpenDDS::DCPS::RepoIdConverter sub_converter(subscription->get_id());
      ACE_DEBUG((LM_DEBUG,
                 ACE_TEXT("(%P|%t) DCPS_IR_Topic_Description::try_associate: ")
                 ACE_TEXT("topic description %C publication %C ignores subscription %C.\n"),
                 this->name_.c_str(),
                 std::string(pub_converter).c_str(),
                 std::string(sub_converter).c_str()));
    }
  }

  else if (subscription->is_publication_ignored(publication->get_participant_id(),
                                                publication->get_topic_id(),
                                                publication->get_id())) {
    if (OpenDDS::DCPS::DCPS_debug_level > 0) {
      OpenDDS::DCPS::RepoIdConverter pub_converter(publication->get_id());
      OpenDDS::DCPS::RepoIdConverter sub_converter(subscription->get_id());
      ACE_DEBUG((LM_DEBUG,
                 ACE_TEXT("(%P|%t) DCPS_IR_Topic_Description::try_associate: ")
                 ACE_TEXT("topic description %C subscription %C ignores publication %C.\n"),
                 this->name_.c_str(),
                 std::string(pub_converter).c_str(),
                 std::string(sub_converter).c_str()));
    }

  } else {
    if (OpenDDS::DCPS::DCPS_debug_level > 0) {
      OpenDDS::DCPS::RepoIdConverter pub_converter(publication->get_id());
      OpenDDS::DCPS::RepoIdConverter sub_converter(subscription->get_id());
      ACE_DEBUG((LM_DEBUG,
                 ACE_TEXT("(%P|%t) DCPS_IR_Topic_Description::try_associate: ")
                 ACE_TEXT("topic description %C checking compatibility of ")
                 ACE_TEXT("publication %C with subscription %C.\n"),
                 this->name_.c_str(),
                 std::string(pub_converter).c_str(),
                 std::string(sub_converter).c_str()));
    }

    if (compatibleQOS(publication, subscription)) {
      associate(publication, subscription);
      return true;
    }

    // Dont notify that there is an incompatible qos here
    // notify where we can distinguish which one is being added
    // so we only send one response(with all incompatible qos) to it
  }

  return false;
}

void DCPS_IR_Topic_Description::associate(DCPS_IR_Publication* publication,
                                          DCPS_IR_Subscription* subscription)
{
  if (OpenDDS::DCPS::DCPS_debug_level > 0) {
    OpenDDS::DCPS::RepoIdConverter pub_converter(publication->get_id());
    OpenDDS::DCPS::RepoIdConverter sub_converter(subscription->get_id());
    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("(%P|%t) DCPS_IR_Topic_Description::associate: ")
               ACE_TEXT("topic description %C associating ")
               ACE_TEXT("publication %C with subscription %C.\n"),
               this->name_.c_str(),
               std::string(pub_converter).c_str(),
               std::string(sub_converter).c_str()));
  }

  // The publication must be told first because it will be the connector
  // if a data link needs to be created.
  // This is only required if the publication and subscription are being
  // handed by the same process and thread.  Order when there is
  // another thread or process is not important.
  // Note: the client thread may process the add_associations() oneway
  //       call instead of the ORB thread because it is currently
  //       in a two-way call to the Repo.
  publication->add_associated_subscription(subscription);
  subscription->add_associated_publication(publication);

  //TBD - these called methods currenlty tell the associated publication and
  //      subscription about the association immediately.
  //      It would be better if the associations were sent after all
  //      associations were added.  This way the publication(s) and
  //      subscription(s) could recieve a list of assocations instead
  //      of multiple one at a time associations.
}

void DCPS_IR_Topic_Description::reevaluate_associations(DCPS_IR_Subscription* subscription)
{
  DCPS_IR_Topic* topic = 0;

  DCPS_IR_Topic_Set::ITERATOR iter = topics_.begin();
  DCPS_IR_Topic_Set::ITERATOR end = topics_.end();

  while (iter != end) {
    topic = *iter;
    ++iter;

    topic->reevaluate_associations(subscription);
  }
}

void DCPS_IR_Topic_Description::reevaluate_associations(DCPS_IR_Publication* publication)
{
  DCPS_IR_Subscription * sub = 0;
  DCPS_IR_Subscription_Set::ITERATOR iter = subscriptionRefs_.begin();
  DCPS_IR_Subscription_Set::ITERATOR end = subscriptionRefs_.end();

  while (iter != end) {
    sub = *iter;
    ++iter;
    publication->reevaluate_association(sub);
    sub->reevaluate_association(publication);
  }
}

const char* DCPS_IR_Topic_Description::get_name() const
{
  return name_.c_str();
}

const char* DCPS_IR_Topic_Description::get_dataTypeName() const
{
  return dataTypeName_.c_str();
}

CORBA::ULong DCPS_IR_Topic_Description::get_number_topics() const
{
  return topics_.size();
}

#if defined (ACE_HAS_EXPLICIT_TEMPLATE_INSTANTIATION)

template class ACE_Node<DCPS_IR_Subscription*>;
template class ACE_Unbounded_Set<DCPS_IR_Subscription*>;
template class ACE_Unbounded_Set_Iterator<DCPS_IR_Subscription*>;

template class ACE_Node<DCPS_IR_Topic*>;
template class ACE_Unbounded_Set<DCPS_IR_Topic*>;
template class ACE_Unbounded_Set_Iterator<DCPS_IR_Topic*>;

#elif defined (ACE_HAS_TEMPLATE_INSTANTIATION_PRAGMA)

#pragma instantiate ACE_Node<DCPS_IR_Subscription*>
#pragma instantiate ACE_Unbounded_Set<DCPS_IR_Subscription*>
#pragma instantiate ACE_Unbounded_Set_Iterator<DCPS_IR_Subscription*>

#pragma instantiate ACE_Node<DCPS_IR_Topic*>
#pragma instantiate ACE_Unbounded_Set<DCPS_IR_Topic*>
#pragma instantiate ACE_Unbounded_Set_Iterator<DCPS_IR_Topic*>

#endif /* ACE_HAS_EXPLICIT_TEMPLATE_INSTANTIATION */
