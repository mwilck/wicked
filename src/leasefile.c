/*
 *	wicked addrconf lease file utilities
 *
 *	Copyright (C) 2010-2014 SUSE LINUX Products GmbH, Nuernberg, Germany.
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
 *	Authors:
 *		Olaf Kirch <okir@suse.de>
 *		Karol Mroz <kmroz@suse.com>
 *		Marius Tomaschewski <mt@suse.de>
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <limits.h>
#include <unistd.h>

#include <wicked/netinfo.h>
#include <wicked/addrconf.h>
#include <wicked/resolver.h>
#include <wicked/nis.h>
#include <wicked/route.h>
#include <wicked/logging.h>
#include <wicked/xml.h>

#include "leasefile.h"
#include "dhcp4/lease.h"
#include "dhcp6/lease.h"
#include "netinfo_priv.h"


/*
 * utility returning a family + type specific node / name
 */
static const char *
__ni_addrconf_lease_xml_new_type_name(unsigned int family, unsigned int type)
{
	switch (family) {
	case AF_INET:
		switch (type) {
		case NI_ADDRCONF_DHCP:
			return NI_ADDRCONF_LEASE_XML_DHCP4_NODE;
		case NI_ADDRCONF_STATIC:
			return NI_ADDRCONF_LEASE_XML_STATIC4_NODE;
		case NI_ADDRCONF_AUTOCONF:
			return NI_ADDRCONF_LEASE_XML_AUTO4_NODE;
		default: ;
		}
		break;
	case AF_INET6:
		switch (type) {
		case NI_ADDRCONF_DHCP:
			return NI_ADDRCONF_LEASE_XML_DHCP6_NODE;
		case NI_ADDRCONF_STATIC:
			return NI_ADDRCONF_LEASE_XML_STATIC6_NODE;
		case NI_ADDRCONF_AUTOCONF:
			return NI_ADDRCONF_LEASE_XML_AUTO6_NODE;
		default: ;
		}
		break;
	default: ;
	}
	return NULL;
}

const char *
ni_addrconf_lease_xml_new_type_name(const ni_addrconf_lease_t *lease)
{
	if (!lease)
		return NULL;
	return __ni_addrconf_lease_xml_new_type_name(lease->family, lease->type);
}

xml_node_t *
ni_addrconf_lease_xml_new_type_node(const ni_addrconf_lease_t *lease,
					xml_node_t *node)
{
	const char *name = NULL;

	name = ni_addrconf_lease_xml_new_type_name(lease);
	return name ? xml_node_new(name, node) : NULL;
}

const xml_node_t *
ni_addrconf_lease_xml_get_type_node(const ni_addrconf_lease_t *lease,
					const xml_node_t *node)
{
	const char *name = NULL;

	name = ni_addrconf_lease_xml_new_type_name(lease);
	return name ? xml_node_get_child(node, name) : NULL;
}

/*
 * utils to dump lease or a part of to xml
 */
static int
__ni_string_array_to_xml(const ni_string_array_t *array, const char *name, xml_node_t *node)
{
	unsigned int i, count = 0;

	for (i = 0; i < array->count; ++i) {
		const char *item = array->data[i];
		if (ni_string_empty(item))
			continue;
		count++;
		xml_node_new_element(name, node, item);
	}
	return count ? 0 : 1;
}

int
ni_addrconf_lease_routes_data_to_xml(const ni_addrconf_lease_t *lease, xml_node_t *node)
{
	ni_route_table_t *tab;
	ni_route_nexthop_t *nh;
	xml_node_t *route, *hop;
	ni_route_t *rp;
	unsigned int count = 0;
	unsigned int i;

	/* A very limitted view */
	for (tab = lease->routes; tab; tab = tab->next) {
		if (tab->tid != 254) /* RT_TABLE_MAIN for now */
			continue;

		for (i = 0; i < tab->routes.count; ++i) {
			if (!(rp = tab->routes.data[i]))
				continue;

			route = xml_node_new("route", NULL);
			if (ni_sockaddr_is_specified(&rp->destination)) {
				xml_node_new_element("destination", route,
					ni_sockaddr_prefix_print(&rp->destination,
								rp->prefixlen));
			}
			for (nh = &rp->nh; nh; nh = nh->next) {
				if (!ni_sockaddr_is_specified(&nh->gateway))
					continue;

				hop = xml_node_new("nexthop", route);
				xml_node_new_element("gateway", hop,
					ni_sockaddr_print(&nh->gateway));
			}
			if (route->children) {
				xml_node_add_child(node, route);
				count++;
			} else {
				xml_node_free(route);
			}
		}
	}
	return count ? 0 : 1;
}

