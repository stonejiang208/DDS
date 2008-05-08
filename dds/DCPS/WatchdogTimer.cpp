// $Id$

#include "WatchdogTimer.h"
#include "Watchdog.h"

OpenDDS::DCPS::WatchdogTimer::WatchdogTimer (Watchdog * dog)
  : watchdog_ (dog)
{
}

OpenDDS::DCPS::WatchdogTimer::~WatchdogTimer ()
{
}

int
OpenDDS::DCPS::WatchdogTimer::handle_timeout (
  ACE_Time_Value const & /* current_time */,
  void const * /* act */)
{
  this->watchdog_->execute ();

  return 0;
}
