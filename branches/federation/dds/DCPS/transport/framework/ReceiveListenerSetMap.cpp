// -*- C++ -*-
//
// $Id$
#include "DCPS/DdsDcps_pch.h" //Only the _pch include should start with DCPS/
#include "ReceiveListenerSetMap.h"
#include "dds/DCPS/Util.h"

#include <sstream>

#if !defined (__ACE_INLINE__)
#include "ReceiveListenerSetMap.inl"
#endif /* __ACE_INLINE__ */


OpenDDS::DCPS::ReceiveListenerSetMap::~ReceiveListenerSetMap()
{
  DBG_ENTRY_LVL("ReceiveListenerSetMap","~ReceiveListenerSetMap",6);
}


int
OpenDDS::DCPS::ReceiveListenerSetMap::insert
                                (RepoId                    publisher_id,
                                 RepoId                    subscriber_id,
                                 TransportReceiveListener* receive_listener)
{
  DBG_ENTRY_LVL("ReceiveListenerSetMap","insert",6);
  ReceiveListenerSet_rch listener_set = this->find_or_create(publisher_id);

  if (listener_set.is_nil())
    {
      // find_or_create failure
      std::stringstream buffer;
      long handle;
      handle = ::OpenDDS::DCPS::GuidConverter( publisher_id);
      buffer << publisher_id << "(" << std::hex << handle << ")";
      ACE_ERROR_RETURN((LM_ERROR,
        ACE_TEXT("(%P|%t) ERROR: ReceiveListenerSetMap::insert: ")
        ACE_TEXT("failed to find_or_create entry for ")
        ACE_TEXT("publisher %s.\n"),
        buffer.str().c_str()
      ), -1);
    }

  int result = listener_set->insert(subscriber_id, receive_listener);

  if (result == 0)
    {
      // Success.  Leave now.
      return 0;
    }

  // This is error handling code from here on out...

  // Handle the two possible failure cases (duplicate key or unknown)
  if (result == 1)
    {
      std::stringstream subscriberBuffer;
      long handle;
      handle = ::OpenDDS::DCPS::GuidConverter( subscriber_id);
      subscriberBuffer << subscriber_id << "(" << std::hex << handle << ")";

      std::stringstream publisherBuffer;
      handle = ::OpenDDS::DCPS::GuidConverter( publisher_id);
      publisherBuffer << publisher_id << "(" << std::hex << handle << ")";
      ACE_ERROR((LM_ERROR,
        ACE_TEXT("(%P|%t) ERROR: ReceiveListenerSetMap::insert: ")
        ACE_TEXT("subscriber %s already exists for ")
        ACE_TEXT("publisher %s.\n"),
        subscriberBuffer.str().c_str(),
        publisherBuffer.str().c_str()
      ));
    }
  else
    {
      std::stringstream subscriberBuffer;
      long handle;
      handle = ::OpenDDS::DCPS::GuidConverter( subscriber_id);
      subscriberBuffer << subscriber_id << "(" << std::hex << handle << ")";

      std::stringstream publisherBuffer;
      handle = ::OpenDDS::DCPS::GuidConverter( publisher_id);
      publisherBuffer << publisher_id << "(" << std::hex << handle << ")";
      ACE_ERROR((LM_ERROR,
        ACE_TEXT("(%P|%t) ERROR: ReceiveListenerSetMap::insert: ")
        ACE_TEXT("failed to insert subscriber %s for ")
        ACE_TEXT("publisher %s.\n"),
        subscriberBuffer.str().c_str(),
        publisherBuffer.str().c_str()
      ));
    }

  // Deal with possibility that the listener_set just got
  // created - and just for us.  This is to make sure we don't leave any
  // empty ReceiveListenerSets in our map_.
  if (listener_set->size() == 0)
    {
      listener_set = this->remove_set(publisher_id);

      if (listener_set.is_nil())
        {
          std::stringstream publisherBuffer;
          long handle;
          handle = ::OpenDDS::DCPS::GuidConverter( publisher_id);
          publisherBuffer << publisher_id << "(" << std::hex << handle << ")";
          ACE_ERROR((LM_ERROR,
            ACE_TEXT("(%P|%t) ERROR: ReceiveListenerSetMap::insert: ")
            ACE_TEXT("failed to remove (undo create) ReceiveListenerSet ")
            ACE_TEXT("for publisher %s.\n"),
            publisherBuffer.str().c_str()
          ));
        }
    }

  return -1;
}