int
ni_addrconf_lease_dns_data_to_xml(const ni_addrconf_lease_t *lease, xml_node_t *node)
{
	ni_resolver_info_t *dns;
	unsigned int count = 0;

	dns = lease->resolver;
	if (!dns || (ni_string_empty(dns->default_domain) &&
			dns->dns_servers.count == 0 &&
			dns->dns_search.count == 0))
		return 1;

	if (dns->default_domain) {
		xml_node_new_element("domain", node, dns->default_domain);
		count++;
	}
	if (__ni_string_array_to_xml(&dns->dns_servers, "server", node) == 0)
		count++;
	if (__ni_string_array_to_xml(&dns->dns_search, "search", node) == 0)
		count++;

	return count ? 0 : 1;
}

int
ni_addrconf_lease_nis_data_to_xml(const ni_addrconf_lease_t *lease, xml_node_t *node)
{
	unsigned int count = 0;
	unsigned int i, j;
	ni_nis_info_t *nis;
	xml_node_t *data;

	nis = lease->nis;
	if (!nis)
		return 1;

	/* Default domain */
	data = xml_node_new("default", NULL);
	if (!ni_string_empty(nis->domainname)) {
		count++;
		xml_node_new_element("domain", data, nis->domainname);
	}
	if (nis->default_binding == NI_NISCONF_BROADCAST ||
	    nis->default_binding == NI_NISCONF_STATIC) {
		/* no SLP here */
		count++;
		xml_node_new_element("binding", data,
			ni_nis_binding_type_to_name(nis->default_binding));
	}
	/* Only in when static binding? */
	for (i = 0; i < nis->default_servers.count; ++i) {
		const char *server = nis->default_servers.data[i];
		if (ni_string_empty(server))
			continue;
		count++;
		xml_node_new_element("server", data, server);
	}
	if (count) {
		xml_node_add_child(node, data);
	}

	/* Further domains */
	for (i = 0; i < nis->domains.count; ++i) {
		ni_nis_domain_t *dom = nis->domains.data[i];
		if (!dom || ni_string_empty(dom->domainname))
			continue;

		count++;
		data = xml_node_new("domain", node);
		xml_node_new_element("domain", data, dom->domainname);
		if (ni_nis_binding_type_to_name(nis->default_binding)) {
			xml_node_new_element("binding", data,
				ni_nis_binding_type_to_name(nis->default_binding));
		}
		for (j = 0; j < dom->servers.count; ++j) {
			const char *server = dom->servers.data[j];
			if (ni_string_empty(server))
				continue;
			xml_node_new_element("server", data, server);
		}
	}

	return count ? 0 : 1;
}

int
ni_addrconf_lease_ntp_data_to_xml(const ni_addrconf_lease_t *lease, xml_node_t *node)
{
	return __ni_string_array_to_xml(&lease->ntp_servers, "server", node);
}

int
ni_addrconf_lease_nds_data_to_xml(const ni_addrconf_lease_t *lease, xml_node_t *node)
{
	unsigned int count = 0;

	if (__ni_string_array_to_xml(&lease->nds_servers, "server", node) == 0)
		count++;
	if (__ni_string_array_to_xml(&lease->nds_context, "context", node) == 0)
		count++;
	if (!ni_string_empty(lease->nds_tree)) {
		count++;
		xml_node_new_element("tree", node, lease->nds_tree);
	}
	return count ? 0 : 1;
}

