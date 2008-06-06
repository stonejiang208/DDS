// $Id$

#include "DCPS/DdsDcps_pch.h" //Only the _pch include should start with DCPS/
#include "DataDurabilityCache.h"
#include "Service_Participant.h"
#include "DataSampleList.h"
#include "WriteDataContainer.h"
#include "DataWriterImpl.h"
#include "Qos_Helper.h"
#include "debug.h"

#include "tao/ORB_Core.h"

#include "ace/Reactor.h"
#include "ace/Message_Block.h"
#include "ace/Log_Msg.h"
#include "ace/Malloc_T.h"
#include "ace/MMAP_Memory_Pool.h"
#include "ace/OS_NS_sys_time.h"

#include <sstream>
#include <string>
#include <algorithm>

// --------------------------------------------------
namespace
{
  /**
   * @class Cleanup_Handler
   *
   * @brief Event handler that is called when @c service_cleanup_delay
   *        period expires.
   */
  class Cleanup_Handler : public ACE_Event_Handler
  {
  public:

    typedef
    ::OpenDDS::DCPS::DataDurabilityCache::sample_data_type data_type;
    typedef
    ::OpenDDS::DCPS::DataDurabilityCache::sample_list_type list_type;

    Cleanup_Handler (list_type & sample_list,
                     list_type::difference_type index,
                     ACE_Allocator * allocator)
      : sample_list_ (sample_list)
      , index_ (index)
      , allocator_ (allocator)
      , tid_ (-1)
      , timer_ids_ (0)
    {
      this->reference_counting_policy ().value (
        ACE_Event_Handler::Reference_Counting_Policy::ENABLED);
    }

    virtual int handle_timeout (ACE_Time_Value const & /* current_time */,
                                void const * /* act */)
    {
      if (OpenDDS::DCPS::DCPS_debug_level >= 4)
      {
        ACE_DEBUG ((LM_DEBUG,
                    ACE_TEXT ("(%P|%t) OpenDDS - Cleaning up ")
                    ACE_TEXT ("data durability cache.\n")));
      }

      typedef ACE_Unbounded_Queue<
        ::OpenDDS::DCPS::DataDurabilityCache::sample_data_type>
        data_queue_type;

      // Cleanup all data samples corresponding to the cleanup delay.
      data_queue_type *& queue = this->sample_list_[this->index_];
      ACE_DES_FREE (queue,
                    this->allocator_->free,
                    data_queue_type);
      queue = 0;

      // No longer any need to keep track of the timer ID.
      this->timer_ids_->remove (this->tid_);

      return 0;
    }

    void timer_id (
      long tid,
      OpenDDS::DCPS::DataDurabilityCache::timer_id_list_type * timer_ids)
    {
      this->tid_ = tid;
      this->timer_ids_ = timer_ids;
    }

  protected:

    virtual ~Cleanup_Handler() {}

  private:

    /// List containing samples to be cleaned up when the cleanup timer
    /// expires.
    list_type & sample_list_;

    /// Location in list/array of queue to be deallocated.
    list_type::difference_type index_;

    /// Allocator to be used when deallocating data queue.
    ACE_Allocator * allocator_;

    /// Timer ID corresponding to this cleanup event handler.
    long tid_;

    /// List of timer IDs.
    /**
     * If the cleanup timer fires successfully, the timer ID must be
     * removed from the timer ID list so that a subsequent attempt to
     * cancel the timer during durability cache destruction does not
     * occur.
     */
    OpenDDS::DCPS::DataDurabilityCache::timer_id_list_type *
    timer_ids_;

  };
}

// --------------------------------------------------

OpenDDS::DCPS
::DataDurabilityCache::sample_data_type::sample_data_type ()
  : length_ (0)
  , sample_ (0)
  , allocator_ (0)
{
  this->source_timestamp_.sec = 0;
  this->source_timestamp_.nanosec = 0;
}

OpenDDS::DCPS
::DataDurabilityCache::sample_data_type::sample_data_type (
  DataSampleListElement & element,
  ACE_Allocator * a)
  : length_ (0)
  , sample_ (0)
  , allocator_ (a)
{
  this->source_timestamp_.sec     = element.source_timestamp_.sec;
  this->source_timestamp_.nanosec = element.source_timestamp_.nanosec;

  // Only copy the data provided by the user.  The DataSampleHeader
  // will be reconstructed when the durable data is retrieved by a
  // DataWriterImpl instance.
  //
  // The user's data is stored in the first message block
  // continuation.
  ACE_Message_Block const * const data = element.sample_->cont ();
  this->length_ = data->total_length ();

  ACE_ALLOCATOR (this->sample_,
                 static_cast<char *> (
                   this->allocator_->malloc (this->length_)));

  char * buf = this->sample_;

  for (ACE_Message_Block const * i = data;
       i != 0;
       i = i->cont ())
  {
    ACE_OS::memcpy (buf, i->rd_ptr (), i->length ());
    buf += i->length ();
  }
}


