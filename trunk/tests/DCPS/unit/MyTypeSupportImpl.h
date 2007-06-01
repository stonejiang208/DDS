// -*- C++ -*-
// ============================================================================
/**
 *  @file   MyTypeSupportImpl.h
 *
 *  $Id$
 *
 *
 */
// ============================================================================


#ifndef MYTYPESUPPORTIMPL_H_
#define MYTYPESUPPORTIMPL_H_

#include "dds/DdsDcpsDataWriterRemoteC.h"
#include "dds/DdsDcpsDataReaderRemoteC.h"
#include "dds/DCPS/DataReaderImpl.h"
#include "dds/DCPS/DataWriterImpl.h"
#include "MyTypeSupportS.h"

#if !defined (ACE_LACKS_PRAGMA_ONCE)
#pragma once
#endif /* ACE_LACKS_PRAGMA_ONCE */

/**
 * @class MyTypeSupportImpl
 *
 * @brief An implementation of a TypeSupport
 *
 */
class MyTypeSupportImpl : public virtual TAO::DCPS::LocalObject<MyTypeSupport>
{

public:
  //Constructor
  MyTypeSupportImpl (void);

  //Destructor
  virtual ~MyTypeSupportImpl (void);


  virtual ::DDS::ReturnCode_t register_type (
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

  virtual ::DDS::DataWriter_ptr create_datawriter (
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
    ));

  virtual ::DDS::DataReader_ptr create_datareader (
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
    ));
};

class MyDataReaderImpl :  public virtual TAO::DCPS::DataReaderImpl
{
public:
  virtual ::DDS::ReturnCode_t enable_specific (
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
        )) { return ::DDS::RETCODE_OK;};

  virtual ::DDS::ReturnCode_t auto_return_loan (void *)
  {
    return ::DDS::RETCODE_ERROR;
  }
  
  virtual void release_instance_i(DDS::InstanceHandle_t) {}
  virtual void dds_demarshal(const TAO::DCPS::ReceivedDataSample&) {}

};

class MyDataWriterImpl :  public virtual TAO::DCPS::DataWriterImpl
{
public:
  virtual ::DDS::ReturnCode_t enable_specific (
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
        )) {return ::DDS::RETCODE_OK;};
};

#endif /* MYTYPESUPPORTIMPL_H_  */