int
OpenDDS::DCPS::ReceiveListenerSetMap::remove(RepoId publisher_id,
                                         RepoId subscriber_id)
{
  DBG_ENTRY_LVL("ReceiveListenerSetMap","remove",6);
  ReceiveListenerSet_rch listener_set;

  if (OpenDDS::DCPS::find(map_, publisher_id, listener_set) != 0)
    {
      return 0;
    }

  int result = listener_set->remove(subscriber_id);

  // Ignore the result
  ACE_UNUSED_ARG(result);

  if (listener_set->size() == 0)
    {
      if (unbind(map_, publisher_id) != 0)
        {
          std::stringstream publisherBuffer;
          long handle;
          handle = ::OpenDDS::DCPS::GuidConverter( publisher_id);
          publisherBuffer << publisher_id << "(" << std::hex << handle << ")";
          ACE_ERROR_RETURN((LM_ERROR,
            ACE_TEXT("(%P|%t) ERROR: ReceiveListenerSetMap::remove: ")
            ACE_TEXT("failed to remove empty ReceiveListenerSet for ")
            ACE_TEXT("publisher %s.\n"),
            publisherBuffer.str().c_str()
          ), -1);
        }
    }

  return 0;
}


//MJM: Other than funky return values, the previous and next methods
//MJM: appear to be identical.  Can't you implement remove(a,b) as
//MJM: "release_subscriber(a,b) ; return 0 ;"  Oh.  I guess it returns
//MJM: -1 from one spot as well.  Could the calling code be happy with
//MJM: either of these?  Is this to place where I found no return value
//MJM: of "1".  Could you have been meaning to call the other method?

/// This method is called when the (remote) subscriber is being
/// released.  This method will return a 0 if the subscriber_id is
/// successfully disassociated with the publisher_id *and* there
/// are still other subscribers associated with the publisher_id.
/// This method will return 1 if, after the disassociation, the
/// publisher_id is no longer associated with any subscribers (which
/// also means it's element was removed from our map_).
int
OpenDDS::DCPS::ReceiveListenerSetMap::release_subscriber(RepoId publisher_id,
                                                     RepoId subscriber_id)
{
  DBG_ENTRY_LVL("ReceiveListenerSetMap","release_subscriber",6);
  ReceiveListenerSet_rch listener_set;

  if (OpenDDS::DCPS::find(map_, publisher_id, listener_set) != 0)
    {
      std::stringstream publisherBuffer;
      long handle;
      handle = ::OpenDDS::DCPS::GuidConverter( publisher_id);
      publisherBuffer << publisher_id << "(" << std::hex << handle << ")";
      ACE_ERROR((LM_ERROR,
        ACE_TEXT("(%P|%t) ERROR: ReciveListenerSetMap::release_subscriber: ")
        ACE_TEXT("publisher %s not found in map_.\n"),
        publisherBuffer.str().c_str()
      ));
      // Return 1 to indicate that the publisher_id is no longer associated
      // with any subscribers at all.
      return 1;
    }

  int result = listener_set->remove(subscriber_id);

  // Ignore the result
  ACE_UNUSED_ARG(result);

  if (listener_set->size() == 0)
    {
      if (unbind(map_, publisher_id) != 0)
        {
          std::stringstream publisherBuffer;
          long handle;
          handle = ::OpenDDS::DCPS::GuidConverter( publisher_id);
          publisherBuffer << publisher_id << "(" << std::hex << handle << ")";
          ACE_ERROR((LM_ERROR,
            ACE_TEXT("(%P|%t) ERROR: ReceiveListenerSetMap::release_subscriber: ")
            ACE_TEXT("failed to remove empty ReceiveListenerSet for ")
            ACE_TEXT("publisher %s.\n"),
            publisherBuffer.str().c_str()
          ));
        }

      // We always return 1 if we know the publisher_id is no longer
      // associated with any ReceiveListeners.
      return 1;
    }

  // There are still ReceiveListeners associated with the publisher_id.
  // We return a 0 in this case.
  return 0;
}



void 
OpenDDS::DCPS::ReceiveListenerSetMap::operator= (const ReceiveListenerSetMap& rh)
{
  DBG_ENTRY_LVL("ReceiveListenerSetMap","operator=",6);
  const MapType& map = rh.map();

  for (MapType::const_iterator itr = map.begin();
    itr != map.end();
    ++itr)
  {
    ReceiveListenerSet_rch set = itr->second;
    ReceiveListenerSet::MapType& smap = set->map();
    for (ReceiveListenerSet::MapType::iterator sitr = smap.begin();
    sitr != smap.end();
    ++sitr)
    {
      this->insert (itr->first, sitr->first, sitr->second);
    }
  }
}


void 
OpenDDS::DCPS::ReceiveListenerSetMap::clear ()
{
  DBG_ENTRY_LVL("ReceiveListenerSetMap","clear",6);

  for (MapType::iterator itr = this->map_.begin();
    itr != this->map_.end();
    ++itr)
  {
    itr->second->clear ();
  }

  this->map_.clear ();
}