OpenDDS::DCPS
::DataDurabilityCache::sample_data_type::sample_data_type (
  sample_data_type const & rhs)
  : length_ (rhs.length_)
  , sample_ (0)
  , allocator_ (rhs.allocator_)
{
  this->source_timestamp_.sec     = rhs.source_timestamp_.sec;
  this->source_timestamp_.nanosec = rhs.source_timestamp_.nanosec;

  ACE_ALLOCATOR (this->sample_,
                 static_cast<char *> (this->allocator_->malloc (rhs.length_)));
  ACE_OS::memcpy (this->sample_, rhs.sample_, rhs.length_);
}

OpenDDS::DCPS
::DataDurabilityCache::sample_data_type::~sample_data_type ()
{
  this->allocator_->free (this->sample_);
}


OpenDDS::DCPS::DataDurabilityCache::sample_data_type &
OpenDDS::DCPS::DataDurabilityCache::sample_data_type::operator= (
  sample_data_type const & rhs)
{
  // Strongly exception-safe copy assignment.
  sample_data_type tmp (rhs);
  std::swap (this->length_, tmp.length_);
  std::swap (this->sample_, tmp.sample_);
  std::swap (this->allocator_, tmp.allocator_);

  this->source_timestamp_.sec     = rhs.source_timestamp_.sec;
  this->source_timestamp_.nanosec = rhs.source_timestamp_.nanosec;

  return *this;
}

void
OpenDDS::DCPS::DataDurabilityCache::sample_data_type::get_sample (
  char const *& s,
  size_t & len,
  ::DDS::Time_t & source_timestamp)
{
  s = this->sample_;
  len = this->length_;
  source_timestamp.sec     = this->source_timestamp_.sec;
  source_timestamp.nanosec = this->source_timestamp_.nanosec;
}


// --------------------------------------------------

OpenDDS::DCPS::DataDurabilityCache::DataDurabilityCache (
  ::DDS::DurabilityQosPolicyKind kind,
  ::DDS::DomainId_t domain_id)
  : allocator_ (make_allocator (kind, domain_id))
  , samples_ (allocator_.get (), allocator_.get ())
  , cleanup_timer_ids_ ()
  , lock_ ()
  , reactor_ (0)
{
  CORBA::ORB_var orb = TheServiceParticipant->get_ORB ();
  this->reactor_ = orb->orb_core ()->reactor ();
}

OpenDDS::DCPS::DataDurabilityCache::~DataDurabilityCache ()
{
  // Cancel timers that haven't expired yet.
  timer_id_list_type::const_iterator const end (
    this->cleanup_timer_ids_.end ());
  for (timer_id_list_type::const_iterator i (
         this->cleanup_timer_ids_.begin ());
       i != end;
       ++i)
  {
    (void) this->reactor_->cancel_timer (*i);
  }

  // Clean up memory that isn't automatically managed.
  if (this->allocator_.get () != 0)
  {
    sample_map_type::iterator const map_end = this->samples_.end ();
    for (sample_map_type::iterator s = this->samples_.begin ();
         s != map_end;
         ++s)
    {
      sample_list_type * const list = (*s).int_id_;

      sample_list_type::iterator const list_end = list->end ();
      for (sample_list_type::iterator l = list->begin ();
           l != list_end;
           ++l)
      {
        ACE_DES_FREE ((*l),
                      this->allocator_->free,
                      ACE_Unbounded_Queue<sample_data_type>);
      }

      ACE_DES_FREE (list,
                    this->allocator_->free,
                    sample_list_type);
    }
  }
}

