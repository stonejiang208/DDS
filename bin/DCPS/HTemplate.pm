# -*- C++ -*-
# HTemplate.pm - template for generating header implementation file for
#                DDS TypeSupport.  The following macros are
#                substituted when generating the output file:
#
#   <%TYPE%>           - Type requiring support in DDS.
#   <%UPPERTYPE%>      - Uppercase version of <%TYPE%>
#   <%MODULE%>         - Module containing the type.
#   <%SCOPE%>          - Enclosing scope of type.
#   <%POA%>            - POA scope to inherit from.
#   <%EXPORT%>         - Export macro.
#   <%NAMESPACESTART%> - Beginning of namespace.
#   <%NAMESPACEEND%>   - End of namespace.
#
package DCPS::HTemplate ;

use warnings ;
use strict ;

sub contents { return <<'!EOT'
// -*- C++ -*-
//
// $Id$

// Generated by dcps_ts.pl

#ifndef <%UPPERTYPE%>TYPESUPPORTI_H_
#define <%UPPERTYPE%>TYPESUPPORTI_H_

#include "<%TYPE%>TypeSupportS.h"
#include "dds/DCPS/DataWriterImpl.h"
#include "dds/DCPS/DataReaderImpl.h"
#include "dds/DCPS/Dynamic_Cached_Allocator_With_Overflow_T.h"
#include "dds/DCPS/DataBlockLockPool.h"

#if !defined (ACE_LACKS_PRAGMA_ONCE)
#pragma once
#endif /* ACE_LACKS_PRAGMA_ONCE */

<%NAMESPACESTART%>
/** Servant for TypeSuport interface of <%TYPE%> data type.
 *
 * See the DDS specification, OMG formal/04-12-02, for a description of
 * this interface.
 *
 */
class <%EXPORT%> <%TYPE%>TypeSupportImpl
  : public virtual TAO::DCPS::LocalObject<<%TYPE%>TypeSupport>
{
public:

  //Constructor
  <%TYPE%>TypeSupportImpl (void);

  //Destructor
  virtual ~<%TYPE%>TypeSupportImpl (void);

  virtual
  DDS::ReturnCode_t register_type (
      ::DDS::DomainParticipant_ptr participant,
      const char * type_name
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
    ));

  virtual
  char * get_type_name (
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
    ));

  virtual
  ::DDS::DataWriter_ptr create_datawriter (
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
    ));

  virtual
  ::DDS::DataReader_ptr create_datareader (
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
    ));

  private:
    CORBA::String_var type_name_;
};
<%NAMESPACEEND%>

<%NAMESPACESTART%>

/** Servant for DataWriter interface of the <%TYPE%> data type.
 *
 * See the DDS specification, OMG formal/04-12-02, for a description of
 * this interface.
 */
class <%EXPORT%> <%TYPE%>DataWriterImpl
  : public virtual TAO::DCPS::LocalObject<<%TYPE%>DataWriter>,
    public virtual TAO::DCPS::DataWriterImpl
{
public:

  typedef std::map<<%SCOPE%><%TYPE%>, DDS::InstanceHandle_t,
      <%TYPE%>KeyLessThan> InstanceMap;
  typedef ::TAO::DCPS::Dynamic_Cached_Allocator_With_Overflow<ACE_Null_Mutex>  DataAllocator;

  //Constructor
  <%TYPE%>DataWriterImpl (void);

  //Destructor
  virtual ~<%TYPE%>DataWriterImpl (void);

  virtual
  DDS::InstanceHandle_t _cxx_register (
      const ::<%SCOPE%><%TYPE%> & instance_data
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
    ));

  virtual
  DDS::InstanceHandle_t register_w_timestamp (
      const ::<%SCOPE%><%TYPE%> & instance_data,
      ::DDS::InstanceHandle_t handle,
      const ::DDS::Time_t & source_timestamp
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
    ));

  virtual
  DDS::ReturnCode_t unregister (
      const ::<%SCOPE%><%TYPE%> & instance_data,
      ::DDS::InstanceHandle_t handle
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
    ));

  virtual
  DDS::ReturnCode_t unregister_w_timestamp (
      const ::<%SCOPE%><%TYPE%> & instance_data,
      ::DDS::InstanceHandle_t handle,
      const ::DDS::Time_t & source_timestamp
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
    ));

  //WARNING: If the handle is non-nil and the instance is not registered
  //         then this operation may cause an access violation.
  //         This lack of safety helps performance.
  virtual
  DDS::ReturnCode_t write (
      const ::<%SCOPE%><%TYPE%> & instance_data,
      ::DDS::InstanceHandle_t handle
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
    ));

  //WARNING: If the handle is non-nil and the instance is not registered
  //         then this operation may cause an access violation.
  //         This lack of safety helps performance.
  virtual
  DDS::ReturnCode_t write_w_timestamp (
      const ::<%SCOPE%><%TYPE%> & instance_data,
      ::DDS::InstanceHandle_t handle,
      const ::DDS::Time_t & source_timestamp
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
    ));

  virtual
  DDS::ReturnCode_t dispose (
      const ::<%SCOPE%><%TYPE%> & instance_data,
      ::DDS::InstanceHandle_t instance_handle
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
    ));

  virtual
  DDS::ReturnCode_t dispose_w_timestamp (
      const ::<%SCOPE%><%TYPE%> & instance_data,
      ::DDS::InstanceHandle_t instance_handle,
      const ::DDS::Time_t & source_timestamp
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
    ));

  virtual
  DDS::ReturnCode_t get_key_value (
      ::<%SCOPE%><%TYPE%> & key_holder,
      ::DDS::InstanceHandle_t handle
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
    ));


  /**
   * Initialize the DataWriter object.
   * Called as part of create_datawriter.
   */
  virtual void init (
        ::DDS::Topic_ptr                       topic,
        TAO::DCPS::TopicImpl                 *topic_servant,
        const ::DDS::DataWriterQos &           qos,
        ::DDS::DataWriterListener_ptr          a_listener,
        TAO::DCPS::DomainParticipantImpl*      participant_servant,
        ::DDS::Publisher_ptr                   publisher,
        TAO::DCPS::PublisherImpl*              publisher_servant,
        ::DDS::DataWriter_ptr                  dw_objref,
        ::TAO::DCPS::DataWriterRemote_ptr      dw_remote_objref
      )
        ACE_THROW_SPEC ((
        CORBA::SystemException
      ));

  /**
   * Do parts of enable specific to the datatype.
   * Called by DataWriterImpl::enable().
   */
  virtual ::DDS::ReturnCode_t enable_specific (
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ));

  /**
   * The framework has completed its part of unregistering the
   * given instance.
   */
  virtual void unregistered(::DDS::InstanceHandle_t   instance_handle);

  /**
   * Accessor to the marshalled data sample allocator.
   */
  ACE_INLINE
  DataAllocator* data_allocator () const  {
    return data_allocator_;
  }

