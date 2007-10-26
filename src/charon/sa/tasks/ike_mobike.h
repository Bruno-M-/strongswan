/**
 * @file ike_mobike.h
 * 
 * @brief Interface ike_mobike_t.
 * 
 */

/*
 * Copyright (C) 2007 Martin Willi
 * Hochschule fuer Technik Rapperswil
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#ifndef IKE_MOBIKE_H_
#define IKE_MOBIKE_H_

typedef struct ike_mobike_t ike_mobike_t;

#include <library.h>
#include <sa/ike_sa.h>
#include <sa/tasks/task.h>
#include <network/packet.h>

/**
 * @brief Task of type ike_mobike, detects and handles MOBIKE extension.
 *
 * The MOBIKE extension is defined in RFC4555. It allows to update IKE
 * and IPsec tunnel addresses.
 * This tasks handles the MOBIKE_SUPPORTED notify exchange to detect MOBIKE
 * support, allows the exchange of ADDITIONAL_*_ADDRESS to exchange additional
 * endpoints and handles the UPDATE_SA_ADDRESS notify to finally update 
 * endpoints.
 *
 * @b Constructors:
 *  - ike_mobike_create()
 * 
 * @ingroup tasks
 */
struct ike_mobike_t {

	/**
	 * Implements the task_t interface
	 */
	task_t task;
	
	/**
	 * @brief Use the task to roam to other addresses.
	 *
	 * @param this			calling object
	 * @param address		TRUE to include address list update
	 */
	void (*roam)(ike_mobike_t *this, bool address);
	
	/**
	 * @brief Transmision hook, called by task manager.
	 *
	 * The task manager calls this hook whenever it transmits a packet. It 
	 * allows the mobike task to send the packet on multiple paths to do path
	 * probing.
	 *
	 * @param this			calling object
	 * @param packet		the packet to transmit
	 */
	void (*transmit)(ike_mobike_t *this, packet_t *packet);
	
	/**
	 * @brief Check if this task is probing for routability.
	 *
	 * @param this			calling object
	 * @return				TRUE if task is probing
	 */
	bool (*is_probing)(ike_mobike_t *this);	
};

/**
 * @brief Create a new ike_mobike task.
 *
 * @param ike_sa		IKE_SA this task works for
 * @param initiator		TRUE if taks is initiated by us
 * @return			  	ike_mobike task to handle by the task_manager
 */
ike_mobike_t *ike_mobike_create(ike_sa_t *ike_sa, bool initiator);

#endif /* IKE_MOBIKE_H_ */

