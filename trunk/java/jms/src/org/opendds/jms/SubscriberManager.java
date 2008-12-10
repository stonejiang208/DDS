/*
 * $Id$
 */

package org.opendds.jms;

import java.util.Arrays;

import javax.jms.JMSException;
import javax.resource.ResourceException;

import DDS.DomainParticipant;
import DDS.Subscriber;
import DDS.SubscriberQosHolder;
import OpenDDS.DCPS.transport.AttachStatus;
import OpenDDS.DCPS.transport.TransportImpl;

import org.opendds.jms.common.ExceptionHelper;
import org.opendds.jms.common.PartitionHelper;
import org.opendds.jms.common.util.Logger;
import org.opendds.jms.qos.QosPolicies;
import org.opendds.jms.qos.SubscriberQosPolicy;
import org.opendds.jms.resource.ConnectionRequestInfoImpl;
import org.opendds.jms.resource.ManagedConnectionImpl;
import org.opendds.jms.transport.TransportManager;

/**
 * @author  Steven Stallion
 * @version $Revision$
 */
public class SubscriberManager {
    private ManagedConnectionImpl connection;
    private ConnectionRequestInfoImpl cxRequestInfo;
    private TransportManager transportManager;
    private Subscriber remoteSubscriber;
    private Subscriber localSubscriber;

    public SubscriberManager(ManagedConnectionImpl connection) throws ResourceException {
        assert connection != null;

        this.connection = connection;
        this.cxRequestInfo = connection.getConnectionRequestInfo();
        this.transportManager = new TransportManager(cxRequestInfo.getSubscriberTransport());
    }

    protected Subscriber createSubscriber(boolean noLocal) throws JMSException {
        try {
            Logger logger = connection.getLogger();

            SubscriberQosHolder holder =
                new SubscriberQosHolder(QosPolicies.newSubscriberQos());

            DomainParticipant participant = connection.getParticipant();
            participant.get_default_subscriber_qos(holder);

            SubscriberQosPolicy policy = cxRequestInfo.getSubscriberQosPolicy();
            policy.setQos(holder.value);

            // Set PARTITION QosPolicy to support the noLocal client
            // specifier on created MessageConsumer instances:
            if (noLocal) {
                holder.value.partition = PartitionHelper.negate(connection.getConnectionId());
            } else {
                holder.value.partition = PartitionHelper.matchAll();
            }

            Subscriber subscriber = participant.create_subscriber(holder.value, null);
            if (subscriber == null) {
                throw new JMSException("Unable to create Subscriber; please check logs");
            }
            logger.debug("Created %s %s", subscriber, policy);
            logger.debug("%s using PARTITION %s", subscriber, Arrays.deepToString(holder.value.partition.name));

            TransportImpl transport = transportManager.getTransport();
            if (transport.attach_to_subscriber(subscriber) != AttachStatus.ATTACH_OK) {
                throw new JMSException("Unable to attach to Transport; please check logs");
            }
            logger.debug("Attached %s to %s", subscriber, transport);

            return subscriber;

        } catch (Exception e) {
            throw ExceptionHelper.notify(connection, e);
        }
    }

    public synchronized Subscriber getLocalSubscriber() throws JMSException {
        if (localSubscriber == null) {
            localSubscriber = createSubscriber(false);
        }
        return localSubscriber;
    }

    public synchronized Subscriber getRemoteSubscriber() throws JMSException {
        if (remoteSubscriber == null) {
            remoteSubscriber = createSubscriber(true);
        }
        return remoteSubscriber;
    }
}