int
ni_addrconf_lease_smb_data_to_xml(const ni_addrconf_lease_t *lease, xml_node_t *node)
{
	unsigned int count = 0;

	if (__ni_string_array_to_xml(&lease->netbios_name_servers, "nbns-server", node) == 0)
		count++;
	if (__ni_string_array_to_xml(&lease->netbios_dd_servers, "nmdd-server", node) == 0)
		count++;
	if (!ni_string_empty(lease->netbios_scope)) {
		count++;
		xml_node_new_element("scope", node, lease->netbios_scope);
	}
	if (lease->netbios_type) {
		count++;
		xml_node_new_element_uint("type", node, lease->netbios_type);
	}
	return count ? 0 : 1;
}

int
ni_addrconf_lease_sip_data_to_xml(const ni_addrconf_lease_t *lease, xml_node_t *node)
{
	return __ni_string_array_to_xml(&lease->sip_servers, "server", node);
}

int
ni_addrconf_lease_slp_data_to_xml(const ni_addrconf_lease_t *lease, xml_node_t *node)
{
	unsigned int count = 0;

	if (__ni_string_array_to_xml(&lease->slp_scopes, "scopes", node) == 0)
		count++;
	if (__ni_string_array_to_xml(&lease->slp_servers, "server", node) == 0)
		count++;
	return count ? 0 : 1;
}

int
ni_addrconf_lease_lpr_data_to_xml(const ni_addrconf_lease_t *lease, xml_node_t *node)
{
	return __ni_string_array_to_xml(&lease->lpr_servers, "server", node);
}

int
ni_addrconf_lease_log_data_to_xml(const ni_addrconf_lease_t *lease, xml_node_t *node)
{
	return __ni_string_array_to_xml(&lease->log_servers, "server", node);
}

int
__ni_addrconf_lease_info_to_xml(const ni_addrconf_lease_t *lease, xml_node_t *node)
{
	xml_node_new_element("family", node, ni_addrfamily_type_to_name(lease->family));
	xml_node_new_element("type", node, ni_addrconf_type_to_name(lease->type));
	if (!ni_string_empty(lease->owner))
		xml_node_new_element("owner", node, lease->owner);
	if (!ni_uuid_is_null(&lease->uuid))
		xml_node_new_element("uuid", node, ni_uuid_print(&lease->uuid));
	xml_node_new_element("state", node, ni_addrconf_state_to_name(lease->state));
	xml_node_new_element_uint("acquired", node, lease->time_acquired);
	return 0;
}

static int
__ni_addrconf_lease_dhcp_to_xml(const ni_addrconf_lease_t *lease, xml_node_t *node)
{
	switch (lease->family) {
	case AF_INET:
		return ni_dhcp4_lease_to_xml(lease, node);
	case AF_INET6:
		return ni_dhcp6_lease_to_xml(lease, node);
	default:
		return -1;
	}
}

int
ni_addrconf_lease_to_xml(const ni_addrconf_lease_t *lease, xml_node_t *root)
{
	xml_node_t *node;
	int ret = -1;

	if (!lease || !root)
		return -1;

	node = xml_node_new(NI_ADDRCONF_LEASE_XML_NODE, NULL);
	switch (lease->type) {
	case NI_ADDRCONF_DHCP:
		if ((ret = __ni_addrconf_lease_info_to_xml(lease, node)) != 0)
			break;

		if ((ret = __ni_addrconf_lease_dhcp_to_xml(lease, node)) != 0)
			break;

		xml_node_add_child(root, node);
		return 0;

	case NI_ADDRCONF_STATIC:
	case NI_ADDRCONF_AUTOCONF:
	case NI_ADDRCONF_INTRINSIC:
		return 1;	/* unsupported / skip */
	default: ;		/* fall through error */
	}

	if (ret) {
		xml_node_free(node);
	}
	return ret;
}

/*
 * utils to parse lease or a lease data group from xml
 */
