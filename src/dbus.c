/*
 * dbus.c
 * Copyright (C) 2015-2016 Ooma Incorporated. All rights reserved.
 */

#include "main.h"

static struct l_dbus *dbus;
static struct l_queue *proxy_queue;

#define PROFILE_VERSION						0x0107
#define PROFILE_NAME						"hfp_recorder"
#define PROFILE_CHANNEL						6

#define DBUS_NAME							"org.hfp.recorder"
#define DBUS_OBJ_PATH						"/org/hfp/recorder"

#define DBUS_BLUEZ_PROFILE_INTERFACE		"org.bluez.Profile1"
#define DBUS_BLUEZ_PROFILE_MANAGER			"org.bluez.ProfileManager1"

static void bluez_client_disconnected(struct l_dbus *dbus, void *user_data)
{

	l_info("bluez disappeared on message bus");
	if (proxy_queue) {
		/* TODO: may need to pass destroy function to free the inserted proxy structures.
		 * Looks like struct l_dbus_proxy that are inserted into queue are dynamically
		 * allocated and need to be freed by application.
		 */
		l_queue_destroy(proxy_queue, NULL);
		proxy_queue = NULL;
	}
}

/* fill the message received in the first argument with the message to be sent. */
static void hfp_registration_msg_setup(struct l_dbus_message *message, void *user_data)
{
	struct l_dbus_message_builder *builder;

	int channel = 6;
	int version = 0x0107;

	/* do we need to call this ? */
	l_dbus_message_set_no_autostart(message, true);

	builder = l_dbus_message_builder_new(message);

	l_dbus_message_builder_enter_struct(builder, "osa{sv}");
	l_dbus_message_builder_append_basic(builder, 'o', DBUS_OBJ_PATH);
	l_dbus_message_builder_append_basic(builder, 's', "hfp-hf");

	l_dbus_message_builder_enter_array(builder, "{sv}");
	l_dbus_message_builder_enter_dict(builder, "sv");
	l_dbus_message_builder_append_basic(builder, 's', "Channel");

	l_dbus_message_builder_enter_variant(builder, "q");
	l_dbus_message_builder_append_basic(builder, 'q', &channel);
	l_dbus_message_builder_leave_variant(builder);

	/* TODO: Do we need a dict here ? */
	l_dbus_message_builder_enter_dict(builder, "{sv}");
	l_dbus_message_builder_append_basic(builder, 's', "Version");

	l_dbus_message_builder_enter_variant(builder, "q");
	l_dbus_message_builder_append_basic(builder, 'q', &version);
	l_dbus_message_builder_leave_variant(builder);

	l_dbus_message_builder_leave_dict(builder);
	l_dbus_message_builder_leave_array(builder);
	l_dbus_message_builder_leave_struct(builder);

	message = l_dbus_message_builder_finalize(builder);

	/* Destroys builder. This may not cause any problem for message
	 * because, message body is already created.
	 */
	l_dbus_message_builder_destroy(builder);
}

static void hfp_registration_msg_reply(struct l_dbus_proxy *proxy,
		struct l_dbus_message *message, void *user_data)
{
	if (l_dbus_message_is_error(message)) {
		const char *name, *text;
		l_dbus_message_get_error(message, &name, &text);
		l_error("Failed registering hfp profile error name: %s error text: %s", name, text);
	} else {
		l_info("hfp profile registered with bluez successfully");
	}
}

static bool register_hfp_service(struct l_dbus_proxy *proxy)
{
	/* Internally calls l_dbus_method_call to call DBus method */
	l_dbus_proxy_method_call(proxy, "RegisterProfile",
			hfp_registration_msg_setup,
			hfp_registration_msg_reply, NULL, NULL);

	return true;
}

/* This method is called for every interface found on the
 * reply of GetManagedObjects method of dbus server that is called.
 */
static void proxy_added(struct l_dbus_proxy *proxy, void *user_data)
{
//	const char *path = l_dbus_proxy_get_path(proxy);
	const char *interface = l_dbus_proxy_get_interface(proxy);

	if (!strcmp(interface, DBUS_BLUEZ_PROFILE_MANAGER)) {
		register_hfp_service(proxy);
		l_queue_push_tail(proxy_queue, proxy);
	} /* TODO: register default agent. */

	/* once a remote bluetooth device is connected,
	 * A new proxy is created and this call back is invoked.
	 */
	l_info("%s", interface);

}

static void proxy_removed(struct l_dbus_proxy *proxy, void *user_data)
{
	const char *path = l_dbus_proxy_get_path(proxy);
	const char *interface = l_dbus_proxy_get_interface(proxy);

	l_info("Proxy removed. object path: %s, interface: %s", path, interface);
}

static void property_changed(struct l_dbus_proxy *proxy,
		const char *name,
		struct l_dbus_message *msg,
		void *user_data)
{
//	const char *path = l_dbus_proxy_get_path(proxy);
//	const char *interface = l_dbus_proxy_get_interface(proxy);
	;
	// Define property change actions here.
}