bool
OpenDDS::DCPS::DataDurabilityCache::insert (
  char const * topic_name,
  char const * type_name,
  DataSampleList & unsent_data,
  ::DDS::DurabilityServiceQosPolicy const & qos)
{
  if (unsent_data.size_ == 0)
    return true;  // Nothing to cache.

  // Apply DURABILITY_SERVICE QoS HISTORY and RESOURCE_LIMITS related
  // settings prior to data insertion into the cache.
  CORBA::Long const depth =
    get_instance_sample_list_depth (
      qos.history_kind,
      qos.history_depth,
      qos.max_samples_per_instance);

  // Iterator to first DataSampleListElement to be copied.
  DataSampleList::iterator element (unsent_data.begin ());

  if (depth < 0)
    return false; // Should never occur.
  else if (depth == 0)
    return true;  // Nothing else to do.  Discard all data.
  else if (unsent_data.size_ > depth)
  { 
    // Drop "old" samples.  Only keep the "depth" most recent
    // samples, i.e. those found at the tail end of the
    // DataSampleList.
    ssize_t const advance_amount = unsent_data.size_ - depth;
    std::advance (element, advance_amount);      
  }

  // -----------

  // Copy unsent samples to the domain/topic/type-specific cache.

  key_type const key (topic_name, type_name, this->allocator_.get ());
  DataSampleList::iterator unsent_end (unsent_data.end ());

  sample_list_type * sample_list = 0;
  sample_list_type::iterator slot;

  typedef ACE_Unbounded_Queue<sample_data_type> data_queue_type;
  data_queue_type * samples = 0;  // sample_list_type::value_type

  {
    ACE_GUARD_RETURN (ACE_SYNCH_MUTEX, guard, this->lock_, false);

    sample_map_type::value_type * entry = 0;
    if (this->samples_.find (key, entry) == 0)
    {
      sample_list = entry->int_id_;
    }
    else
    {
      // Create a new list (actually an ACE_Array_Base<>) with the
      // appropriate allocator passed to its constructor.
      ACE_NEW_MALLOC_RETURN (
        sample_list,
        static_cast<sample_list_type *> (
          this->allocator_->malloc (sizeof (sample_list_type))),
        sample_list_type (0, this->allocator_.get ()),
        false);

      if (this->samples_.bind (key, sample_list) != 0)
        return false;
    }

    // Find an empty slot in the array.  This is a linear search but
    // that should be fine for the common case, i.e. a small number of
    // DataWriters that push unsent data into the cache.x
    slot = std::find (sample_list->begin (),
                      sample_list->end (),
                      static_cast<sample_list_type::value_type> (0));

    if (slot == sample_list->end ())
    {
      // No available slots.  Grow the array accordingly.
      sample_list_type::size_type const old_len = sample_list->size ();
      sample_list->size (old_len + 1);

      slot = sample_list->begin () + old_len;
    }

    ACE_NEW_MALLOC_RETURN (
      samples,
      static_cast<data_queue_type *> (
        this->allocator_->malloc (sizeof (data_queue_type))),
      data_queue_type (this->allocator_.get ()),
      false);

    // Insert the samples in to the sample list.
    *slot = samples;

    for (DataSampleList::iterator i (element); i != unsent_end; ++i)
      if (samples->enqueue_tail (
            sample_data_type (*i,
                              this->allocator_.get ())) != 0)
        return false;
  }

  // -----------

  // Schedule cleanup timer.
  Cleanup_Handler * const cleanup =
    new Cleanup_Handler (*sample_list,
                         std::distance (sample_list->begin (), slot),
                         this->allocator_.get ());
  ACE_Event_Handler_var safe_cleanup (cleanup);  // Transfer ownership

  ACE_Time_Value const cleanup_delay (
    duration_to_time_value (qos.service_cleanup_delay));

  if (cleanup_delay > ACE_Time_Value::zero)
  {
    long const tid =
      this->reactor_->schedule_timer (cleanup,
                                      0, // ACT
                                      cleanup_delay);

    if (tid == -1)
    {
      ACE_GUARD_RETURN (ACE_SYNCH_MUTEX, guard, this->lock_, false);

      ACE_DES_FREE (samples,
                    this->allocator_->free,
                    ACE_Unbounded_Queue<sample_data_type>);
      *slot = 0;

      return false;
    }
    else
    {
      {
        ACE_GUARD_RETURN (ACE_SYNCH_MUTEX, guard, this->lock_, false);
        this->cleanup_timer_ids_.push_back (tid);
      }

      cleanup->timer_id (tid,
                         &this->cleanup_timer_ids_);
    }
  }

  return true;
}

