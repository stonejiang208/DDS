// -*- C++ -*-
//
// $Id$

#include "DCPS/DdsDcps_pch.h" //Only the _pch include should start with DCPS/
#include "debug.h"
#include "SubscriberImpl.h"
#include "DataReaderRemoteImpl.h"
#include "DomainParticipantImpl.h"
#include "Qos_Helper.h"
#include "TopicImpl.h"
#include "DataReaderImpl.h"
#include "Service_Participant.h"
#include "dds/DdsDcpsTypeSupportTaoC.h"
#include "TopicDescriptionImpl.h"
#include "Marked_Default_Qos.h"
#include "DataSampleList.h"
#include "AssociationData.h"
#include "Transient_Kludge.h"
#include "dds/DCPS/transport/framework/TransportInterface.h"
#include "dds/DCPS/transport/framework/TransportImpl.h"
#include "dds/DCPS/transport/framework/DataLinkSet.h"

#include "tao/debug.h"


namespace TAO
{
  namespace DCPS
  {

#if 0
    // Emacs trick to align code with first column
    // This will cause emacs to emit bogus alignment message
    // For now just disregard them.
  }}
#endif

// Implementation skeleton constructor
SubscriberImpl::SubscriberImpl (const ::DDS::SubscriberQos & qos,
				::DDS::SubscriberListener_ptr a_listener,
				DomainParticipantImpl*       participant,
				::DDS::DomainParticipant_ptr
				participant_objref)
  : qos_(qos),
    default_datareader_qos_(
			    TheServiceParticipant->initial_DataReaderQos()),

    listener_mask_(DEFAULT_STATUS_KIND_MASK),
    fast_listener_ (0),
    participant_(participant),
    participant_objref_ (::DDS::DomainParticipant::_duplicate (participant_objref)),
    repository_ (TheServiceParticipant->get_repository ())
{
  participant_->_add_ref ();

  //Note: OK to duplicate a nil.
  listener_ = ::DDS::SubscriberListener::_duplicate(a_listener);
  if (! CORBA::is_nil (a_listener))
    {
      fast_listener_ =
	reference_to_servant<DDS::SubscriberListener,
	DDS::SubscriberListener_ptr>
	(listener_.in ());
    }
}

// Implementation skeleton destructor
SubscriberImpl::~SubscriberImpl (void)
{
  participant_->_remove_ref ();

  // Tell the transport to detach this
  // Subscriber/TransportInterface.
  this->detach_transport ();
  //
  // The datareders should be deleted already before calling delete
  // subscriber.
  if (! is_clean ())
    {
      ACE_ERROR ((LM_ERROR,
		  ACE_TEXT("(%P|%t) ERROR: ")
		  ACE_TEXT("SubscriberImpl::~SubscriberImpl, ")
		  ACE_TEXT("some datareaders still exist.\n")));
    }
}

