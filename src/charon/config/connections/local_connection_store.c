/**
 * @file local_connection_store.c
 *
 * @brief Implementation of local_connection_store_t.
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

#include <string.h>

#include "local_connection_store.h"

#include <daemon.h>
#include <utils/linked_list.h>


typedef struct private_local_connection_store_t private_local_connection_store_t;

/**
 * Private data of an local_connection_store_t object
 */
struct private_local_connection_store_t {

	/**
	 * Public part
	 */
	local_connection_store_t public;
	
	/**
	 * stored connection
	 */
	linked_list_t *connections;
	
	/**
	 * Mutex to exclusivly access connection list
	 */
	pthread_mutex_t mutex;
};


/**
 * Implementation of connection_store_t.get_connection_by_hosts.
 */
static connection_t *get_connection_by_hosts(private_local_connection_store_t *this, host_t *my_host, host_t *other_host)
{
	typedef enum {
		PRIO_UNDEFINED=		0x00,
		PRIO_ADDR_ANY= 		0x01,
		PRIO_ADDR_MATCH=	0x02
	} prio_t;

	prio_t best_prio = PRIO_UNDEFINED;

	iterator_t *iterator;
	connection_t *candidate;
	connection_t *found = NULL;
	
	DBG2(DBG_CFG, "looking for connection for host pair %H...%H",
		 my_host, other_host);
	
	pthread_mutex_lock(&(this->mutex));
	iterator = this->connections->create_iterator(this->connections, TRUE);
	/* determine closest matching connection */
	while (iterator->iterate(iterator, (void**)&candidate))
	{
		host_t *candidate_my_host;
		host_t *candidate_other_host;

		candidate_my_host = candidate->get_my_host(candidate);
		candidate_other_host = candidate->get_other_host(candidate);

		/* my_host addresses must match*/
		if (my_host->ip_equals(my_host, candidate_my_host))
		{
			prio_t prio = PRIO_UNDEFINED;

			/* exact match of peer host address or wildcard address? */
			if (other_host->ip_equals(other_host, candidate_other_host))
			{
				prio |= PRIO_ADDR_MATCH;
			}
			else if (candidate_other_host->is_anyaddr(candidate_other_host))
			{
				prio |= PRIO_ADDR_ANY;
			}

			DBG2(DBG_CFG, "candidate connection \"%s\": %H...%H (prio=%d)",
				 candidate->get_name(candidate),
				 candidate_my_host, candidate_other_host, prio);

			if (prio > best_prio)
			{
				found = candidate;
				best_prio = prio;
			}
		}
	}
	iterator->destroy(iterator);
	
	if (found)
	{
		DBG2(DBG_CFG, "found matching connection \"%s\": %H...%H (prio=%d)",
			 found->get_name(found), found->get_my_host(found),
			 found->get_other_host(found), best_prio);
		
		/* give out a new reference to it */
		found->get_ref(found);
	}
	pthread_mutex_unlock(&(this->mutex));
	return found;
}

/**
 * Implementation of connection_store_t.get_connection_by_name.
 */
static connection_t *get_connection_by_name(private_local_connection_store_t *this, char *name)
{
	iterator_t *iterator;
	connection_t *current, *found = NULL;
	
	pthread_mutex_lock(&(this->mutex));
	iterator = this->connections->create_iterator(this->connections, TRUE);
	while (iterator->iterate(iterator, (void**)&current))
	{
		if (strcmp(name, current->get_name(current)) == 0)
		{
			found = current;
			break;
		}
	}
	iterator->destroy(iterator);
	pthread_mutex_unlock(&(this->mutex));
	
	if (found)
	{
		/* get a new reference for it */
		found->get_ref(found);
	}
	return found;
}

/**
 * Implementation of connection_store_t.delete_connection.
 */
static status_t delete_connection(private_local_connection_store_t *this, char *name)
{
	iterator_t *iterator;
	connection_t *current;
	bool found = FALSE;
	
	pthread_mutex_lock(&(this->mutex));
	iterator = this->connections->create_iterator(this->connections, TRUE);
	while (iterator->iterate(iterator, (void **)&current))
	{
		if (strcmp(current->get_name(current), name) == 0)
		{
			/* remove connection from list, and destroy it */
			iterator->remove(iterator);
			current->destroy(current);
			found = TRUE;
			break;
		}
	}
	iterator->destroy(iterator);
	pthread_mutex_unlock(&(this->mutex));
	if (found)
	{
		return SUCCESS;
	}
	return NOT_FOUND;
}

/**
 * Implementation of connection_store_t.add_connection.
 */
static status_t add_connection(private_local_connection_store_t *this, connection_t *connection)
{
	pthread_mutex_lock(&(this->mutex));
	this->connections->insert_last(this->connections, connection);
	pthread_mutex_unlock(&(this->mutex));
	return SUCCESS;
}

/**
 * Implementation of connection_store_t.create_iterator.
 */
static iterator_t* create_iterator(private_local_connection_store_t *this)
{
	return this->connections->create_iterator_locked(this->connections,
													 &this->mutex);
}

/**
 * Implementation of connection_store_t.destroy.
 */
static void destroy (private_local_connection_store_t *this)
{
	pthread_mutex_lock(&(this->mutex));
	this->connections->destroy_offset(this->connections, offsetof(connection_t, destroy));
	pthread_mutex_unlock(&(this->mutex));
	free(this);
}

/**
 * Described in header.
 */
local_connection_store_t * local_connection_store_create(void)
{
	private_local_connection_store_t *this = malloc_thing(private_local_connection_store_t);

	this->public.connection_store.get_connection_by_hosts = (connection_t*(*)(connection_store_t*,host_t*,host_t*))get_connection_by_hosts;
	this->public.connection_store.get_connection_by_name = (connection_t*(*)(connection_store_t*,char*))get_connection_by_name;
	this->public.connection_store.delete_connection = (status_t(*)(connection_store_t*,char*))delete_connection;
	this->public.connection_store.add_connection = (status_t(*)(connection_store_t*,connection_t*))add_connection;
	this->public.connection_store.create_iterator = (iterator_t*(*)(connection_store_t*))create_iterator;
	this->public.connection_store.destroy = (void(*)(connection_store_t*))destroy;
	
	/* private variables */
	this->connections = linked_list_create();
	pthread_mutex_init(&(this->mutex), NULL);

	return (&this->public);
}
