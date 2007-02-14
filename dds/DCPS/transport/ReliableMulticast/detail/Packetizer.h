// -*- C++ -*-
//

#ifndef TAO_DCPS_PACKETIZER_H
#define TAO_DCPS_PACKETIZER_H

#include /**/ "ace/pre.h"
#include /**/ "ace/config-all.h"

#if !defined (ACE_LACKS_PRAGMA_ONCE)
#pragma once
#endif /* ACE_LACKS_PRAGMA_ONCE */

#include "ReliableMulticast_Export.h"
#include "ace/SOCK_IO.h"
#include <vector>

namespace TAO
{

  namespace DCPS
  {

    namespace ReliableMulticast
    {

      namespace detail
      {

        struct Packet;

        class ReliableMulticast_Export Packetizer
        {
        public:
          enum
          {
            MAX_PAYLOAD_SIZE = 1024
          };

          void packetize(
            const iovec iov[],
            int size,
            std::vector<Packet>& packets
            );
        };

      } /* namespace detail */

    } /* namespace ReliableMulticast */

  } /* namespace DCPS */

} /* namespace TAO */

#if defined (__ACE_INLINE__)
#include "Packetizer.inl"
#endif /* __ACE_INLINE__ */

#include /**/ "ace/post.h"

#endif /* TAO_DCPS_PACKETIZER_H */
