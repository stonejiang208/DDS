/*
 * $Id$
 */
 
package org.opendds.jms.common.beans.spi;

import org.opendds.jms.common.beans.UnsupportedTypeException;

/**
 * @author  Steven Stallion
 * @version $Revision$
 */
public class BooleanType implements Type<Boolean> {

    public Class<Boolean> getType() {
        return Boolean.class;
    }

    public Boolean defaultValue() {
        return false;
    }

    public Boolean valueOf(Object o) {
        if (o instanceof Number) {
            return ((Number) o).intValue() != 0;

        } else if (o instanceof String) {
            return Boolean.valueOf((String) o);
        }

        throw new UnsupportedTypeException(o);
    }
}