private:

  /**
   * Serialize the instance data.
   *
   * @param instance_data The data to serialize.
   * @param for_write If 1 use the fast allocator; otherwise use the heap.
   * @return returns the serialized data.
   */
  ACE_Message_Block* dds_marshal(
    const ::<%SCOPE%><%TYPE%>& instance_data,
    int  for_write = 1);

  /**
   * Find the instance handle for the given instance_data using
   * the data type's key(s).  If the instance does not already exist
   * create a new instance handle for it.
   */
  ::DDS::ReturnCode_t get_or_create_instance_handle(
    DDS::InstanceHandle_t& handle,
    const ::<%SCOPE%><%TYPE%>& instance_data,
    const ::DDS::Time_t & source_timestamp);

  /**
   * Get the InstanceHanle for the given data.
   */
  ::DDS::InstanceHandle_t get_instance_handle(
    ::<%SCOPE%><%TYPE%> instance_data);

   InstanceMap  instance_map_;
   size_t       marshaled_size_;
   // The lock pool will be thread safe because
   // only one write call is allowed at a time.
   DataBlockLockPool*  db_lock_pool_;
   DataAllocator* data_allocator_;
   ::TAO::DCPS::MessageBlockAllocator* mb_allocator_;
   ::TAO::DCPS::DataBlockAllocator*    db_allocator_;
};
<%NAMESPACEEND%>

<%NAMESPACESTART%>
/** Servant for DataReader interface of <%TYPE%> data type.
 *
 * See the DDS specification, OMG formal/04-12-02, for a description of
 * this interface.
 *
 * @note zero-copy methods are specific to TAO but this is allowed by
 *       the DDS specification.
 *
 */
