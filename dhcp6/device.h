/*
 *	DHCP6 supplicant
 *
 *	Copyright (C) 2010-2012 Olaf Kirch <okir@suse.de>
 *	Copyright (C) 2012 Marius Tomaschewski <mt@suse.de>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License along
 *	with this program; if not, see <http://www.gnu.org/licenses/> or write
 *	to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *	Boston, MA 02110-1301 USA.
 *
 */
#ifndef __WICKED_DHCP6_DEVICE_H__
#define __WICKED_DHCP6_DEVICE_H__

/* device functions used in fsm.c and protocol.c */
extern int		ni_dhcp6_device_transmit_init(ni_dhcp6_device_t *);
extern int		ni_dhcp6_device_transmit_start(ni_dhcp6_device_t *);
extern int		ni_dhcp6_device_retransmit(ni_dhcp6_device_t *);

#if 0
//extern int		ni_dhcp6_device_transmit(ni_dhcp6_device_t *);

int			ni_dhcp6_device_retransmit_start(ni_dhcp6_device_t *);
//extern void		ni_dhcp6_device_retransmit_arm(ni_dhcp6_device_t *);
#endif
extern void		ni_dhcp6_device_retransmit_disarm(ni_dhcp6_device_t *);

extern unsigned int	ni_dhcp6_device_uptime(const ni_dhcp6_device_t *, unsigned int);
extern int		ni_dhcp6_device_iaid(const ni_dhcp6_device_t *dev, uint32_t *iaid);

/* config access [/etc/wicked/config.xml, node /config/addrconf/dhcp6] */
extern const char *	ni_dhcp6_config_default_duid(ni_opaque_t *);
extern int		ni_dhcp6_config_user_class(ni_string_array_t *);
extern int		ni_dhcp6_config_vendor_class(unsigned int *, ni_string_array_t *);
extern int		ni_dhcp6_config_vendor_opts(unsigned int *, ni_var_array_t *);
extern int		ni_dhcp6_config_ignore_server(struct in6_addr);
extern int		ni_dhcp6_config_have_server_preference(void);
extern int		ni_dhcp6_config_server_preference(struct in6_addr *, ni_opaque_t *);
extern unsigned int	ni_dhcp6_config_max_lease_time(void);

#endif /* __WICKED_DHCP6_DEVICE_H__ */
