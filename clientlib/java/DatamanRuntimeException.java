// ***************************************************************
//
// CLASS:		DatamanRuntimeException.java
//
// PROJECT:		dataman client side java library
// 
// DATE:		summer, 2004
// 
// AUTHOR:		Tom Green
// 
// FILES:
//
// MODIFICATION HISTORY:
//
//***************************************************************

// throw my errors so that something intelligent can be
// printed out.  well intelligent?
//
package Dataman;

public class DatamanRuntimeException extends RuntimeException {
    private static final long serialVersionUID = 1L;

    public DatamanRuntimeException(String message) {
        super(message);
    }

    public DatamanRuntimeException(String message, Throwable cause) {
        super(message, cause);
    }
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