class <%EXPORT%> <%TYPE%>DataReaderImpl
  : public virtual TAO::DCPS::LocalObject<<%TYPE%>DataReader>,
    public virtual TAO::DCPS::DataReaderImpl
{
public:

  typedef std::map<<%SCOPE%><%TYPE%>, DDS::InstanceHandle_t,
      <%TYPE%>KeyLessThan> InstanceMap;
  typedef ::TAO::DCPS::Cached_Allocator_With_Overflow<<%SCOPE%><%TYPE%>, ACE_Null_Mutex>  DataAllocator;

  //Constructor
  <%TYPE%>DataReaderImpl (void);

  //Destructor
  virtual ~<%TYPE%>DataReaderImpl (void);

  virtual
  DDS::ReturnCode_t delete_contained_entities (
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
    ));

  /**
   * Initialize the DataReader object.
   * Called as part of create_datareader.
   */
  virtual
  void init (
        TAO::DCPS::TopicImpl*                    a_topic,
        const ::DDS::DataReaderQos &             qos,
        ::DDS::DataReaderListener_ptr            a_listener,
        TAO::DCPS::DomainParticipantImpl*        participant,
        TAO::DCPS::SubscriberImpl*               subscriber,
        ::DDS::Subscriber_ptr                    subscriber_objref,
        ::DDS::DataReader_ptr					 dr_objerf,
        TAO::DCPS::DataReaderRemote_ptr          dr_remote_objref
      )
        ACE_THROW_SPEC ((
        CORBA::SystemException
      ));

  /**
   * Do parts of enable specific to the datatype.
   * Called by DataReaderImpl::enable().
   */
  virtual ::DDS::ReturnCode_t enable_specific (
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ));

  virtual
  DDS::ReturnCode_t read (
      ::<%MODULE%><%TYPE%>Seq & received_data,
      ::DDS::SampleInfoSeq & info_seq,
      ::CORBA::Long max_samples,
      ::DDS::SampleStateMask sample_states,
      ::DDS::ViewStateMask view_states,
      ::DDS::InstanceStateMask instance_states
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
    ));

  virtual
  DDS::ReturnCode_t take (
      ::<%MODULE%><%TYPE%>Seq & received_data,
      ::DDS::SampleInfoSeq & info_seq,
      ::CORBA::Long max_samples,
      ::DDS::SampleStateMask sample_states,
      ::DDS::ViewStateMask view_states,
      ::DDS::InstanceStateMask instance_states
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
    ));

  virtual
  DDS::ReturnCode_t read_next_sample (
      ::<%SCOPE%><%TYPE%> & received_data,
      ::DDS::SampleInfo & sample_info
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
    ));

  virtual
  DDS::ReturnCode_t take_next_sample (
      ::<%SCOPE%><%TYPE%> & received_data,
      ::DDS::SampleInfo & sample_info
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
    ));

  virtual
  DDS::ReturnCode_t read_instance (
      ::<%MODULE%><%TYPE%>Seq & received_data,
      ::DDS::SampleInfoSeq & info_seq,
      ::CORBA::Long max_samples,
      ::DDS::InstanceHandle_t a_handle,
      ::DDS::SampleStateMask sample_states,
      ::DDS::ViewStateMask view_states,
      ::DDS::InstanceStateMask instance_states
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
    ));

  virtual
  DDS::ReturnCode_t take_instance (
      ::<%MODULE%><%TYPE%>Seq & received_data,
      ::DDS::SampleInfoSeq & info_seq,
      ::CORBA::Long max_samples,
      ::DDS::InstanceHandle_t a_handle,
      ::DDS::SampleStateMask sample_states,
      ::DDS::ViewStateMask view_states,
      ::DDS::InstanceStateMask instance_states
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
    ));

  virtual
  DDS::ReturnCode_t read_next_instance (
      ::<%MODULE%><%TYPE%>Seq & received_data,
      ::DDS::SampleInfoSeq & info_seq,
      ::CORBA::Long max_samples,
      ::DDS::InstanceHandle_t a_handle,
      ::DDS::SampleStateMask sample_states,
      ::DDS::ViewStateMask view_states,
      ::DDS::InstanceStateMask instance_states
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
    ));

  virtual
  DDS::ReturnCode_t take_next_instance (
      ::<%MODULE%><%TYPE%>Seq & received_data,
      ::DDS::SampleInfoSeq & info_seq,
      ::CORBA::Long max_samples,
      ::DDS::InstanceHandle_t a_handle,
      ::DDS::SampleStateMask sample_states,
      ::DDS::ViewStateMask view_states,
      ::DDS::InstanceStateMask instance_states
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
    ));

  virtual
  DDS::ReturnCode_t return_loan (
      ::<%MODULE%><%TYPE%>Seq & received_data,
      ::DDS::SampleInfoSeq & info_seq
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
    ));

  virtual
  DDS::ReturnCode_t get_key_value (
      ::<%SCOPE%><%TYPE%> & key_holder,
      ::DDS::InstanceHandle_t handle
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
    ));

  virtual 
  DDS::ReturnCode_t auto_return_loan(void* seq);

  void release_loan (::<%MODULE%><%TYPE%>Seq & received_data);

  void dec_ref_data_element(::TAO::DCPS::ReceivedDataElement* r);

 protected:

    virtual void dds_demarshal(const TAO::DCPS::ReceivedDataSample& sample) ;

    virtual void dispose(const TAO::DCPS::ReceivedDataSample& sample) ;

    //virtual TAO::DCPS::DataReaderRemote_ptr get_datareaderremote_obj_ref ();
    
    virtual void release_instance_i (::DDS::InstanceHandle_t handle);


  private:
    ::DDS::ReturnCode_t  store_instance_data(
         ::<%SCOPE%><%TYPE%> *instance_data,
         const TAO::DCPS::DataSampleHeader& header
         ) ;

    /// common input read* & take* input processing and precondition checks
    ::DDS::ReturnCode_t check_inputs (
        const char* method_name,
        ::<%MODULE%><%TYPE%>Seq & received_data,
        ::DDS::SampleInfoSeq & info_seq,
        ::CORBA::Long& max_samples
        ) ;

   InstanceMap  instance_map_;
   DataAllocator* data_allocator_;
};

<%NAMESPACEEND%>

#endif /* <%UPPERTYPE%>TYPESUPPORTI_H_  */


!EOT

}

1;

