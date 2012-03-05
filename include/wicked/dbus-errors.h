/*
 * DBus errors
 *
 * Copyright (C) 2012 Olaf Kirch <okir@suse.de>
 */

#ifndef __WICKED_DBUS_ERRORS_H__
#define __WICKED_DBUS_ERRORS_H__

#include <dbus/dbus.h>

#define __NI_DBUS_ERROR(x)		"com.suse.Wicked." #x

#define NI_DBUS_ERROR_PERMISSION_DENIED		__NI_DBUS_ERROR(PermissionDenied)
#define NI_DBUS_ERROR_INTERFACE_NOT_KNOWN	__NI_DBUS_ERROR(InterfaceNotKnown)
#define NI_DBUS_ERROR_INTERFACE_BAD_HIERARCHY	__NI_DBUS_ERROR(InterfaceBadHierarchy)
#define NI_DBUS_ERROR_INTERFACE_IN_USE		__NI_DBUS_ERROR(InterfaceInUse)
#define NI_DBUS_ERROR_INTERFACE_NOT_UP		__NI_DBUS_ERROR(InterfaceNotUp)
#define NI_DBUS_ERROR_INTERFACE_NOT_DOWN	__NI_DBUS_ERROR(InterfaceNotDown)
#define NI_DBUS_ERROR_INTERFACE_NOT_COMPATIBLE	__NI_DBUS_ERROR(InterfaceNotCompatible)
#define NI_DBUS_ERROR_INTERFACE_EXISTS		__NI_DBUS_ERROR(InterfaceExists)
#define NI_DBUS_ERROR_AUTH_INFO_MISSING		__NI_DBUS_ERROR(AuthInfoMissing)
#define NI_DBUS_ERROR_ADDRCONF_NO_LEASE		__NI_DBUS_ERROR(AddrconfNoLease)
#define NI_DBUS_ERROR_CANNOT_CONFIGURE_ADDRESS	__NI_DBUS_ERROR(CannotConfigureAddress)
#define NI_DBUS_ERROR_CANNOT_CONFIGURE_ROUTE	__NI_DBUS_ERROR(CannotConfigureRoute)
#define NI_DBUS_ERROR_CANNOT_MARSHAL		__NI_DBUS_ERROR(CannotMarshal)
#define NI_DBUS_ERROR_PROPERTY_NOT_PRESENT	__NI_DBUS_ERROR(PropertyNotPresent)

/* Map dbus error strings to our internal error codes and vice versa */
extern int		ni_dbus_get_error(const DBusError *error, char **detail);
extern void		ni_dbus_set_error_from_code(DBusError *, int, const char *fmt, ...);

static inline dbus_bool_t
ni_dbus_error_property_not_present(DBusError *error, const char *path, const char *property)
{
	dbus_set_error(error, NI_DBUS_ERROR_PROPERTY_NOT_PRESENT,
			"%s property %s not set", path, property);
	return FALSE;
}

static inline dbus_bool_t
ni_dbus_error_invalid_args(DBusError *error, const char *path, const char *method)
{
	dbus_set_error(error, DBUS_ERROR_INVALID_ARGS,
			"bad arguments in call to %s.%s()", path, method);
	return FALSE;
}

#endif /* __WICKED_DBUS_ERRORS_H__ */
