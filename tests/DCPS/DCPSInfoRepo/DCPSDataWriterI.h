// -*- C++ -*-
//
// $Id$

// ****  Code generated by the The ACE ORB (TAO) IDL Compiler ****
// TAO and the TAO IDL Compiler have been developed by:
//       Center for Distributed Object Computing
//       Washington University
//       St. Louis, MO
//       USA
//       http://www.cs.wustl.edu/~schmidt/doc-center.html
// and
//       Distributed Object Computing Laboratory
//       University of California at Irvine
//       Irvine, CA
//       USA
//       http://doc.ece.uci.edu/
//
// Information about TAO is available at:
//     http://www.cs.wustl.edu/~schmidt/TAO.html

// TAO_IDL - Generated from
// .\be\be_codegen.cpp:951

#ifndef DCPSDATAWRITERI_H_
#define DCPSDATAWRITERI_H_

#include "dds/DCPS/DataWriterCallbacks.h"
#include "dds/DCPS/Definitions.h"

#include <vector>

#if !defined (ACE_LACKS_PRAGMA_ONCE)
#pragma once
#endif /* ACE_LACKS_PRAGMA_ONCE */

#include "DiscReceivedCalls.h"

//Class TAO_DDS_DCPSDataWriter_i
class TAO_DDS_DCPSDataWriter_i
  : public OpenDDS::DCPS::DataWriterCallbacks
{
public:
  //Constructor
  TAO_DDS_DCPSDataWriter_i (void);

  //Destructor
  virtual ~TAO_DDS_DCPSDataWriter_i (void);

  virtual ::DDS::ReturnCode_t enable_specific (
      ) { received_.received(DiscReceivedCalls::ENABLE_SPECIFIC); return ::DDS::RETCODE_OK;};



  virtual void add_association (
      const ::OpenDDS::DCPS::RepoId& yourId,
      const OpenDDS::DCPS::ReaderAssociation& reader,
      bool active
    );

  virtual void association_complete(const OpenDDS::DCPS::RepoId& /*remote_id*/) { received_.received(DiscReceivedCalls::ASSOC_COMPLETE); }

  virtual void remove_associations (
      const OpenDDS::DCPS::ReaderIdSeq & readers,
      ::CORBA::Boolean notify_lost
    );

  virtual void update_incompatible_qos (
      const OpenDDS::DCPS::IncompatibleQosStatus & status
    );

  virtual void update_subscription_params(
    const OpenDDS::DCPS::RepoId&, const DDS::StringSeq &);

  void inconsistent_topic() {}

  DiscReceivedCalls& received()
    {
      return received_;
    }
private:
  DiscReceivedCalls received_;
};


#endif /* DCPSDATAWRITERI_H_  */
