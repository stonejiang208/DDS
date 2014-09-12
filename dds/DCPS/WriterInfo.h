/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */


#ifndef OPENDDS_DCPS_WRITERINFO_H
#define OPENDDS_DCPS_WRITERINFO_H

#include <list>
#include "dds/DdsDcpsInfoUtilsC.h"
// #include "RcHandle_T.h"
#include "RcObject_T.h"
#include "Definitions.h"
#include "CoherentChangeControl.h"
#include "DisjointSequence.h"

namespace OpenDDS {
namespace DCPS {

class WriterInfo;

class OpenDDS_Dcps_Export WriterInfoListener
{
public:
  WriterInfoListener();
  virtual ~WriterInfoListener();

  RepoId subscription_id_;

  /// The time interval for checking liveliness.
  /// TBD: Should this be initialized with
  ///      DDS::DURATION_INFINITE_SEC and DDS::DURATION_INFINITE_NSEC
  ///      instead of ACE_Time_Value::zero to be consistent with default
  ///      duration qos ? Or should we simply use the ACE_Time_Value::zero
  ///      to indicate the INFINITY duration ?
  ACE_Time_Value liveliness_lease_duration_;

  /// tell instances when a DataWriter transitions to being alive
  /// The writer state is inout parameter, it has to be set ALIVE before
  /// handle_timeout is called since some subroutine use the state.
  virtual void writer_became_alive(WriterInfo&           info,
                                   const ACE_Time_Value& when);

  /// tell instances when a DataWriter transitions to DEAD
  /// The writer state is inout parameter, the state is set to DEAD
  /// when it returns.
  virtual void writer_became_dead(WriterInfo&           info,
                                  const ACE_Time_Value& when);

  /// tell instance when a DataWriter is removed.
  /// The liveliness status need update.
  virtual void writer_removed(WriterInfo& info);
};


#ifndef OPENDDS_NO_OBJECT_MODEL_PROFILE
enum Coherent_State {
  NOT_COMPLETED_YET,
  COMPLETED,
  REJECTED
};
#endif



/// Keeps track of a DataWriter's liveliness for a DataReader.
class OpenDDS_Dcps_Export WriterInfo : public RcObject<ACE_SYNCH_MUTEX> {
  friend class WriteInfoListner;

public:
  enum WriterState { NOT_SET, ALIVE, DEAD };

  WriterInfo();  // needed for maps

  WriterInfo(WriterInfoListener*         reader,
             const PublicationId&        writer_id,
             const ::DDS::DataWriterQos& writer_qos,
             const ::DDS::DataReaderQos& reader_qos);

  /// check to see if this writer is alive (called by handle_timeout).
  /// @param now next time this DataWriter will become not active (not alive)
  ///      if no sample or liveliness message is received.
  /// @returns absolute time when the Writer will become not active (if no activity)
  ///          of ACE_Time_Value::zero if the writer is already or became not alive
  ACE_Time_Value check_activity(const ACE_Time_Value& now);

  /// called when a sample or other activity is received from this writer.
  int received_activity(const ACE_Time_Value& when);

  /// returns 1 if the DataWriter is lively; 2 if dead; otherwise returns 0.
  WriterState get_state() {
    return state_;
  };

  std::string get_state_str() const;

  /// update liveliness when remove_association is called.
  void removed();

  /// Remove ack requests prior to the given sequence.
  /// NOTE: This removes *all* ack requests for this publication
  ///       satisfied by this sequence.
  void clear_acks(SequenceNumber sequence);

  /// Determine if a SAMPLE_ACK message should be sent to this
  /// publication.
  bool should_ack(ACE_Time_Value now);

  /// Set the time after which we no longer need to generate a
  /// SAMPLE_ACK for this sequence value.
  void ack_deadline(SequenceNumber sequence, ACE_Time_Value when);

  /// Update the last observed sequence number.
  void ack_sequence(SequenceNumber value);

  /// Return the most recently observed contiguous sequence number.
  SequenceNumber ack_sequence() const;

#ifndef OPENDDS_NO_OBJECT_MODEL_PROFILE
  Coherent_State coherent_change_received ();
  void reset_coherent_info ();
  void set_group_info (const CoherentChangeControl& info);
#endif

  void clear_owner_evaluated ();
  void set_owner_evaluated (::DDS::InstanceHandle_t instance, bool flag);
  bool is_owner_evaluated (::DDS::InstanceHandle_t instance);

  //private:

  /// Timestamp of last write/dispose/assert_liveliness from this DataWriter
  ACE_Time_Value last_liveliness_activity_time_;

  /// Times after which we no longer need to respond to a REQUEST_ACK message.
  typedef std::list<std::pair<SequenceNumber, ACE_Time_Value> > DeadlineList;
  DeadlineList ack_deadlines_;

  DisjointSequence ack_sequence_;

  bool seen_data_;

  // Non-zero if this a durable writer for which we are awaiting an
  // end historic samples control message
  long historic_samples_timer_;

  /// State of the writer.
  WriterState state_;

  /// The DataReader owning this WriterInfo
  WriterInfoListener* reader_;

  /// DCPSInfoRepo ID of the DataWriter
  PublicationId writer_id_;

  /// Writer qos
  ::DDS::DataWriterQos writer_qos_;

  /// The publication entity instance handle.
  ::DDS::InstanceHandle_t handle_;

  /// Number of received coherent changes in active change set.
  ACE_Atomic_Op<ACE_Thread_Mutex, ACE_UINT32> coherent_samples_;

  /// Is this writer evaluated for owner ?
  typedef std::map < ::DDS::InstanceHandle_t, bool> OwnerEvaluateFlag;
  OwnerEvaluateFlag owner_evaluated_;

  /// Data to support GROUP access scope.
#ifndef OPENDDS_NO_OBJECT_MODEL_PROFILE
  bool group_coherent_;
  RepoId publisher_id_;
  DisjointSequence coherent_sample_sequence_;
  WriterCoherentSample writer_coherent_samples_;
  GroupCoherentSamples group_coherent_samples_;
#endif

};

inline
int
OpenDDS::DCPS::WriterInfo::received_activity(const ACE_Time_Value& when)
{
  last_liveliness_activity_time_ = when;

  if (state_ != ALIVE) { // NOT_SET || DEAD
    reader_->writer_became_alive(*this, when);
    return 0;
  }

  //TBD - is the "was alive" return value used?
  return 1;
}

} // namespace DCPS
} // namespace

#endif  /* end of include guard: OPENDDS_DCPS_WRITERINFO_H */
