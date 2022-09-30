/*
    en50221 encoder An implementation for libdvb
    an implementation for the en50221 transport layer

    Copyright (C) 2004, 2005 Manu Abraham <abraham.manu@gmail.com>
    Copyright (C) 2005 Julian Scheel (julian at jusst dot de)
    Copyright (C) 2006 Andrew de Quincey (adq_dvb@lidskialf.net)
    Copyright (C) 2022 Morten Møller Jørgensen (mmj@triax.com)

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as
    published by the Free Software Foundation; either version 2.1 of
    the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
*/

#include <string.h>
#include <libdvbmisc/dvbmisc.h>
#include <pthread.h>
#include "en50221_app_sas.h"
#include "en50221_app_tags.h"
#include "asn_1.h"

struct en50221_app_sas {
	struct en50221_app_send_functions *funcs;

	en50221_app_sas_con_cnf_callback con_conf_callback;
	void *con_conf_callback_arg;

	en50221_app_sas_async_msg_callback async_msg_callback;
	void *async_msg_callback_arg;

	pthread_mutex_t lock;
};

static int en50221_app_sas_parse_con_conf(struct en50221_app_sas *sas,
				      uint8_t slot_id,
				      uint16_t session_number,
				      uint8_t * data,
				      uint32_t data_length);

static int en50221_app_sas_parse_async_msg(struct en50221_app_sas *sas,
					 uint8_t slot_id,
					 uint16_t session_number,
					 uint8_t * data,
					 uint32_t data_length);

uint32_t en50221_app_sas_calc_async_buff(uint16_t async_msg_len)
{
	if (async_msg_len <= 124) {
		// 3 bytes tag + length_field 1 byte + 3 bytes + async_ msg_len
		return 7 + async_msg_len;
	} else if (async_msg_len <= 65532) {
		// 3 bytes tag + length_field 3 byte + 3 bytes + async_ msg_len
		return 9 + async_msg_len;
	} else {
		// 3 bytes tag + length_field 4 byte + 3 bytes + async_ msg_len
		return 10 + async_msg_len;
	}
}

struct en50221_app_sas *en50221_app_sas_create(struct en50221_app_send_functions *funcs)
{
	struct en50221_app_sas *sas = NULL;

	// create structure and set it up
	sas = malloc(sizeof(struct en50221_app_sas));
	if (sas == NULL) {
		return NULL;
	}
	sas->funcs = funcs;
	sas->con_conf_callback = NULL;
	sas->async_msg_callback = NULL;

	pthread_mutex_init(&sas->lock, NULL);

	// done
	return sas;
}

void en50221_app_sas_destroy(struct en50221_app_sas *sas)
{
	pthread_mutex_destroy(&sas->lock);
	free(sas);
}

void en50221_app_sas_register_con_conf_callback(struct en50221_app_sas *sas,
						   en50221_app_sas_con_cnf_callback callback,
						   void *arg)
{
	pthread_mutex_lock(&sas->lock);
	sas->con_conf_callback = callback;
	sas->con_conf_callback_arg = arg;
	pthread_mutex_unlock(&sas->lock);
}

void en50221_app_sas_register_async_msg_callback(struct en50221_app_sas *sas,
						   en50221_app_sas_async_msg_callback callback,
						   void *arg)
{
	pthread_mutex_lock(&sas->lock);
	sas->async_msg_callback = callback;
	sas->async_msg_callback_arg = arg;
	pthread_mutex_unlock(&sas->lock);
}

int en50221_app_sas_con_req(struct en50221_app_sas *sas,
			      uint16_t session_number,
				  uint64_t pvt_host_app_id)
{
	uint8_t data[12];
	data[0] = (TAG_SAS_CON_REQ >> 16) & 0xFF;
	data[1] = (TAG_SAS_CON_REQ >> 8) & 0xFF;
	data[2] = TAG_SAS_CON_REQ & 0xFF;
	data[3] = 8;

	data[4] = (pvt_host_app_id >> 56) & 0xFF;
	data[5] = (pvt_host_app_id >> 48) & 0xFF;
	data[6] = (pvt_host_app_id >> 40) & 0xFF;
	data[7] = (pvt_host_app_id >> 32) & 0xFF;
	data[8] = (pvt_host_app_id >> 24) & 0xFF;
	data[9] = (pvt_host_app_id >> 16) & 0xFF;
	data[10] = (pvt_host_app_id >> 8) & 0xFF;
	data[11] = pvt_host_app_id & 0xFF;

	return sas->funcs->send_data(sas->funcs->arg, session_number, data, 12);
}