//Note: caller should NOT assign to DataReader_var (without _duplicate'ing)
//      because it will steal the framework's reference.
::DDS::DataReader_ptr
SubscriberImpl::create_datareader (
				   ::DDS::TopicDescription_ptr a_topic_desc,
				   const ::DDS::DataReaderQos & qos,
				   ::DDS::DataReaderListener_ptr a_listener
				   )
  ACE_THROW_SPEC ((
		   CORBA::SystemException
		   ))
{
  if (CORBA::is_nil (a_topic_desc))
    {
      ACE_ERROR ((LM_ERROR,
		  ACE_TEXT("(%P|%t) ERROR: ")
		  ACE_TEXT("SubscriberImpl::create_datareader, ")
		  ACE_TEXT("topic desc is nil.\n")));
      return ::DDS::DataReader::_nil();
    }

  ::DDS::DataReaderQos dr_qos;

  TopicImpl* topic_servant =
    reference_to_servant<TopicImpl,
    DDS::TopicDescription_ptr>(
			       a_topic_desc);

  if (qos == DATAREADER_QOS_DEFAULT)
    {
      this->get_default_datareader_qos(dr_qos);
    }
  else if (qos == DATAREADER_QOS_USE_TOPIC_QOS)
    {
      ::DDS::TopicQos topic_qos;
      topic_servant->get_qos (topic_qos);

      this->get_default_datareader_qos(dr_qos);

      this->copy_from_topic_qos (dr_qos,
				 topic_qos);
    }
  else
    {
      dr_qos = qos;
    }

  if (! Qos_Helper::valid (dr_qos))
    {
      ACE_ERROR ((LM_ERROR,
		  ACE_TEXT("(%P|%t) ERROR: ")
		  ACE_TEXT("SubscriberImpl::create_datareader, ")
		  ACE_TEXT("invalid qos.\n")));
      return ::DDS::DataReader::_nil();
    }

  if (! Qos_Helper::consistent (dr_qos))
    {
      ACE_ERROR ((LM_ERROR,
		  ACE_TEXT("(%P|%t) ERROR: ")
		  ACE_TEXT("SubscriberImpl::create_datareader, ")
		  ACE_TEXT("inconsistent qos.\n")));
      return ::DDS::DataReader::_nil();
    }

  TAO::DCPS::TypeSupport_ptr typesupport =
    topic_servant->get_type_support();

  if (0 == typesupport)
    {
      CORBA::String_var name = topic_servant->get_name ();
      ACE_ERROR ((LM_ERROR,
		  ACE_TEXT("(%P|%t) ERROR: ")
		  ACE_TEXT("SubscriberImpl::create_datareader, ")
		  ACE_TEXT("typesupport(topic_name=%s) is nil.\n"),
		  name.in ()));
      return ::DDS::DataReader::_nil ();
    }

    ::DDS::DataReader_var dr_obj = typesupport->create_datareader();

  DataReaderImpl* dr_servant =
    reference_to_servant<DataReaderImpl, ::DDS::DataReader_ptr>
    (dr_obj.in ());

  DataReaderRemoteImpl* reader_remote_impl = 0;
  ACE_NEW_RETURN(reader_remote_impl,
                    DataReaderRemoteImpl(dr_servant),
                    ::DDS::DataReader::_nil());

  ::TAO::DCPS::DataReaderRemote_var dr_remote_obj = 
      servant_to_remote_reference(reader_remote_impl);

  // Give ownership to poa.
  dr_servant->_remove_ref ();

  DomainParticipantImpl* participant =
    reference_to_servant<DomainParticipantImpl,
    ::DDS::DomainParticipant_ptr>(
				  participant_objref_.in ());

  dr_servant->init (topic_servant,
		    dr_qos,
		    a_listener,
		    participant,
		    this,
		    subscriber_objref_.in (),
		    dr_obj.in (),
            dr_remote_obj.in ());

  if ((this->enabled_ == true)
      && (qos_.entity_factory.autoenable_created_entities == 1))
    {
      ::DDS::ReturnCode_t ret
	  = dr_servant->enable ();

      if (ret != ::DDS::RETCODE_OK)
	{
	  ACE_ERROR ((LM_ERROR,
		      ACE_TEXT("(%P|%t) ERROR: ")
		      ACE_TEXT("SubscriberImpl::create_datareader, ")
		      ACE_TEXT("enable failed.\n")));
	  return ::DDS::DataReader::_nil ();
	}
    }

  TAO::DCPS::TransportImpl_rch impl = this->get_transport_impl();
  if (impl.is_nil ())
    {
      ACE_ERROR ((LM_ERROR,
		  ACE_TEXT("(%P|%t) ERROR: ")
		  ACE_TEXT("SubscriberImpl::create_datareader, ")
		  ACE_TEXT("the subscriber has not been attached to the TransportImpl.\n")));
      return ::DDS::DataReader::_nil ();
    }
  // Register the DataReader object with the TransportImpl.
  else if (impl->register_subscription (dr_servant->get_subscription_id(),
					dr_servant) == -1)
    {
      ACE_ERROR ((LM_ERROR,
		  ACE_TEXT("(%P|%t) ERROR: ")
		  ACE_TEXT("SubscriberImpl::create_datareader, ")
		  ACE_TEXT("failed to register datareader %d with TransportImpl.\n"),
		  dr_servant->get_subscription_id()));
      return ::DDS::DataReader::_nil ();
    }

  // add created data reader to this' data reader container -
  // done in enable_reader
  return ::DDS::DataReader::_duplicate(dr_obj.in ());
}