static int
__ni_addrconf_lease_info_from_xml(ni_addrconf_lease_t *lease, const xml_node_t *node)
{
	xml_node_t *child;
	int value;

	if (!lease || !node)
		return -1;

	for (child = node->children; child; child = child->next) {
		if (ni_string_eq(child->name, "family")) {
			if ((value = ni_addrfamily_name_to_type(child->cdata)) == -1)
				return -1;
			lease->family = value;
		} else
		if (ni_string_eq(child->name, "type")) {
			if ((value = ni_addrconf_name_to_type(child->cdata)) == -1)
				return -1;
			lease->type = value;
		} else
		if (ni_string_eq(child->name, "owner")) {
			if (!ni_string_empty(child->cdata))
				ni_string_dup(&lease->owner, child->cdata);
		} else
		if (ni_string_eq(child->name, "uuid")) {
			if (ni_uuid_parse(&lease->uuid, child->cdata) != 0)
				return -1;
		} else
		if (ni_string_eq(child->name, "acquired")) {
			if (ni_parse_uint(child->cdata, &lease->time_acquired, 10) != 0)
				return -1;
		}
	}
	return 0;
}

static int
__ni_addrconf_lease_dhcp_from_xml(ni_addrconf_lease_t *lease, const xml_node_t *node)
{
	switch (lease->family) {
	case AF_INET:
		return ni_dhcp4_lease_from_xml(lease, node);
	case AF_INET6:
		return ni_dhcp6_lease_from_xml(lease, node);
	default:
		return -1;
	}
}

static int
__ni_addrconf_lease_route_nh_from_xml(ni_route_t *rp, const xml_node_t *node)
{
	const xml_node_t *child;
	ni_route_nexthop_t *nh;
	ni_sockaddr_t addr;

	for (child = node->children; child; child = child->next) {
		if (ni_string_eq(child->name, "gateway") && child->cdata) {
			if (ni_sockaddr_parse(&addr, child->cdata, AF_UNSPEC) != 0)
				return -1;
			if (rp->family != addr.ss_family)
				return -1;
			if (nh == NULL) {
				nh = ni_route_nexthop_new();
				ni_route_nexthop_list_append(&rp->nh.next, nh);
			}
			nh->gateway = addr;
			nh = NULL;
		}
	}
	return 0;
}

static int
__ni_addrconf_lease_route_from_xml(ni_route_t *rp, const xml_node_t *node)
{
	const xml_node_t *child;
	ni_sockaddr_t addr;
	unsigned int plen;

	for (child = node->children; child; child = child->next) {
		if (ni_string_eq(child->name, "destination") && child->cdata) {
			if (!ni_sockaddr_prefix_parse(child->cdata, &addr, &plen))
				return -1;
			if (rp->family != addr.ss_family)
				return -1;
			if ((rp->family == AF_INET  && plen > 32) ||
			    (rp->family == AF_INET6 && plen > 128))
				return -1;
			rp->destination = addr;
			rp->prefixlen = plen;
		} else
		if (ni_string_eq(child->name, "nexthop")) {
			if (__ni_addrconf_lease_route_nh_from_xml(rp, child) != 0)
				return -1;
		}
	}
	return 0;
}

int
ni_addrconf_lease_routes_data_from_xml(ni_addrconf_lease_t *lease, const xml_node_t *node)
{
	const xml_node_t *child;
	ni_route_t *rp;

	for (child = node->children; child; child = child->next) {
		if (ni_string_eq(child->name, "route")) {
			rp = ni_route_new();
			rp->family = lease->family;
			rp->table = ni_route_guess_table(rp);
			if (__ni_addrconf_lease_route_from_xml(rp, child) != 0) {
				ni_route_free(rp);
			} else
			if (!ni_route_tables_add_route(&lease->routes, rp)) {
				ni_route_free(rp);
				return -1;
			}
		}
	}
	return 0;
}