bool
OpenDDS::DCPS::DataDurabilityCache::get_data (
  char const * topic_name,
  char const * type_name,
  DataWriterImpl * data_writer,
  ACE_Allocator * mb_allocator,
  ACE_Allocator * db_allocator,
  ::DDS::LifespanQosPolicy const & /* lifespan */)
{

  key_type const key (topic_name, type_name, this->allocator_.get ());

  ACE_GUARD_RETURN (ACE_SYNCH_MUTEX, guard, this->lock_, false);

  sample_list_type * p_sample_list = 0;
  if (this->samples_.find (key, p_sample_list) == -1)
    return true;  // No durable data for this domain/topic/type.
  else if (p_sample_list == 0)
    return false; // Should never happen.

  sample_list_type & sample_list = *p_sample_list;

  // We will register an instance, and then write all of the cached
  // data to the DataWriter using that instance.

  sample_data_type * registration_data = 0;
  if (sample_list[0]->get (registration_data, 0) == -1)
    return false;

  char const * marshaled_sample = 0;
  size_t marshaled_sample_length = 0;
  ::DDS::Time_t registration_timestamp;

  registration_data->get_sample (marshaled_sample,
                                 marshaled_sample_length,
                                 registration_timestamp);


  // Don't use the cached allocator for the registered sample message
  // block.
  std::auto_ptr<DataSample> registration_sample (
    new ACE_Message_Block (marshaled_sample_length));

  ACE_OS::memcpy (registration_sample->wr_ptr (),
                  marshaled_sample,
                  marshaled_sample_length);

  registration_sample->wr_ptr (marshaled_sample_length);

  ::DDS::InstanceHandle_t handle = ::DDS::HANDLE_NIL;

  /**
   * @todo Is this going to cause problems for users that set a finite
   *       DDS::ResourceLimitsQosPolicy::max_instances value when
   *       OpenDDS supports that value?
   */
  ::DDS::ReturnCode_t ret =
      data_writer->register_instance (handle,
                                      registration_sample.get (),
                                      registration_timestamp);
  
  if (ret != ::DDS::RETCODE_OK)
    return false;

  registration_sample.release ();

  sample_list_type::iterator const end = sample_list.end ();
  for (sample_list_type::iterator i = sample_list.begin ();
       i != end;
       ++i)
  {
    typedef sample_list_type::value_type value_type;
    value_type q = *i;

    for (ACE_Unbounded_Queue<sample_data_type>::ITERATOR j = q->begin ();
         !j.done ();
         j.advance ())
    {
      sample_data_type * data = 0;
      if (j.next (data) == 0)
        return false;  // Should never happen.

      char const * sample = 0;  // Sample does not include header.
      size_t sample_length = 0;
      ::DDS::Time_t source_timestamp;

      data->get_sample (sample, sample_length, source_timestamp);

      ACE_Message_Block * mb = 0;
      ACE_NEW_MALLOC_RETURN (mb,
                             static_cast<ACE_Message_Block*> (
                               mb_allocator->malloc (
                                 sizeof (ACE_Message_Block))),
                             ACE_Message_Block(
                               sample_length,
                               ACE_Message_Block::MB_DATA,
                               0, // cont
                               0, // data
                               0, // allocator_strategy
                               0, // data block locking_strategy
                               ACE_DEFAULT_MESSAGE_BLOCK_PRIORITY,
                               ACE_Time_Value::zero,
                               ACE_Time_Value::max_time,
                               db_allocator,
                               mb_allocator),
                             false);

      ACE_OS::memcpy (mb->wr_ptr (),
                      sample,
                      sample_length);
      mb->wr_ptr (sample_length);

      if (data_writer->write (mb,
                              handle,
                              source_timestamp) == ::DDS::RETCODE_OK)
      {
        // Data successfully written.  Remove it from the cache.
        sample_data_type data;
        (void) q->dequeue_head (data);
      }
      else
      {
        ACE_DES_FREE (mb,
                      mb_allocator->free,
                      ACE_Message_Block);
        return false;
      }
    }
  }

  return true;
}

std::auto_ptr<ACE_Allocator>
OpenDDS::DCPS::DataDurabilityCache::make_allocator (
  ::DDS::DurabilityQosPolicyKind kind,
  ::DDS::DomainId_t domain_id)
{
  if (kind == ::DDS::PERSISTENT_DURABILITY_QOS)
    {
      typedef
        ACE_Allocator_Adapter<
          ACE_Malloc<ACE_MMAP_MEMORY_POOL, ACE_SYNCH_MUTEX> > allocator_type;

      static size_t const minimum_bytes = 1024;

      // @note Each persistent DDS domain will have its own
      //       mmap()-based allocator.  Do we need to worry about the
      //       same base address value being used for all?  Probably
      //       not since mmap() should return an unused region
      //       starting at a different base address.
      static ACE_MMAP_Memory_Pool_Options const pool_options (
        ACE_DEFAULT_BASE_ADDR,
        ACE_MMAP_Memory_Pool_Options::ALWAYS_FIXED,
        false, // No need to sync each page.
        minimum_bytes,
        MAP_SHARED, // Written data must be reflected in the backing
                    // store.  Fortunately, updates to the backing
                    // store until sync()ing occurs or the memory
                    // region is unmapped - the behavior desired!
        true,  // Guess on fault.
        0,   // Windows LPSECURITY_ATTRIBUTES
        /* 0 */ ACE_DEFAULT_FILE_PERMS,
        false); // Do not generate an unique pool for each mmap.

      std::stringstream bss;
      bss << "OpenDDS-durable-data-" << domain_id;
      char const * const backing_store = bss.str ().c_str ();

      return
        std::auto_ptr<ACE_Allocator> (new allocator_type (backing_store,
                                                          0,
                                                          &pool_options));
    }

  // kind == ::DDS::TRANSIENT_DURABILITY_QOS)
  return std::auto_ptr<ACE_Allocator> (new ACE_New_Allocator);
}
