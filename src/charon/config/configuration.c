/**
 * @file configuration.c
 * 
 * @brief Implementation of configuration_t.
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

#include <stdlib.h>
#include <math.h>

#include "configuration.h"

#include <library.h>

/**
 * Timeout in milliseconds after that a half open IKE_SA gets deleted.
 */
#define HALF_OPEN_IKE_SA_TIMEOUT 30000

/**
 * Retransmission uses a backoff algorithm. The timeout is calculated using
 * TIMEOUT * (BASE ** try).
 * When try reaches TRIES, retransmission is given up.
 *
 * Using an initial TIMEOUT of 4s, a BASE of 1.8, and 5 TRIES gives us:
 *
 *                        | relative | absolute
 * ---------------------------------------------------------
 * 4s * (1.8 ** (0  % 5)) =    4s         4s
 * 4s * (1.8 ** (1  % 5)) =    7s        11s
 * 4s * (1.8 ** (2  % 5)) =   13s        24s
 * 4s * (1.8 ** (3  % 5)) =   23s        47s
 * 4s * (1.8 ** (4  % 5)) =   42s        89s
 * 4s * (1.8 ** (5  % 5)) =   76s       165s
 *
 * The peer is considered dead after 2min 45s when no reply comes in.
 */

/**
 * First retransmit timeout in milliseconds.
 * Timeout value is increasing in each retransmit round.
 */
#define RETRANSMIT_TIMEOUT 4000

/**
 * Base which is raised to the power of the retransmission count.
 */
#define RETRANSMIT_BASE 1.8

/**
 * Number of retransmits done in a retransmit sequence
 */
#define RETRANSMIT_TRIES 5

/**
 * Keepalive interval in seconds.
 */
#define KEEPALIVE_INTERVAL 20

/**
 * retry interval in seconds.
 */
#define RETRY_INTERVAL 30

/**
 * jitter to user for retrying
 */
#define RETRY_JITTER 20


typedef struct private_configuration_t private_configuration_t;

/**
 * Private data of an configuration_t object.
 */
struct private_configuration_t {

	/**
	 * Public part of configuration_t object.
	 */
	configuration_t public;

};

/**
 * Implementation of configuration_t.get_retransmit_timeout.
 */
static u_int32_t get_retransmit_timeout (private_configuration_t *this,
										 u_int32_t retransmit_count)
{
	if (retransmit_count > RETRANSMIT_TRIES)
	{
		/* give up */
		return 0;
	}
	return (u_int32_t)
				(RETRANSMIT_TIMEOUT * pow(RETRANSMIT_BASE, retransmit_count));
}

/**
 * Implementation of configuration_t.get_half_open_ike_sa_timeout.
 */
static u_int32_t get_half_open_ike_sa_timeout (private_configuration_t *this)
{
	return HALF_OPEN_IKE_SA_TIMEOUT;
}

/**
 * Implementation of configuration_t.get_keepalive_interval.
 */
static u_int32_t get_keepalive_interval (private_configuration_t *this)
{
	return KEEPALIVE_INTERVAL;
}

/**
 * Implementation of configuration_t.get_retry_interval.
 */
static u_int32_t get_retry_interval (private_configuration_t *this)
{
	return RETRY_INTERVAL - (random() % RETRY_JITTER);
}

/**
 * Implementation of configuration_t.destroy.
 */
static void destroy(private_configuration_t *this)
{
	free(this);
}

/*
 * Described in header-file
 */
configuration_t *configuration_create()
{
	private_configuration_t *this = malloc_thing(private_configuration_t);
	
	/* public functions */
	this->public.destroy = (void(*)(configuration_t*))destroy;
	this->public.get_retransmit_timeout = (u_int32_t (*) (configuration_t*,u_int32_t))get_retransmit_timeout;
	this->public.get_half_open_ike_sa_timeout = (u_int32_t (*) (configuration_t*)) get_half_open_ike_sa_timeout;
	this->public.get_keepalive_interval = (u_int32_t (*) (configuration_t*)) get_keepalive_interval;
	this->public.get_retry_interval = (u_int32_t (*) (configuration_t*)) get_retry_interval;
	
	return (&this->public);
}