int
ni_addrconf_lease_dns_data_from_xml(ni_addrconf_lease_t *lease, const xml_node_t *node)
{
	ni_resolver_info_t *dns;
	const xml_node_t *child;

	if (!(dns = ni_resolver_info_new()))
		return -1;

	if (lease->resolver) {
		ni_resolver_info_free(lease->resolver);
		lease->resolver = NULL;
	}

	for (child = node->children; child; child = child->next) {
		if (ni_string_eq(child->name, "domain") &&
				!ni_string_empty(child->cdata)) {
			ni_string_dup(&dns->default_domain, child->cdata);
		} else
		if (ni_string_eq(child->name, "server") &&
				!ni_string_empty(child->cdata)) {
			ni_string_array_append(&dns->dns_servers, child->cdata);
		} else
		if (ni_string_eq(child->name, "search") &&
				!ni_string_empty(child->cdata)) {
			ni_string_array_append(&dns->dns_search, child->cdata);
		}
	}

	if (ni_string_empty(dns->default_domain) &&
			dns->dns_servers.count == 0 &&
			dns->dns_search.count == 0) {
		ni_resolver_info_free(dns);
		return 1;
	}
	lease->resolver = dns;
	return 0;
}

int
ni_addrconf_lease_dns_from_xml(ni_addrconf_lease_t *lease, const xml_node_t *node)
{
	if (!(node = xml_node_get_child(node, "dns")))
		return 1;
	return ni_addrconf_lease_dns_data_from_xml(lease, node);
}

int
__ni_addrconf_lease_nis_domain_from_xml(ni_nis_info_t *nis, const xml_node_t *node)
{
	const xml_node_t *child;
	ni_nis_domain_t *dom = NULL;

	for (child = node->children; child; child = child->next) {
		if (ni_string_eq(child->name, "domain") && child->cdata) {
			if (!(dom = ni_nis_domain_find(nis, child->cdata)))
				dom = ni_nis_domain_new(nis, child->cdata);
			else
				return -1;
		}
	}
	if (dom) {
		for (child = node->children; child; child = child->next) {
			if (ni_string_eq(child->name, "binding") &&
				!ni_string_empty(child->cdata)) {
				int b = ni_nis_binding_name_to_type(child->cdata);
				if (b != -1) {
					dom->binding = (unsigned int)b;
				}
			}
			if (ni_string_eq(child->name, "server")) {
				if (!ni_string_empty(child->cdata)) {
					ni_string_array_append(&dom->servers,
								child->cdata);
				}
			}
		}
	}
	return dom ? 0 : 1;
}

int
ni_addrconf_lease_nis_data_from_xml(ni_addrconf_lease_t *lease, const xml_node_t *node)
{
	const xml_node_t *child, *gc;
	ni_nis_info_t *nis;

	if (!(nis = ni_nis_info_new()))
		return -1;

	if (lease->nis) {
		ni_nis_info_free(lease->nis);
		lease->nis = NULL;
	}

	nis->default_binding = NI_NISCONF_STATIC;
	for (child = node->children; child; child = child->next) {
		if (ni_string_eq(child->name, "default")) {
			for (gc = child->children; gc; gc = gc->next) {
				if (ni_string_eq(gc->name, "domain") &&
					!ni_string_empty(gc->cdata)) {
					ni_string_dup(&nis->domainname, gc->cdata);
				} else
				if (ni_string_eq(gc->name, "binding") &&
					ni_string_eq(gc->cdata, "broadcast")) {
					nis->default_binding = NI_NISCONF_BROADCAST;
				} else
				if (ni_string_eq(gc->name, "server") &&
					!ni_string_empty(gc->cdata)) {
					ni_string_array_append(&nis->default_servers,
								gc->cdata);
				}
			}
		} else
		if (ni_string_eq(child->name, "domain")) {
			if (__ni_addrconf_lease_nis_domain_from_xml(nis, child) != 0)
				continue;
		}
	}

	if (nis->default_binding == NI_NISCONF_STATIC &&
	    ni_string_empty(nis->domainname) &&
	    nis->default_servers.count == 0 &&
	    nis->domains.count == 0) {
		ni_nis_info_free(nis);
		return 1;
	}
	lease->nis = nis;
	return 0;
}

