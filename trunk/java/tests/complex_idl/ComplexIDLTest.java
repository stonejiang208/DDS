/*
 * $Id$
 */

import DDS.*;
import OpenDDS.DCPS.*;
import OpenDDS.DCPS.transport.*;

import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

import org.omg.CORBA.StringSeqHolder;

import Complex.*;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.util.ArrayList;
import java.util.List;

/**
 * @author  Steven Stallion
 * @version $Revision$
 */
public class ComplexIDLTest extends QuoteSupport {
    private static final int DOMAIN_ID = 42;
    
    private static DomainParticipantFactory dpf;
    private static DomainParticipant participant;

    private static Topic topic;
    
    private static Publisher publisher;
    private static Subscriber subscriber;
    
    protected static void setUp(String[] args) {
        dpf = TheParticipantFactory.WithArgs(new StringSeqHolder(args));
        assert (dpf != null);
        
        participant = dpf.create_participant(DOMAIN_ID, PARTICIPANT_QOS_DEFAULT.get(), null);
        assert (participant != null);
        
        DataTypeSupport typeSupport = new DataTypeSupportImpl();
        
        int result = typeSupport.register_type(participant, "Complex::Data");
        assert (result != RETCODE_ERROR.value);

        topic = participant.create_topic("Complex::Topic", typeSupport.get_type_name(),
                                         TOPIC_QOS_DEFAULT.get(), null);
        assert (topic != null);

        publisher = participant.create_publisher(PUBLISHER_QOS_DEFAULT.get(), null);
        assert (publisher != null);
        
        AttachStatus status;
        
        TransportImpl transport1 = 
            TheTransportFactory.create_transport_impl(1, TheTransportFactory.AUTO_CONFIG);
        assert (transport1 != null);
        
        status = transport1.attach_to_publisher(publisher);
        assert (status.value() != AttachStatus._ATTACH_ERROR);
        
        subscriber = participant.create_subscriber(SUBSCRIBER_QOS_DEFAULT.get(), null);
        assert (subscriber != null);
        
        TransportImpl transport2 =
            TheTransportFactory.create_transport_impl(2, TheTransportFactory.AUTO_CONFIG);
        assert (transport2 != null);
        
        status = transport2.attach_to_subscriber(subscriber);
        assert (status.value() != AttachStatus._ATTACH_ERROR);
    }
    
    protected static void testQuotes() throws Exception {
        final AtomicInteger count = new AtomicInteger();
        
        final Lock lock = new ReentrantLock();
        final Condition finished = lock.newCondition();
        
        publisher.create_datawriter(topic, DATAWRITER_QOS_DEFAULT.get(),
            new DDS._DataWriterListenerLocalBase() {
                public void on_liveliness_lost(DataWriter dw, LivelinessLostStatus status) {}

                public void on_offered_deadline_missed(DataWriter dw, OfferedDeadlineMissedStatus status) {}

                public void on_offered_incompatible_qos(DataWriter dw, OfferedIncompatibleQosStatus status) {}

                public void on_publication_match(DataWriter dw, PublicationMatchStatus status) {
                    try {
                        assert (status.total_count > 0);

                        DataDataWriter writer = DataDataWriterHelper.narrow(dw);

                        //NOTE: Since we are testing a complex type which contains a
                        //      union, both variants (DATA_IDL, DATA_STREAM) must be
                        //      tested on the same set of data:
                        
                        List<Data> dataItems = new ArrayList<Data>();
                        
                        for (Quote quote : quotes) {
                            dataItems.add(createData(quote));
                        }
                        for (Quote quote : quotes) {
                            ByteArrayOutputStream out = new ByteArrayOutputStream();
                            
                            ObjectOutputStream os = new ObjectOutputStream(out);
                            os.writeObject(quote.line); // Quote is not Serializable
                            
                            dataItems.add(createData(out.toByteArray()));
                        }
                        
                        count.set(dataItems.size());
                        
                        for (Data data : dataItems) {
                            int result = writer.write(data, HANDLE_NIL.value);
                            assert (result != RETCODE_ERROR.value);
                        }
                        
                    } catch (Throwable t) {
                        t.printStackTrace();
                    }
                };
            }
        );

        lock.lock();
        try {
            subscriber.create_datareader(topic, DATAREADER_QOS_DEFAULT.get(),
                new DDS._DataReaderListenerLocalBase() {
                    public void on_liveliness_changed(DataReader dr, LivelinessChangedStatus status) {}

                    public void on_requested_deadline_missed(DataReader dr, RequestedDeadlineMissedStatus status) {}

                    public void on_requested_incompatible_qos(DataReader dr, RequestedIncompatibleQosStatus status) {}

                    public void on_sample_lost(DataReader dr, SampleLostStatus status) {}

                    public void on_sample_rejected(DataReader dr, SampleRejectedStatus status) {}

                    public void on_subscription_match(DataReader dr, SubscriptionMatchStatus status) {}

                    public void on_data_available(DataReader dr) {
                        try {
                            DataDataReader reader = DataDataReaderHelper.narrow(dr);

                            DataHolder dh = new DataHolder(createDefaultData());

                            SampleInfo si = new SampleInfo();
                            si.source_timestamp = new Time_t();

                            int result = reader.take_next_sample(dh, new SampleInfoHolder(si));
                            assert (result != RETCODE_ERROR.value);

                            Data data = dh.value;

                            switch (data.payload.discriminator().value()) {
                                case DataType._DATA_IDL:
                                    printQuote(data.payload.idl_quote());
                                    break;

                                case DataType._DATA_STREAM:
                                    ByteArrayInputStream in =
                                        new ByteArrayInputStream(data.payload.stream());

                                    ObjectInputStream os = new ObjectInputStream(in);
                                    Object obj = os.readObject();
                                    
                                    assert (obj instanceof String);
                            }

                            if (count.decrementAndGet() == 0) {
                                // Signal main thread
                                lock.lock();
                                try {
                                    finished.signalAll();
                                } finally {
                                    lock.unlock();
                                }
                            }
                            
                        } catch (Throwable t) {
                            t.printStackTrace();
                        }
                    }
                }
            );
        
            // Wait for DataReader
            finished.await();
            
        } finally {
            lock.unlock();
        }
    }

    public static void main(String[] args) throws Exception {
        setUp(args);
        try {
            testQuotes();
            
        } finally {
            tearDown();
        }
    }

    protected static void tearDown() {
        participant.delete_contained_entities();
        dpf.delete_participant(participant);
        
        TheTransportFactory.release();
        TheServiceParticipant.shutdown();
        
        System.out.println("(Those responsible have been sacked.)");
    }
    
    //

    private static Data createData(byte[] bytes) {
        Data data = new Data();
        data.payload = new DataUnion();
        
        data.payload.stream(bytes);
        
        return data;
    }
    
    private static Data createData(Quote quote) {
        Data data = new Data();
        data.payload = new DataUnion();
                    
        data.payload.idl_quote(quote);

        return data;
    }
    
    private static Data createDefaultData() {
        Quote quote = new Quote();
        quote.cast_member = new CastMember();
        
        return createData(quote);
    }
}