::DDS::ReturnCode_t
SubscriberImpl::delete_datareader (::DDS::DataReader_ptr a_datareader)
  ACE_THROW_SPEC (( CORBA::SystemException ))
{
  DBG_ENTRY_LVL("SubscriberImpl","delete_datareader",5);

  ACE_TRACE(ACE_TEXT("SubscriberImpl::delete_datareader"));
  if (enabled_ == false)
    {
      ACE_ERROR_RETURN ((LM_ERROR,
			 ACE_TEXT("(%P|%t) ERROR: SubscriberImpl::delete_datareader, ")
			 ACE_TEXT(" Entity is not enabled. \n")),
			::DDS::RETCODE_NOT_ENABLED);
    }

  DataReaderImpl* dr_servant
    = reference_to_servant <DataReaderImpl, ::DDS::DataReader_ptr>
    (a_datareader);

  if (dr_servant->get_subscriber_servant () != this)
    {
      return ::DDS::RETCODE_PRECONDITION_NOT_MET;
    }

  SubscriberDataReaderInfo* dr_info = 0;

  {
    ACE_GUARD_RETURN (ACE_Recursive_Thread_Mutex,
      guard,
      this->si_lock_,
      ::DDS::RETCODE_ERROR);

    DataReaderMap::iterator it;

    for (it = datareader_map_.begin ();
      it != datareader_map_.end ();
      it ++)
    {
      if (it->second->local_reader_impl_ == dr_servant)
      {
        break;
      }
    }


    if (it == datareader_map_.end ())
    {
      CORBA::String_var topic_name = dr_servant->get_topic_name();
      RepoId id = dr_servant->get_subscription_id ();
      ACE_ERROR_RETURN((LM_ERROR,
        ACE_TEXT("(%P|%t) ERROR: ")
        ACE_TEXT("SubscriberImpl::delete_datareader, ")
        ACE_TEXT("The datareader(topic_name=%s id=%d) is not found\n"),
        topic_name.in (), id),
        ::DDS::RETCODE_ERROR);
    }

    dr_info = it->second;
      
    datareader_map_.erase(it) ;
    datareader_set_.erase(dr_servant) ;
  }

  RepoId subscription_id  = dr_info->subscription_id_ ;

  try
    {
      this->repository_->remove_subscription(
					     participant_->get_domain_id (),
					     participant_->get_id (),
					     subscription_id) ;
    }
  catch (const CORBA::SystemException& sysex)
    {
      sysex._tao_print_exception (
        "ERROR: System Exception"
        " in SubscriberImpl::delete_datareader");
      return ::DDS::RETCODE_ERROR;
    }
  catch (const CORBA::UserException& userex)
    {
      userex._tao_print_exception (
        "ERROR: User Exception"
        " in SubscriberImpl::delete_datareader");
      return ::DDS::RETCODE_ERROR;
    }


  TAO::DCPS::TransportImpl_rch impl = this->get_transport_impl();
  if (impl.is_nil ())
    {
      ACE_ERROR ((LM_ERROR,
		  ACE_TEXT("(%P|%t) ERROR: ")
		  ACE_TEXT("SubscriberImpl::delete_datareader, ")
		  ACE_TEXT("the subscriber has not been attached to the TransportImpl.\n")));
      return ::DDS::RETCODE_ERROR;
    }
  // Unregister the DataReader object with the TransportImpl.
  else if (impl->unregister_subscription (subscription_id) == -1)
    {
      ACE_ERROR ((LM_ERROR,
		  ACE_TEXT("(%P|%t) ERROR: ")
		  ACE_TEXT("SubscriberImpl::delete_datareader, ")
		  ACE_TEXT("failed to unregister datareader %d with TransportImpl.\n"),
		  subscription_id));
      return ::DDS::RETCODE_ERROR;
    }

  // Clean up any remaining associations
  dr_servant->remove_all_associations();

  delete dr_info;

  dr_servant->cleanup ();

  // Decrease the ref count after the servant is removed
  // from the datareader map.

  dr_servant->_remove_ref ();

  return ::DDS::RETCODE_OK;
}


