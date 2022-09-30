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

#ifndef __EN50221_APPLICATION_SAS_H__
#define __EN50221_APPLICATION_SAS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>
#include <libdvben50221/en50221_app_utils.h>

#define EN50221_APP_SAS_OC_RESOURCEID MKRID(144,1,2)
#define EN50221_APP_SAS_CIPLUS_RESOURCEID MKRID(150,64,1)

/**
 * Type definition for connection conf - called when we receive a SAS_connection_cnf reply from a CAM.
 *
 * @param arg Private argument.
 * @param slot_id Slot id concerned.
 * @param session_number Session number concerned.
 * @param pvt_host_app_id Unique identifier of the private host application
 * @param sas_session_status Connection status.
 * @return 0 on success, -1 on failure.
 */
typedef int (*en50221_app_sas_con_cnf_callback) (void *arg,
					      uint8_t slot_id,
					      uint16_t session_number,
					      uint64_t pvt_host_app_id,
						  uint8_t sas_session_status);

/**
 * Type definition for async msg - called when we receive a SAS_async_msg
 *
 * @param arg Private argument.
 * @param slot_id Slot id concerned.
 * @param msg_nb Message number.
 * @param msg_len Length of msg_buff in bytes.
 * @param msg_buff Message buffer of length <msg_len>.
 * @return 0 on success, -1 on failure.
 */
typedef int (*en50221_app_sas_async_msg_callback) (void *arg,
					      uint8_t slot_id,
					      uint16_t session_number,
					      uint8_t msg_nb,
						  uint16_t msg_len,
						  uint8_t* msg_buff);

/**
 * Opaque type representing a SAS resource.
 */
struct en50221_app_sas;

/**
 * Create an instance of the SAS resource.
 *
 * @param funcs Send functions to use.
 * @return Instance, or NULL on failure.
 */
extern struct en50221_app_sas *en50221_app_sas_create(struct en50221_app_send_functions *funcs);

/**
 * Destroy an instance of the dvb resource.
 *
 * @param dvb Instance to destroy.
 */
extern void en50221_app_sas_destroy(struct en50221_app_sas *sas);

/**
 * Register the callback for when we receive a connection conf reply.
 *
 * @param sas SAS resource instance.
 * @param callback The callback. Set to NULL to remove the callback completely.
 * @param arg Private data passed as arg0 of the callback.
 */
extern void en50221_app_sas_register_con_conf_callback(struct en50221_app_sas *sas,
						   en50221_app_sas_con_cnf_callback callback,
						   void *arg);

/**
 * Register the callback for when we receive a async message from CAM
 *
 * @param sas SAS resource instance.
 * @param callback The callback. Set to NULL to remove the callback completely.
 * @param arg Private data passed as arg0 of the callback.
 */
extern void en50221_app_sas_register_async_msg_callback(struct en50221_app_sas *sas,
						   en50221_app_sas_async_msg_callback callback,
						   void *arg);

/**
 * Send a connection request to the CAM.
 *
 * @param sas SAS resource instance.
 * @param session_number Session number to send it on.
 * @param pvt_host_app_id Unique identifier of the private host application
 * @return 0 on success, -1 on failure.
 */
extern int en50221_app_sas_con_req(struct en50221_app_sas *sas,
			      uint16_t session_number,
				  uint64_t pvt_host_app_id);

/**
 * Send an async message to the CAM.
 *
 * @param sas SAS resource instance.
 * @param session_number Session number to send it on.
 * @param pvt_host_app_id Unique identifier of the private host application
 * @return 0 on success, -1 on failure.
 */
extern int en50221_app_sas_async_msg(struct en50221_app_sas *sas,
					uint16_t session_number,
					uint8_t msg_nb,
					uint16_t msg_len,
					uint8_t* msg_buff);

/**
 * Pass data received for this resource into it for parsing.
 *
 * @param sas SAS resource instance.
 * @param slot_id Slot ID concerned.
 * @param session_number Session number concerned.
 * @param resource_id Resource ID concerned.
 * @param data The data.
 * @param data_length Length of data in bytes.
 * @return 0 on success, -1 on failure.
 */
extern int en50221_app_sas_message(struct en50221_app_sas *sas,
					uint8_t slot_id,
					uint16_t session_number,
					uint32_t resource_id,
					uint8_t *data,
					uint32_t data_length);

#ifdef __cplusplus
}
#endif
#endif
