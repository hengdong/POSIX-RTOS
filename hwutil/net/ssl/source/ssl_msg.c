

#include "ssl_msg.h"

/**
 *
 */
#define SSL_SET_MSG_TYPE(pbuf, type) pbuf[0] = type
#define SSL_SET_PROTOCOL_VERSION(pbuf, ver)
/*************************************************/

/**
 *
 */
INLINE err_t __ssl_set_msg(ssl_t *ssl,
		                   char *pbuf,
		                   int buffer_len)
{
	SSL_SET_MSG_TYPE(pbuf, ssl->msg_type);

}
