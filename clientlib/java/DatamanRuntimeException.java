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

class DatamanRuntimeException extends RuntimeException   {
    DatamanRuntimeException(String string)
    {
         super(string);
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