::DDS::ReturnCode_t
SubscriberImpl::delete_contained_entities (
					   )
  ACE_THROW_SPEC ((
		   CORBA::SystemException
		   ))
{
  ACE_TRACE(ACE_TEXT("SubscriberImpl::delete_contained_entities")) ;

  if (enabled_ == false)
    {
      ACE_ERROR_RETURN ((LM_ERROR,
			 ACE_TEXT("(%P|%t) ERROR: SubscriberImpl::delete_contained_entities, ")
			 ACE_TEXT(" Entity is not enabled. \n")),
			::DDS::RETCODE_NOT_ENABLED);
    }

  // mark that the entity is being deleted
  set_deleted (true);


  ACE_Vector< ::DDS::DataReader_ptr> drs ;

  {
    ACE_GUARD_RETURN (ACE_Recursive_Thread_Mutex,
      guard,
      this->si_lock_,
      ::DDS::RETCODE_ERROR);

    DataReaderMap::iterator it;
    DataReaderMap::iterator itEnd = datareader_map_.end ();
    for (it = datareader_map_.begin (); it != datareader_map_.end (); ++it)
    {
       drs.push_back (it->second->local_reader_objref_);
    }
  }   
    
  size_t num_rds = drs.size ();

  for (size_t i = 0; i < num_rds; ++i)
  {
    ::DDS::ReturnCode_t ret = delete_datareader(drs[i]);
    if (ret != ::DDS::RETCODE_OK)
    {
      ACE_ERROR_RETURN ((LM_ERROR,
        ACE_TEXT("(%P|%t) ERROR: ")
        ACE_TEXT("SubscriberImpl::delete_contained_entities, ")
        ACE_TEXT("failed to delete datareader\n")),
        ret);
    }
  }

  // the subscriber can now start creating new publications
  set_deleted (false);

  return ::DDS::RETCODE_OK;
}


::DDS::DataReader_ptr
SubscriberImpl::lookup_datareader (
				   const char * topic_name
				   )
  ACE_THROW_SPEC ((
		   CORBA::SystemException
		   ))
{
  if (enabled_ == false)
    {
      //tbd: should throw NotEnabled exception ?
      ACE_ERROR ((LM_ERROR,
		  ACE_TEXT("(%P|%t) ERROR: SubscriberImpl::lookup_datareader, ")
		  ACE_TEXT(" Entity is not enabled. \n")));
      return ::DDS::DataReader::_nil ();
    }

  ACE_GUARD_RETURN (ACE_Recursive_Thread_Mutex,
		    guard,
		    this->si_lock_,
		    ::DDS::DataReader::_nil ());

  // If multiple entries whose key is "topic_name" then which one is
  // returned ? Spec does not limit which one should give.
  DataReaderMap::iterator it = datareader_map_.find (topic_name);
  if (it == datareader_map_.end ())
    {
      if (DCPS_debug_level >= 2)
        {
          ACE_DEBUG ((LM_DEBUG,
		      ACE_TEXT("(%P|%t) ")
		      ACE_TEXT("SubscriberImpl::lookup_datareader, ")
		      ACE_TEXT("The datareader(topic_name=%s) is not found\n"),
		      topic_name));
        }
      return ::DDS::DataReader::_nil ();
    }
  else
    {
      return ::DDS::DataReader::_duplicate (it->second->local_reader_objref_);
    }
}


::DDS::ReturnCode_t
SubscriberImpl::get_datareaders (
				 ::DDS::DataReaderSeq_out readers,
				 ::DDS::SampleStateMask sample_states,
				 ::DDS::ViewStateMask view_states,
				 ::DDS::InstanceStateMask instance_states
				 )
  ACE_THROW_SPEC ((
		   CORBA::SystemException
		   ))
{
  ACE_GUARD_RETURN (ACE_Recursive_Thread_Mutex,
		    guard,
		    this->si_lock_,
		    ::DDS::RETCODE_ERROR);

  int count(0) ;
  readers = new ::DDS::DataReaderSeq(datareader_set_.size()) ;
  for (DataReaderSet::const_iterator pos = datareader_set_.begin() ;
       pos != datareader_set_.end() ; ++pos)
    {
      if ((*pos)->have_sample_states(sample_states) &&
	  (*pos)->have_view_states(view_states) &&
	  (*pos)->have_instance_states(instance_states))
        {
          readers->length(count + 1) ;
          (*readers)[count++] = (*pos)->get_dr_obj_ref() ;
        }
    }

  return ::DDS::RETCODE_OK ;
}


