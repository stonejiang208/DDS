/*
 * $Id$
 *
 * Copyright 2010 Object Computing, Inc.
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#ifndef OPENDDS_DCPS_DATALINK_RCH_H
#define OPENDDS_DCPS_DATALINK_RCH_H

#include "dds/DCPS/RcHandle_T.h"

namespace OpenDDS {
namespace DCPS {

/**
 * This file instantiates a smart-pointer type (rch) to a specific
 * underlying "pointed-to" type.
 *
 * This type definition is in its own header file so that the
 * smart-pointer type can be defined without causing the inclusion
 * of the underlying "pointed-to" type header file.  Instead, the
 * underlying "pointed-to" type is forward-declared.  This is analogous
 * to the inclusion requirements that would be imposed if the
 * "pointed-to" type was being referenced via a raw pointer type.
 * Holding the raw pointer indirectly via a smart pointer doesn't
 * change the inclusion requirements (ie, the underlying type doesn't
 * need to be included in either case).
 */

// Forward declaration of the underlying type.
class DataLink;

/// The type definition for the smart-pointer to the underlying type.
typedef RcHandle<DataLink> DataLink_rch;

} // namespace DCPS
} // namespace OpenDDS

#endif /* OPENDDS_DCPS_DATALINK_RCH_H */
