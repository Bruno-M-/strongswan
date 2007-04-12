/**
 * @file configuration.h
 * 
 * @brief Interface configuration_t.
 *  
 */

/*
 * Copyright (C) 2006 Martin Willi
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

#ifndef CONFIGURATION_H_
#define CONFIGURATION_H_

typedef struct configuration_t configuration_t;

#include <library.h>

/**
 * @brief The interface for various daemon related configs.
 * 
 * @b Constructors:
 * 	- configuration_create()
 * 
 * @ingroup config
 */
struct configuration_t {

	/**
	 * @brief Returns the retransmit timeout.
	 *
	 * A return value of zero means the request should not be
	 * retransmitted again.
	 *
	 * @param this				calling object
	 * @param retransmitted		number of times a message was retransmitted so far
	 * @return					time in milliseconds, when to do next retransmit
	 */
	u_int32_t (*get_retransmit_timeout) (configuration_t *this, 
										 u_int32_t retransmitted);
	
	/**
	 * @brief Returns the timeout for an half open IKE_SA in ms.
	 *
	 * Half open means that the IKE_SA is still on a not established state
	 *
	 * @param this				calling object
	 * @return					timeout in milliseconds (ms)
	 */
	u_int32_t (*get_half_open_ike_sa_timeout) (configuration_t *this);

	/**
	 * @brief Returns the keepalive interval in s.
	 * 
	 * The keepalive interval defines the idle time after which a
	 * NAT keepalive packet should be sent.
	 * 
	 * @param this				calling object
	 * @return					interval in s
	 */	
	u_int32_t (*get_keepalive_interval) (configuration_t *this);

	/**
	 * @brief Returns the interval to retry a failed action again.
	 *
	 * In some situations, the protocol may be in a state where processing
	 * is not possible and an action must be retried (e.g. rekeying).
	 * 
	 * @param this				calling object
	 * @return					interval in s
	 */	
	u_int32_t (*get_retry_interval) (configuration_t *this);

	/**
	 * @brief Destroys a configuration_t object.
	 * 
	 * @param this 					calling object
	 */
	void (*destroy) (configuration_t *this);
};

/**
 * @brief Creates a configuration backend.
 * 
 * @return static_configuration_t object
 * 
 * @ingroup config
 */
configuration_t *configuration_create(void);

#endif /*CONFIGURATION_H_*/