int
ni_addrconf_lease_nds_data_from_xml(ni_addrconf_lease_t *lease, const xml_node_t *node)
{
	const xml_node_t *child;

	for (child = node->children; child; child = child->next) {
		if (ni_string_eq(child->name, "tree") && !ni_string_empty(child->cdata)) {
			ni_string_dup(&lease->nds_tree, child->cdata);
		} else
		if (ni_string_eq(child->name, "server") && !ni_string_empty(child->cdata)) {
			ni_string_array_append(&lease->nds_servers, child->cdata);
		} else
		if (ni_string_eq(child->name, "context") && !ni_string_empty(child->cdata)) {
			ni_string_array_append(&lease->nds_context, child->cdata);
		}
	}
	return 0;
}

int
ni_addrconf_lease_smb_data_from_xml(ni_addrconf_lease_t *lease, const xml_node_t *node)
{
	const xml_node_t *child;
	unsigned int value;

	for (child = node->children; child; child = child->next) {
		if (ni_string_eq(child->name, "type") && child->cdata) {
			if (ni_parse_uint(child->cdata, &value, 10) != 0 || value > 8)
				return -1;
			lease->netbios_type = value;
		} else
		if (ni_string_eq(child->name, "scope") && !ni_string_empty(child->cdata)) {
			ni_string_dup(&lease->netbios_scope, child->cdata);
		} else
		if (ni_string_eq(child->name, "name-server") && !ni_string_empty(child->cdata)) {
			ni_string_array_append(&lease->netbios_name_servers, child->cdata);
		} else
		if (ni_string_eq(child->name, "dd-server") && !ni_string_empty(child->cdata)) {
			ni_string_array_append(&lease->netbios_dd_servers, child->cdata);
		}
	}
	return 0;
}

static int
__ni_string_array_from_xml(ni_string_array_t *array, const char *name, const xml_node_t *node)
{
	const xml_node_t *child;

	for (child = node->children; child; child = child->next) {
		if (ni_string_eq(child->name, name) && !ni_string_empty(child->cdata)) {
			ni_string_array_append(array, child->cdata);
		}
	}
	return 0;
}

int
ni_addrconf_lease_ntp_data_from_xml(ni_addrconf_lease_t *lease, const xml_node_t *node)
{
	return __ni_string_array_from_xml(&lease->ntp_servers, "server", node);
}

int
ni_addrconf_lease_slp_data_from_xml(ni_addrconf_lease_t *lease, const xml_node_t *node)
{
	int ret;

	if ((ret = __ni_string_array_from_xml(&lease->slp_servers, "server", node)) != 0)
		return ret;
	if ((ret = __ni_string_array_from_xml(&lease->slp_scopes, "scope", node)) != 0)
		return ret;
	return 0;
}

int
ni_addrconf_lease_sip_data_from_xml(ni_addrconf_lease_t *lease, const xml_node_t *node)
{
	return __ni_string_array_from_xml(&lease->sip_servers, "server", node);
}

int
ni_addrconf_lease_log_data_from_xml(ni_addrconf_lease_t *lease, const xml_node_t *node)
{
	return __ni_string_array_from_xml(&lease->log_servers, "server", node);
}

int
ni_addrconf_lease_lpr_data_from_xml(ni_addrconf_lease_t *lease, const xml_node_t *node)
{
	return __ni_string_array_from_xml(&lease->lpr_servers, "server", node);
}

int
ni_addrconf_lease_from_xml(ni_addrconf_lease_t **leasep, const xml_node_t *root)
{
	const xml_node_t *node = root;
	ni_addrconf_lease_t *lease;
	int ret = -1;

	if (root && !ni_string_eq(root->name, NI_ADDRCONF_LEASE_XML_NODE))
		node = xml_node_get_child(root, NI_ADDRCONF_LEASE_XML_NODE);

	if (!node || !leasep)
		return ret;

	if (*leasep) {
		ni_addrconf_lease_free(*leasep);
		*leasep = NULL;
	}

	if (!(lease  = ni_addrconf_lease_new(__NI_ADDRCONF_MAX, AF_UNSPEC)))
		return ret;

	if ((ret = __ni_addrconf_lease_info_from_xml(lease, node)) != 0) {
		ni_addrconf_lease_free(lease);
		return ret;
	}

	switch (lease->type) {
	case NI_ADDRCONF_DHCP:
		ret = __ni_addrconf_lease_dhcp_from_xml(lease, node);
		break;

	case NI_ADDRCONF_STATIC:
	case NI_ADDRCONF_AUTOCONF:
	case NI_ADDRCONF_INTRINSIC:
		ret = 1;	/* unsupported / skip */
		break;
	default: ;		/* fall through error */
	}

	if (ret) {
		ni_addrconf_lease_free(lease);
	} else {
		*leasep = lease;
	}
	return ret;
}