static void bluez_client_connected(struct l_dbus *dbus, void *user_data)
{
	l_info("bluez client connected");
	if (!proxy_queue)
		proxy_queue = l_queue_new();
}

/* This callback called once requested DBus name is allocated to the application. */
static void name_acquired_callback(struct l_dbus *dbus, bool success,
		bool queued, void *user_data)
{
	struct l_dbus_client *client;

	if (!success) {
		l_error("Failed acquiring message bus");
		return;
	}

	client = l_dbus_client_new(dbus, "org.bluez", "/org/bluez");

	/* Once bluez exits or disconnects from message bus, this callback
	 * gets called.
	 */
	l_dbus_client_set_disconnect_handler(client, bluez_client_disconnected,
								NULL, NULL);

	l_dbus_client_set_connect_handler(client, bluez_client_connected,
			NULL, NULL);

	/* Once GetManagedObjects of the remote service replies, callback
	 * registered with l_dbus_client_set_proxy_handlers is called on
	 * every interface found on the GetManagedObjects reply.
	 */
	l_dbus_client_set_proxy_handlers(client, proxy_added, proxy_removed,
							property_changed, NULL, NULL);

//	l_dbus_proxy_method_call();

}

struct l_dbus_message* new_connection(struct l_dbus *dbus, struct l_dbus_message *message,
		void *user_data)
{
	int sock;
	struct l_dbus_message *reply;

	l_info("%s", __func__);

	if (!l_dbus_message_get_arguments(message, "h", &sock)) {
		l_info("no fd received");
	} else {
		new_rfcomm_connection(sock);
	}

	reply = l_dbus_message_new_method_return(message);
	l_dbus_message_set_arguments(reply, "");

	return reply;
}

struct l_dbus_message* request_disconnection(struct l_dbus *dbus, struct l_dbus_message *message,
		void *user_data)
{
	struct l_dbus_message *reply;

	l_info("%s Method Call", __func__);

	reply = l_dbus_message_new_method_return(message);
	l_dbus_message_set_arguments(reply, "");

	return reply;
}

struct l_dbus_message* release(struct l_dbus *dbus, struct l_dbus_message *message,
		void *user_data)
{
	struct l_dbus_message *reply;

	l_info("Method Call");

	reply = l_dbus_message_new_method_return(message);
	l_dbus_message_set_arguments(reply, "");

	return reply;
}

void dbus_interface_setup(struct l_dbus_interface *interface)
{

	if (!l_dbus_object_manager_enable(dbus)) {
		l_error("Unable to enable DBus ObjectManager");
		return;
	}

	/* flags indicating NO_REPLY and DEPRACATED methods can be set.
	 *
	 */
	l_dbus_interface_method(interface, "NewConnection", 0, new_connection,
			"", "oha{sv}", "device", "fd", "fd_properties");

	l_dbus_interface_method(interface, "RequestDisconnection", 0, request_disconnection,
			"", "o", "object_path");

	l_dbus_interface_method(interface, "Release", 0, release, "", "");
}

/* can register all the interfaces here. */
static void ready_callback(void *user_data)
{
	bool success;
	/* export all the interfaces here */

	/* If this method is already registered it returns false */
	success = l_dbus_register_interface(dbus, DBUS_BLUEZ_PROFILE_INTERFACE,
			dbus_interface_setup, NULL, false);
	if (!success) {
		l_error("failed to register interface");
		goto error;
	}

	/* This method returns false if the dbus structure passed is NULL or any of its memory
	 * allocations fail.
	 */
	success = l_dbus_object_add_interface(dbus, DBUS_OBJ_PATH, DBUS_BLUEZ_PROFILE_INTERFACE, NULL);
	if (!success) {
		l_error("failed to add interface on %s", DBUS_OBJ_PATH);
		goto error;
	}

	/* The callback passed may get called while l_dbus_name_acquire is running
	 * or during main_loop.
	 */
	l_dbus_name_acquire(dbus, DBUS_NAME, false, false, false,
			name_acquired_callback, NULL);

error:
	;
}

static void disconnect_callback(void *user_data)
{
	l_main_quit();
}

bool dbus_init(void)
{

	dbus = l_dbus_new_default(L_DBUS_SYSTEM_BUS);

	/* The callback registered here gets called when
	 * a unique name is assigned to the application.
	 * Unique name starts with a ":". This is the first thing this application
	 * owns and the last thing it losses.
	 */
	l_dbus_set_ready_handler(dbus, ready_callback, dbus, NULL);

	/* The callback registered here gets called when application
	 * is disconnected from message bus.
	 */
	l_dbus_set_disconnect_handler(dbus, disconnect_callback, NULL, NULL);

	return false;
}

void dbus_cleanup(void)
{
	l_dbus_unregister_object(dbus, DBUS_OBJ_PATH);
	l_dbus_destroy(dbus);
}
