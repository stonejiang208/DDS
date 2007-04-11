#ifndef TRANSPORTAPI_TRANSPORT_H
#define TRANSPORTAPI_TRANSPORT_H

#include "TransportTypes.h"

namespace TransportAPI
{

  class LinkCallback;

  /// Transport
  class Transport
  {
  public:
    /// A Link is used to refer to an active connection.
    class Link;

    /**
     *  @name Configuration methods (synchronous)
     */
    //@{
    virtual void getBLOB(BLOB*& endpoint) const = 0;
    virtual size_t getMaximumBufferSize() const = 0;
    virtual Status isCompatibleEndpoint(BLOB* endpoint) const = 0;
    virtual Status configure(const NVPList& configuration) = 0;
    //@}

    /**
     *  @name Link creation / deletion methods (synchronous)
     */
    //@{
    virtual Link* createLink();
    virtual void destroyLink(Link* link);
    //@}

    class Link
    {
    public:
      /**
       *  @name Configuration methods (synchronous)
       */
      //@{
      virtual Status setCallback(LinkCallback* callback) = 0;
      //@}

      /**
       *  @name Connection methods (asynchronous)
       */
      //@{
      virtual Status connect(BLOB* endpoint, const Id& requestId) = 0;
      virtual Status disconnect(const Id& requestId);
      //@}

      /**
       *  @name Data-related methods (asynchronous)
       */
      //@{
      virtual Status send(const iovec buffers[], size_t iovecSize, const Id& requestId) = 0;
      //@}

    protected:
      virtual ~Link() {}
    };

  protected:
    virtual ~Transport() {}
  };
}

#endif /* TRANSPORTAPI_TRANSPORT_H */