/*
 * lease file read and write routines
 */
static const char *		__ni_addrconf_lease_file_path(int, int, const char *);

#if 1
int
ni_addrconf_lease_file_write(const char *ifname, ni_addrconf_lease_t *lease)
{
	ni_error("%s: currently not implemented", __func__);
	return -1;
}

ni_addrconf_lease_t *
ni_addrconf_lease_file_read(const char *ifname, int type, int family)
{
	ni_error("%s: currently not implemented", __func__);
	return NULL;
}
#else
/*
 * Write a lease to a file
 */
int
ni_addrconf_lease_file_write(const char *ifname, ni_addrconf_lease_t *lease)
{
	const char *filename;
	xml_node_t *xml = NULL;
	FILE *fp;

	filename = __ni_addrconf_lease_file_path(lease->type, lease->family, ifname);
	if (lease->state == NI_ADDRCONF_STATE_RELEASED) {
		ni_debug_dhcp("removing %s", filename);
		return unlink(filename);
	}

	ni_debug_dhcp("writing lease to %s", filename);
	if (ni_netcf_lease_to_xml(lease, NULL, &xml) < 0) {
		ni_error("cannot store lease: unable to represent lease as XML");
		goto failed;
	}

	if ((fp = fopen(filename, "w")) == NULL) {
		ni_error("unable to open %s for writing: %m", filename);
		goto failed;
	}

	xml_node_print(xml, fp);
	fclose(fp);

	xml_node_free(xml);
	return 0;

failed:
	if (xml)
		xml_node_free(xml);
	unlink(filename);
	return -1;
}

/*
 * Read a lease from a file
 */
ni_addrconf_lease_t *
ni_addrconf_lease_file_read(const char *ifname, int type, int family)
{
	ni_addrconf_lease_t *lease;
	const char *filename;
	xml_node_t *xml = NULL, *lnode;
	FILE *fp;

	filename = __ni_addrconf_lease_file_path(type, family, ifname);

	ni_debug_dhcp("reading lease from %s", filename);
	if ((fp = fopen(filename, "r")) == NULL) {
		if (errno != ENOENT)
			ni_error("unable to open %s for reading: %m", filename);
		return NULL;
	}

	xml = xml_node_scan(fp);
	fclose(fp);

	if (xml == NULL) {
		ni_error("unable to parse %s", filename);
		return NULL;
	}

	if (xml->name == NULL)
		lnode = xml->children;
	else
		lnode = xml;
	if (!lnode || !lnode->name || strcmp(lnode->name, "lease")) {
		ni_error("%s: does not contain a lease", filename);
		xml_node_free(xml);
		return NULL;
	}

	if (ni_netcf_xml_to_lease(lnode, &lease) < 0) {
		ni_error("%s: unable to parse lease xml", filename);
		xml_node_free(xml);
		return NULL;
	}

	xml_node_free(xml);
	return lease;
}
#endif

/*
 * Remove a lease file
 */
void
ni_addrconf_lease_file_remove(const char *ifname, int type, int family)
{
	const char *filename;

	filename = __ni_addrconf_lease_file_path(type, family, ifname);
	ni_debug_dhcp("removing %s", filename);
	unlink(filename);
}

static const char *
__ni_addrconf_lease_file_path(int type, int family, const char *ifname)
{
	static char pathname[PATH_MAX];

	snprintf(pathname, sizeof(pathname), "%s/lease-%s-%s-%s.xml",
			ni_config_statedir(),
			ni_addrconf_type_to_name(type),
			ni_addrfamily_type_to_name(family),
			ifname);
	return pathname;
}