void
SubscriberImpl::notify_datareaders (
				    )
  ACE_THROW_SPEC ((
		   CORBA::SystemException
		   ))
{
  ACE_GUARD (ACE_Recursive_Thread_Mutex,
	     guard,
	     this->si_lock_);

  DataReaderMap::iterator it;

  for (it = datareader_map_.begin () ; it != datareader_map_.end () ;
       it ++)
    {
      ::DDS::DataReaderListener_var listener =
	  it->second->local_reader_impl_->get_listener() ;

      if (it->second->local_reader_impl_->have_sample_states(
							::DDS::NOT_READ_SAMPLE_STATE))
	{
	  listener->on_data_available(it->second->local_reader_objref_) ;
	}
    }
}


::DDS::ReturnCode_t
SubscriberImpl::set_qos (
			 const ::DDS::SubscriberQos & qos
			 )
  ACE_THROW_SPEC ((
		   CORBA::SystemException
		   ))
{
  if (Qos_Helper::valid(qos) && Qos_Helper::consistent(qos))
    {
      if (enabled_ == true)
        {
          if (! Qos_Helper::changeable (qos_, qos))
	    {
	      return ::DDS::RETCODE_IMMUTABLE_POLICY;
	    }
        }
      if (! (qos_ == qos)) // no != operator ?
        {
          qos_ = qos;
          // TBD - when there are changable QoS supported
          //       this code may need to do something
          //       with the changed values.
          // TBD - when there are changable QoS then we
          //       need to tell the DCPSInfo/repo about
          //       the changes in Qos.
          // repo->set_qos(qos_);
        }
      return ::DDS::RETCODE_OK;
    }
  else
    {
      return ::DDS::RETCODE_INCONSISTENT_POLICY;
    }
}


void
SubscriberImpl::get_qos (
			 ::DDS::SubscriberQos & qos
			 )
  ACE_THROW_SPEC ((
		   CORBA::SystemException
		   ))
{
  qos = qos_;
}


::DDS::ReturnCode_t
SubscriberImpl::set_listener (
			      ::DDS::SubscriberListener_ptr a_listener,
			      ::DDS::StatusKindMask mask
			      )
  ACE_THROW_SPEC ((
		   CORBA::SystemException
		   ))
{
  listener_mask_ = mask;
  //note: OK to duplicate  and reference_to_servant a nil object ref
  listener_ = ::DDS::SubscriberListener::_duplicate(a_listener);
  fast_listener_
    = reference_to_servant< ::DDS::SubscriberListener,
    ::DDS::SubscriberListener_ptr >
    (listener_.in ());
  return ::DDS::RETCODE_OK;
}


::DDS::SubscriberListener_ptr
SubscriberImpl::get_listener (
			      )
  ACE_THROW_SPEC ((
		   CORBA::SystemException
		   ))
{
  return ::DDS::SubscriberListener::_duplicate (listener_.in ());
}


::DDS::ReturnCode_t
SubscriberImpl::begin_access (
			      )
  ACE_THROW_SPEC ((
		   CORBA::SystemException
		   ))
{
  // For ACC, PRESENTATION != GROUP, so
  return ::DDS::RETCODE_OK;
}


::DDS::ReturnCode_t
SubscriberImpl::end_access (
			    )
  ACE_THROW_SPEC ((
		   CORBA::SystemException
		   ))
{
  // For ACC, PRESENTATION != GROUP, so
  return ::DDS::RETCODE_OK;
}


::DDS::DomainParticipant_ptr
SubscriberImpl::get_participant (
				 )
  ACE_THROW_SPEC ((
		   CORBA::SystemException
		   ))
{
  return ::DDS::DomainParticipant::_duplicate (participant_objref_.in ());
}


::DDS::ReturnCode_t
SubscriberImpl::set_default_datareader_qos (
					    const ::DDS::DataReaderQos & qos
					    )
  ACE_THROW_SPEC ((
		   CORBA::SystemException
		   ))
{
  if (Qos_Helper::valid(qos) && Qos_Helper::consistent(qos))
    {
      default_datareader_qos_ = qos;
      return ::DDS::RETCODE_OK;
    }
  else
    {
      return ::DDS::RETCODE_INCONSISTENT_POLICY;
    }
}


void
SubscriberImpl::get_default_datareader_qos (
					    ::DDS::DataReaderQos & qos
					    )
  ACE_THROW_SPEC ((
		   CORBA::SystemException
		   ))
{
  qos = default_datareader_qos_;
}


