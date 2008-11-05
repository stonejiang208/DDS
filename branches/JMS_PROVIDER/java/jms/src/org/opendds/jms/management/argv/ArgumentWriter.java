/*
 * $Id$
 */

package org.opendds.jms.management.argv;

import java.util.ArrayList;
import java.util.List;
import java.util.StringTokenizer;

import org.opendds.jms.management.DynamicMBeanSupport;
import org.opendds.jms.util.Strings;

/**
 * @author  Steven Stallion
 * @version $Revision$
 */
public class ArgumentWriter {
    private DynamicMBeanSupport instance;

    private List<String> args =
        new ArrayList<String>();

    public ArgumentWriter(DynamicMBeanSupport instance) {
        this.instance = instance;
    }

    public void writeIfSet(String arg, String attribute) throws Exception {
        Object value = instance.getAttribute(attribute);
        if (value != null) {
            write(arg, value);
        }
    }

    public void writeDelimited(String attribute) throws Exception {
        String value = (String) instance.getAttribute(attribute);

        if (!Strings.isEmpty(value)) {
            StringTokenizer stok = new StringTokenizer(value, DynamicArguments.DELIMS);

            while (stok.hasMoreTokens()) {
                write(stok.nextToken());
            }
        }
    }

    public void write(String arg, Object value) {
        write(arg);

        if (!(value instanceof Boolean)) {
            write(value.toString());
        }
    }

    public void write(String value) {
        args.add(value);
    }

    public void writeTo(List<String> args) {
        args.addAll(this.args);
    }
}