int en50221_app_sas_async_msg(struct en50221_app_sas *sas,
					uint16_t session_number,
					uint8_t msg_nb,
					uint16_t msg_len,
					uint8_t* msg_buff)
{
	uint8_t buff[10];
	struct iovec vector[2];
	uint8_t* data = NULL;
	uint8_t* msg_ptr = NULL;
	int length_field_len = -1;

	// Check msg_len - library only support asn_1 length <= 0xFFFF
	if (msg_len > 65526) {
		print(LOG_LEVEL, ERROR, 1, "Invalid msg_len. Cannot send SAS_async_msg\n");
		return -1;
	}

	buff[0] = (TAG_SAS_ASYNC_MSG >> 16) & 0xFF;
	buff[1] = (TAG_SAS_ASYNC_MSG >> 8) & 0xFF;
	buff[2] = TAG_SAS_ASYNC_MSG & 0xFF;

	if ((length_field_len = asn_1_encode(msg_len + 3,&buff[3],7)) <= 0) {
		print(LOG_LEVEL, ERROR, 1, "Failed encoding asn_1 length\n");
		return -1;
	}

	data = buff + 3 + length_field_len;
	data[0] = msg_nb;
	data[1] = (msg_len >> 8) & 0xFF;
	data[2] = msg_len & 0xFF;
	vector[0].iov_base = buff;
	// 3 bytes tag + length_field + 3 bytes data (msg_nb )
	vector[0].iov_len = 3 + length_field_len + 3;

	vector[1].iov_len = msg_len;
	vector[1].iov_base = msg_buff;

	return sas->funcs->send_datav(sas->funcs->arg, session_number,vector,2);
}

int en50221_app_sas_message(struct en50221_app_sas *sas,
			    uint8_t slot_id,
			    uint16_t session_number,
			    uint32_t resource_id,
			    uint8_t * data, uint32_t data_length)
{
	(void) resource_id;

	// get the tag
	if (data_length < 3) {
		print(LOG_LEVEL, ERROR, 1, "Received short data\n");
		return -1;
	}
	uint32_t tag = (data[0] << 16) | (data[1] << 8) | data[2];

	switch (tag) {
	case TAG_SAS_CON_CNF:
		return en50221_app_sas_parse_con_conf(sas, slot_id,
						  session_number, data + 3,
						  data_length - 3);
	case TAG_SAS_ASYNC_MSG:
		return en50221_app_sas_parse_async_msg(sas, slot_id,
						     session_number,
						     data + 3,
						     data_length - 3);
	}

	print(LOG_LEVEL, ERROR, 1, "Received unexpected tag %x\n", tag);
	return -1;
}

static int en50221_app_sas_parse_con_conf(struct en50221_app_sas *sas,
				      uint8_t slot_id,
				      uint16_t session_number,
				      uint8_t * data,
				      uint32_t data_length)
{
	uint64_t pvt_host_app_id = 0;
	uint8_t sas_session_status = 0;

	// validate data
	if (data_length < 10) {
		print(LOG_LEVEL, ERROR, 1, "Received short data\n");
		return -1;
	}
	if (data[0] != 9) {
		print(LOG_LEVEL, ERROR, 1, "Received short data\n");
		return -1;
	}

	// parse it
	pvt_host_app_id = ((uint64_t)data[1] << 56);
	pvt_host_app_id |= ((uint64_t)data[2] << 48);
	pvt_host_app_id |= ((uint64_t)data[3] << 40);
	pvt_host_app_id |= ((uint64_t)data[4] << 32);
	pvt_host_app_id |= ((uint64_t)data[5] << 24);
	pvt_host_app_id |= ((uint64_t)data[6] << 16);
	pvt_host_app_id |= ((uint64_t)data[7] << 8);
	pvt_host_app_id |= (uint64_t)data[8];
	sas_session_status = data[9];

	// tell the app
	pthread_mutex_lock(&sas->lock);
	en50221_app_sas_con_cnf_callback cb = sas->con_conf_callback;
	void *cb_arg = sas->con_conf_callback_arg;
	pthread_mutex_unlock(&sas->lock);
	if (cb) {
		return cb(cb_arg, slot_id, session_number, 
			pvt_host_app_id,sas_session_status);
	}
	return 0;
}

static int en50221_app_sas_parse_async_msg(struct en50221_app_sas *sas,
					 uint8_t slot_id,
					 uint16_t session_number,
					 uint8_t * data,
					 uint32_t data_length)
{
	uint8_t *msg_buff;
	uint16_t msg_len;
	uint8_t msg_nb;
	uint16_t asn_data_length;
	int length_field_len;

	// first of all, decode the length field
	if ((length_field_len = asn_1_decode(&asn_data_length, data, data_length)) < 0) {
		print(LOG_LEVEL, ERROR, 1, "ASN.1 decode error\n");
		return -1;
	}
	// check it
	if (asn_data_length > (data_length - length_field_len)) {
		print(LOG_LEVEL, ERROR, 1, "Received short data\n");
		return -1;
	}
	if (asn_data_length < 1) {
		print(LOG_LEVEL, ERROR, 1, "Received short data\n");
		return -1;
	}
	// skip over the length field
	data += length_field_len;

	// parse it
	msg_nb = data[0];
	msg_len = (uint16_t)(data[1] << 8);
	msg_len |= (uint16_t)data[2];
	msg_buff = &data[3];

	// check msg_len
	if (msg_len > (data_length - (length_field_len + 3))) {
		print(LOG_LEVEL, ERROR, 1, "Received invalid msg_len in SAS_async_msg\n");
		return -1;
	}

	// tell the app
	pthread_mutex_lock(&sas->lock);
	en50221_app_sas_async_msg_callback cb = sas->async_msg_callback;
	void *cb_arg = sas->async_msg_callback_arg;
	pthread_mutex_unlock(&sas->lock);
	if (cb) {
		return cb(cb_arg, slot_id, session_number,msg_nb,msg_len,msg_buff);
	}
	return 0;
}
