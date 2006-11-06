/* Mode Config related functions
 * Copyright (C) 2001-2002 Colubris Networks
 * Copyright (C) 2003-2004 Xelerance Corporation
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
 *
 * RCSID $Id: modecfg.h,v 1.3 2006/10/19 21:07:40 as Exp $
 */

struct state;

/* ModeConfig starting functions */
extern stf_status modecfg_send_request(struct state *st);
extern stf_status modecfg_send_set(struct state *st);

/* ModeConfig state transition functions */
extern stf_status modecfg_inR0(struct msg_digest *md);
extern stf_status modecfg_inR1(struct msg_digest *md);
extern stf_status modecfg_inI1(struct msg_digest *md);
extern stf_status modecfg_inI2(struct msg_digest *md);
