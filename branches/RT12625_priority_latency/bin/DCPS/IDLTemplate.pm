#
# IDLTemplate.pm - template for generating IDL implementation file for
#                  DDS TypeSupport.  The following macros are
#                  substituted when generating the output file:
#
#   <%TYPE%>        - Type requiring support in DDS.
#   <%SCOPE%>       - Enclosing scope of type.
#   <%SUBDIR%>      - Subdirectory where IDL files are located.
#   <%IDLFILE%>     - IDL file containing type definition.
#   <%MODULESTART%> - Beginning of module definition.
#   <%MODULEEND%>   - End of module definition.
#
package DCPS::IDLTemplate ;

use warnings ;
use strict ;

sub header {
  return <<'!EOT'
// $Id$

// Generated by dcps_ts.pl

#include "dds/DdsDcpsInfrastructure.idl"
#include "dds/DdsDcpsTopic.idl"
#include "dds/DdsDcpsPublication.idl"
#include "dds/DdsDcpsDataReaderEx.idl"
#include "dds/DdsDcpsTypeSupportExt.idl"

#include "<%SUBDIR%><%IDLFILE%>"

!EOT

}

sub contents { return <<'!EOT'
<%MODULESTART%>
#pragma DCPS_SUPPORT_ZERO_COPY_READ
#pragma DCPS_GEN_ZERO_COPY_READ
//typedef sequence<<%SCOPE%><%TYPE%>> <%TYPE%>Seq;
native <%TYPE%>Seq;

/** Support topic registartion for <%TYPE%> data type.
 *
 * See the DDS specification, OMG formal/04-12-02, for a description of
 * this interface.
 */
local interface <%TYPE%>TypeSupport : OpenDDS::DCPS::TypeSupport {
    DDS::ReturnCode_t register_type(
                in DDS::DomainParticipant participant,
                in string type_name);
};

/** DataWriter interface for <%TYPE%> data type.
 *
 * See the DDS specification, OMG formal/04-12-02, for a description of
 * this interface.
 */
local interface <%TYPE%>DataWriter : DDS::DataWriter {
    DDS::InstanceHandle_t register(
                in <%SCOPE%><%TYPE%> instance_data);

    DDS::InstanceHandle_t register_w_timestamp(
                in <%SCOPE%><%TYPE%> instance_data,
                in DDS::InstanceHandle_t handle,
                in DDS::Time_t source_timestamp);

    DDS::ReturnCode_t unregister(
                in <%SCOPE%><%TYPE%> instance_data,
                in DDS::InstanceHandle_t handle);

    DDS::ReturnCode_t unregister_w_timestamp(
                in <%SCOPE%><%TYPE%> instance_data,
                in DDS::InstanceHandle_t handle,
                in DDS::Time_t source_timestamp);

    //WARNING: If the handle is non-nil and the instance is not registered
    //         then this operation may cause an access violation.
    //         This lack of safety helps performance.
    DDS::ReturnCode_t write(
                in <%SCOPE%><%TYPE%> instance_data,
                in DDS::InstanceHandle_t handle);

    //WARNING: If the handle is non-nil and the instance is not registered
    //         then this operation may cause an access violation.
    //         This lack of safety helps performance.
    DDS::ReturnCode_t write_w_timestamp(
                in <%SCOPE%><%TYPE%> instance_data,
                in DDS::InstanceHandle_t handle,
                in DDS::Time_t source_timestamp);

    DDS::ReturnCode_t dispose(
                in <%SCOPE%><%TYPE%> instance_data,
                in DDS::InstanceHandle_t instance_handle);

    DDS::ReturnCode_t dispose_w_timestamp(
                in <%SCOPE%><%TYPE%> instance_data,
                in DDS::InstanceHandle_t instance_handle,
                in DDS::Time_t source_timestamp);

    DDS::ReturnCode_t get_key_value(
                inout <%SCOPE%><%TYPE%> key_holder,
                in DDS::InstanceHandle_t handle);
};

/** DataReader interface for <%TYPE%> data type.
 *
 * See the DDS specification, OMG formal/04-12-02, for a description of
 * this interface.
 */
local interface <%TYPE%>DataReader : OpenDDS::DCPS::DataReaderEx {
    DDS::ReturnCode_t read(
                inout <%TYPE%>Seq received_data,
                inout DDS::SampleInfoSeq info_seq,
                in long max_samples,
                in DDS::SampleStateMask sample_states,
                in DDS::ViewStateMask view_states,
                in DDS::InstanceStateMask instance_states);

    DDS::ReturnCode_t take(
                inout <%TYPE%>Seq received_data,
                inout DDS::SampleInfoSeq info_seq,
                in long max_samples,
                in DDS::SampleStateMask sample_states,
                in DDS::ViewStateMask view_states,
                in DDS::InstanceStateMask instance_states);

    DDS::ReturnCode_t read_next_sample(
                inout <%SCOPE%><%TYPE%> received_data,
                inout DDS::SampleInfo sample_info);

    DDS::ReturnCode_t take_next_sample(
                inout <%SCOPE%><%TYPE%> received_data,
                inout DDS::SampleInfo sample_info);

    DDS::ReturnCode_t read_instance(
                inout <%TYPE%>Seq received_data,
                inout DDS::SampleInfoSeq info_seq,
                in long max_samples,
                in DDS::InstanceHandle_t a_handle,
                in DDS::SampleStateMask sample_states,
                in DDS::ViewStateMask view_states,
                in DDS::InstanceStateMask instance_states);

    DDS::ReturnCode_t take_instance(
                inout <%TYPE%>Seq received_data,
                inout DDS::SampleInfoSeq info_seq,
                in long max_samples,
                in DDS::InstanceHandle_t a_handle,
                in DDS::SampleStateMask sample_states,
                in DDS::ViewStateMask view_states,
                in DDS::InstanceStateMask instance_states);

    DDS::ReturnCode_t read_next_instance(
                inout <%TYPE%>Seq received_data,
                inout DDS::SampleInfoSeq info_seq,
                in long max_samples,
                in DDS::InstanceHandle_t a_handle,
                in DDS::SampleStateMask sample_states,
                in DDS::ViewStateMask view_states,
                in DDS::InstanceStateMask instance_states);

    DDS::ReturnCode_t take_next_instance(
                inout <%TYPE%>Seq received_data,
                inout DDS::SampleInfoSeq info_seq,
                in long max_samples,
                in DDS::InstanceHandle_t a_handle,
                in DDS::SampleStateMask sample_states,
                in DDS::ViewStateMask view_states,
                in DDS::InstanceStateMask instance_states);

    DDS::ReturnCode_t return_loan(
                inout <%TYPE%>Seq received_data,
                inout DDS::SampleInfoSeq info_seq);

    DDS::ReturnCode_t get_key_value(
                inout <%SCOPE%><%TYPE%> key_holder,
                in DDS::InstanceHandle_t handle);
};

<%MODULEEND%>

!EOT

}

1;