::DDS::ReturnCode_t
SubscriberImpl::copy_from_topic_qos (
				     ::DDS::DataReaderQos & a_datareader_qos,
				     const ::DDS::TopicQos & a_topic_qos
				     )
  ACE_THROW_SPEC ((
		   CORBA::SystemException
		   ))
{
  if (Qos_Helper::valid(a_topic_qos) &&
      Qos_Helper::consistent(a_topic_qos))
    {
      // the caller can get the default before calling this
      // method if it wants to.
      //a_datareader_qos = this->default_datareader_qos_;
      a_datareader_qos.durability = a_topic_qos.durability;
      a_datareader_qos.deadline = a_topic_qos.deadline;
      a_datareader_qos.latency_budget = a_topic_qos.latency_budget;
      a_datareader_qos.liveliness = a_topic_qos.liveliness;
      a_datareader_qos.reliability = a_topic_qos.reliability;
      a_datareader_qos.destination_order = a_topic_qos.destination_order;
      a_datareader_qos.history = a_topic_qos.history;
      a_datareader_qos.resource_limits = a_topic_qos.resource_limits;
      return ::DDS::RETCODE_OK;
    }
  else
    {
      return ::DDS::RETCODE_INCONSISTENT_POLICY;
    }
}


::DDS::ReturnCode_t
SubscriberImpl::enable (
			)
  ACE_THROW_SPEC ((
		   CORBA::SystemException
		   ))
{
  //TDB - check if factory is enables and then enable all entities
  // (don't need to do it for now because
  //  entity_factory.autoenable_created_entities is always = 1)

  //if (factory not enabled)
  //{
  //  return ::DDS::RETCODE_PRECONDITION_NOT_MET;
  //}

  this->set_enabled ();
  return ::DDS::RETCODE_OK;
}


::DDS::StatusKindMask
SubscriberImpl::get_status_changes (
				    )
  ACE_THROW_SPEC ((
		   CORBA::SystemException
		   ))
{
  return EntityImpl::get_status_changes() ;
}


int
SubscriberImpl::is_clean () const
{
  int sub_is_clean = datareader_map_.empty ();
  if (sub_is_clean == 0 && ! TheTransientKludge->is_enabled ())
    {
      // Four BIT datareaders.
      sub_is_clean = datareader_map_.size () == 4;
    }
  return sub_is_clean;
}


void
SubscriberImpl::data_received(DataReaderImpl *reader)
{
  ACE_GUARD (ACE_Recursive_Thread_Mutex,
	     guard,
	     this->si_lock_);

  datareader_set_.insert(reader) ;
}

//SHH This method is not called.
//    Should it be implemented and called?
//void
//SubscriberImpl::cleanup()
//{
//  // tbd: add implementation.
//}

void
SubscriberImpl::add_associations (
				  const WriterAssociationSeq & writers,
				  DataReaderImpl* reader,
				  const ::DDS::DataReaderQos reader_qos
				  )
{
  if (entity_deleted_ == true)
    {
      if (DCPS_debug_level >= 1)
	ACE_DEBUG ((LM_DEBUG,
		    ACE_TEXT("(%P|%t) SubscriberImpl::add_associations")
		    ACE_TEXT(" This is a deleted subscriber, ignoring add.\n")));
      return;
    }

  size_t length = writers.length ();
  AssociationData* associations = new AssociationData [length];

  for (size_t i = 0; i < length; i++)
    {
      associations[i].remote_id_ = writers[i].writerId;
      associations[i].remote_data_ = writers[i].writerTransInfo;
    }

  // 1/11/06 SHH - this lock is not required.
  // it also causes deadlock in some cases.
  //ACE_GUARD (ACE_Recursive_Thread_Mutex,
  //           guard,
  //           this->si_lock_);

  // TBD - pass the priority as part of the associations data
  //       because there is a priority per remote publication.


 
  this->add_publications(reader->get_subscription_id(),
			                   reader,
			                   writers[0].writerQos.transport_priority.value,
			                   length,
			                   associations);

  ACE_UNUSED_ARG(reader_qos) ;  // for now...

  delete []associations; // TransportInterface does not take ownership
}

void
SubscriberImpl::remove_associations(
				    const WriterIdSeq& writers,
            const RepoId&      reader)
{
  /// Delegate to the (inherited) TransportInterface version.

  // 1/11/06 SHH - this lock is not required.
  //ACE_GUARD (ACE_Recursive_Thread_Mutex,
  //  guard,
  //  this->si_lock_);

  // TMB - I don't know why I need to call it this way, but gcc complains
  //       under linux otherwise.

  this->TransportInterface::remove_associations(writers.length(),
						writers.get_buffer(), reader, false) ; // as sub side
}


