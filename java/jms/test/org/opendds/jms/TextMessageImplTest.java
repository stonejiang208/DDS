package org.opendds.jms;

import static org.junit.Assert.*;
import org.junit.Test;
import javax.jms.TextMessage;
import javax.jms.JMSException;
import javax.jms.MessageNotWriteableException;

public class TextMessageImplTest {
    @Test
    public void testNewlyCreatedTextMessage() throws JMSException {
        TextMessage textMessage = new TextMessageImpl();
        assertNull(textMessage.getText());
    }

    @Test
    public void testSetAndGetText() throws JMSException {
        TextMessage textMessage = new TextMessageImpl();
        final String greeting = "Hello OpenDDS JMS Provider";
        textMessage.setText(greeting);
        assertEquals(greeting, textMessage.getText());
    }

    @Test
    public void testClearBody() throws JMSException {
        TextMessage textMessage = new TextMessageImpl();
        final String greeting = "Hello OpenDDS JMS Provider";
        textMessage.setText(greeting);
        assertEquals(greeting, textMessage.getText());

        textMessage.clearBody();
        assertNull(textMessage.getText());
    }

    @Test
    public void testSetTextInNotWritableState() throws JMSException {
        TextMessageImpl textMessage = new TextMessageImpl();
        final String greeting = "Hello OpenDDS JMS Provider";
        textMessage.setText(greeting);
        assertEquals(greeting, textMessage.getText());
        textMessage.setBodyState(new MessageStateBodyNonWritable(textMessage));

        try {
            textMessage.setText("Goodbye OpenDDS JMS Provider");
            fail("Should throw");
        } catch (MessageNotWriteableException e) {
            assertEquals("The message is in a body non-writable state", e.getMessage());
        }
    }

    @Test
    public void testClearBodyInNotWritableState() throws JMSException {
        TextMessageImpl textMessage = new TextMessageImpl();
        final String greeting = "Hello OpenDDS JMS Provider";
        textMessage.setText(greeting);
        assertEquals(greeting, textMessage.getText());
        textMessage.setBodyState(new MessageStateBodyNonWritable(textMessage));

        textMessage.clearBody();
        assertNull(textMessage.getText());

        final String greeting2 = "Goodbye OpenDDS JMS Provider";
        textMessage.setText(greeting2);
        assertEquals(greeting2, textMessage.getText());
    }
}
