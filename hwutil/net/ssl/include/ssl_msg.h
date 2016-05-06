#ifndef _SSL_MSG_H_
#define _SSL_MSG_H_

#include "rtos.h"

/*
http://www.freesoft.org/CIE/Topics/ssl-draft/3-SPEC.HTM

struct __ssl_msg {
	uint8_t msg_type;
	uint24_t length;
	select (msg_type): __body {
		1. client hello(client->server) {
			struct __protocol_version {
				uint8_t major;
				uint8_t minor;
			}version;
			struct __random {
				uint32_t unix_time; // not necessarily correct
				uint8_t random_byte[28];
			}random;
			struct __session {
				uint8_t id_number;
				uint8_t id[<0..32>]; //provided by server
			}session;
			struct __cipher {
				uint16_t cipher_number; //cipher_data bytes
				struct __cipher_suite {
					uint8_t cipher_data[2];
				}cipher[<2..(2^16 - 1)>];
			}
			struct __compression {
				uint8_t methods_numers; //methods_numers >= 1
				uint8_t algorithm(NULL:0);
				//all implementations must support CompressionMethod.null
				struct __methods {
					uint8_t algorithm;
				}methods[<0..(2^8 - 2)>];
				//if add CompressionMethod.null ,methods[<1...(2^8 - 1)>]
			}compression;
		}

		2. server hello(server->client) {
			struct __protocol_version {
				uint8_t major;
				uint8_t minor;
			}version;
			struct __random {
				uint32_t unix_time; // not necessarily correct
				uint8_t random_byte[28];
			}random;
			struct __session {
				uint8_t id_number;
				uint8_t id[<0..32>]; //provided by server
			}session;
			struct __cipher {
				uint8_t cipher_key;
			}
			struct __compression {
				uint8_t algorithm;
			}compression;
		}

		3. 	client key exchange message (client->server){
				select (exchage_algorithm): __exchange_key {
					1. RSA {
						uint16_t secret_bytes; // encrypt secret
						struct __secret {
							struct __protocol_version {
								uint8_t major;
								uint8_t minor;
							}version;
							uint8_t random[46];
						}secret;
					}
				}exchange_key;
			}
		}

		4. certificate request (server->client){


		}
	}body;
}ssl_msg;

 */

struct __ssl {
	u8 msg_type;
	u8 protocol;
};

#endif