void
SubscriberImpl::reader_enabled(
                   DataReaderRemote_ptr     remote_reader,
                   ::DDS::DataReader_ptr    local_reader,
			       DataReaderImpl*          local_reader_impl,
			       const char*              topic_name,
			       RepoId                   topic_id
			       )
  ACE_THROW_SPEC ((
		   CORBA::SystemException
		   ))
{
  SubscriberDataReaderInfo* info = new SubscriberDataReaderInfo ;
  info->remote_reader_objref_  = remote_reader ;
  info->local_reader_objref_   = local_reader;
  info->local_reader_impl_     = local_reader_impl ;

  info->topic_id_ = topic_id ;

  try
    {
      ::DDS::DataReaderQos qos;
      info->local_reader_objref_->get_qos(qos);

      TAO::DCPS::TransportInterfaceInfo trans_conf_info = connection_info ();
      info->subscription_id_
	= this->repository_->add_subscription(
					      participant_->get_domain_id (),
					      participant_->get_id (),
					      info->topic_id_,
					      info->remote_reader_objref_,
					      qos,
					      trans_conf_info,
					      qos_) ;
      info->local_reader_impl_->set_subscription_id (info->subscription_id_);
    }
  catch (const CORBA::SystemException& sysex)
    {
      sysex._tao_print_exception (
        "ERROR: System Exception"
        " in SubscriberImpl::reader_enabled");
      return;
    }
  catch (const CORBA::UserException& userex)
    {
      userex._tao_print_exception (
        "ERROR: User Exception"
        " in SubscriberImpl::reader_enabled");
      return;
    }

  if (DCPS_debug_level >= 4)
    ACE_DEBUG ((LM_DEBUG,
		ACE_TEXT("(%P|%t) ")
		ACE_TEXT("SubscriberImpl::reader_enabled, ")
		ACE_TEXT("datareader(topic_name=%s) enabled\n"),
		topic_name));

  DataReaderMap::iterator it
    = datareader_map_.insert(DataReaderMap::value_type(topic_name,
						       info));

  if (it == datareader_map_.end ())
    {
      ACE_ERROR ((LM_ERROR,
		  ACE_TEXT("(%P|%t) ERROR: SubscriberImpl::reader_enabled, ")
		  ACE_TEXT("insert datareader(topic_name=%s) failed. \n"),
		  topic_name));
      return;
    }

  // Increase the ref count when the servant is referenced
  // by the datareader map.
  info->local_reader_impl_->_add_ref ();
}

::DDS::SubscriberListener*
SubscriberImpl::listener_for (::DDS::StatusKind kind)
{
  // per 2.1.4.3.1 Listener Access to Plain Communication Status
  // use this entities factory if listener is mask not enabled
  // for this kind.
  if ((listener_mask_ & kind) == 0)
    {
      return participant_->listener_for (kind);
    }
  else
    {
      return fast_listener_;
    }
}

void
SubscriberImpl::set_object_reference (const ::DDS::Subscriber_ptr& sub)
{
  if (! CORBA::is_nil (subscriber_objref_.in ()))
    {
      ACE_ERROR ((LM_ERROR,
		  ACE_TEXT("(%P|%t) ERROR: SubscriberImpl::set_object_reference, ")
		  ACE_TEXT("This subscriber is already activated. \n")));
      return;
    }

  subscriber_objref_ = ::DDS::Subscriber::_duplicate (sub);
}

} // namespace DCPS
} // namespace TAO


#if defined (ACE_HAS_EXPLICIT_TEMPLATE_INSTANTIATION)

template class std::multimap<ACE_CString,
			     SubscriberDataReaderInfo*> ;

template class std::set<DataReaderImpl *> ;

#elif defined(ACE_HAS_TEMPLATE_INSTANTIATION_PRAGMA)

#pragma instantiate std::multimap<ACE_CString,
SubscriberDataReaderInfo*> ;
#pragma instantiate std::set<DataReaderImpl *> ;

#endif /* ACE_HAS_EXPLICIT_TEMPLATE_INSTANTIATION */
