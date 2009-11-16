/*
 * $Id$
 *
 * Copyright 2009 Object Computing, Inc.
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "MulticastSendStrategy.h"
#include "Multicast_Export.h"

#ifndef DCPS_MULTICASTSENDUNRELIABLE_H
#define DCPS_MULTICASTSENDUNRELIABLE_H

namespace OpenDDS {
namespace DCPS {

class OpenDDS_Multicast_Export MulticastSendUnreliable
  : public MulticastSendStrategy {
public:
  explicit MulticastSendUnreliable(MulticastDataLink* link);

  virtual void stop_i();

protected:
  virtual ACE_HANDLE get_handle();

  virtual ssize_t send_bytes_i(const iovec iov[], int n);
};

} // namespace DCPS
} // namespace OpenDDS

#endif  /* DCPS_MULTICASTSENDUNRELIABLE_H */
