#include <arpa/inet.h>
#include <gtk/gtk.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <json.h>
#include <limits.h>
#include <netdb.h> 
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

struct user_st {
	const char *id;
	const char *name;
	const char *status;
	GtkWidget *label;
};

struct usr_entry {
	struct user_st *usr;
	STAILQ_ENTRY(usr_entry) entries;
};

struct cnc_det {
	const char *username;
	const char *ip;
	int port;
};
static int sfd;
static GtkWidget *window;
static GtkWidget *header_bar;
static GtkWidget *status_combo;
static GtkWidget *paned;
static GtkWidget *user_list;
static GtkWidget *chat_stack;
static GtkWidget *chat_box;
static GtkWidget *chat_text;
static GtkWidget *form_box;
static GtkWidget *send_msg_button;
static GtkWidget *msg_input;
static GdkPixbuf *uvg_logo;

// Login components
static GtkWidget *spinner;
static GtkWidget *welcome_box;
static GtkWidget *welcome_image;
static GtkWidget *login_box;
static GtkWidget *name_label;
static GtkWidget *ip_label;
static GtkWidget *port_label;
static GtkWidget *name_in;
static GtkWidget *ip_in;
static GtkWidget *port_in;
static GtkWidget *cnct_btn;
static GtkWidget *error_label;

static gboolean logged_in = FALSE;

static pthread_mutex_t glock;
static pthread_mutex_t socket_lock;
static struct user_st *current_user;

static STAILQ_HEAD(slisthead, usr_entry) user_st_list = STAILQ_HEAD_INITIALIZER(user_st_list);


static gboolean show_error_gui(void *msg_v)
{
	char *msg = (char *) msg_v;
	GtkWidget *dialog;
	GtkDialogFlags flags = GTK_DIALOG_MODAL; 
	dialog = gtk_message_dialog_new(GTK_WINDOW(window), flags,
			GTK_MESSAGE_ERROR,
			GTK_BUTTONS_CLOSE,
			"%s",
			msg);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	return FALSE;
}

static void show_error(char *msg)
{
	gdk_threads_add_idle(show_error_gui, msg);
}

static struct user_st *find_user_by_index(int index)
{
	struct usr_entry *np;
	int i = 0;
	STAILQ_FOREACH(np, &user_st_list, entries) {
		if (i == index) {
			return np->usr;
		}
		i++;
	}
	return NULL;
}

static void free_user_list(void)
{
	struct usr_entry *np;
	while (!STAILQ_EMPTY(&user_st_list)) {
		np = STAILQ_FIRST(&user_st_list);
		STAILQ_REMOVE_HEAD(&user_st_list, entries);
		free(np);
	}
}

static gboolean display_error(void *data)
{
	char *msg = (char *) data;
	gtk_widget_set_sensitive(cnct_btn, TRUE);
	gtk_label_set_text(GTK_LABEL(error_label), msg);
	gtk_widget_show(error_label);
	gtk_spinner_stop(GTK_SPINNER(spinner));
	return FALSE;
}

static void handle_error(char *msg)
{
	gdk_threads_add_idle(display_error, msg);
}

static gboolean login_user(void *data)
{
	gtk_stack_set_visible_child_name(GTK_STACK(chat_stack), "chat-box");
	gtk_widget_show(status_combo);
	return FALSE;
}

static void test_set_prop(json_bool *err, json_object *obj, char *key, json_object **dest)
{
	if (*err) {
		return;
	}
	*err = !json_object_object_get_ex(obj, key, dest);
}


static gboolean show_users(void *data)
{
	struct usr_entry *np;
	pthread_mutex_lock(&glock);
	STAILQ_FOREACH(np, &user_st_list, entries) {
		if (strcmp(np->usr->id, current_user->id) != 0) {
			np->usr->label = gtk_label_new(np->usr->name);
			gtk_widget_show(np->usr->label);
			gtk_container_add(GTK_CONTAINER(user_list), np->usr->label);
		}
	}
	pthread_mutex_unlock(&glock);
	return FALSE;
}

static void fetch_users(const char *userid)
{
	int usramnt = 0;
	char msg_buffer[BUFFER_SIZE];
	struct json_object *req_j, *action_j;
	struct json_object *server_resp, *user_list_json, *user_obj;
	struct json_object *user_id_j, *user_name_j, *user_status_j;

	req_j = json_object_new_object();
	action_j = json_object_new_string("LIST_USER");
	json_object_object_add(req_j, "action", action_j);
	const char *req = json_object_to_json_string(req_j);
	pthread_mutex_lock(&socket_lock);
	int bytes_wrt = write(sfd, req, strlen(req));
	pthread_mutex_unlock(&socket_lock);
	if (bytes_wrt == -1) {
		handle_error("Unable to write to socket");
		return;
	}
	int bytes_read = read(sfd, msg_buffer, BUFFER_SIZE);
	if (bytes_read == -1) {
		handle_error("Error reading users");
		return;
	}

	server_resp = json_tokener_parse(msg_buffer);
	if (!json_object_object_get_ex(server_resp, "users", &user_list_json)) {
		return;
	}

	usramnt = json_object_array_length(user_list_json);
	pthread_mutex_lock(&glock);
	for (int i = 0; i < usramnt; ++i) {
		user_obj = json_object_array_get_idx(user_list_json, i);
		json_object_object_get_ex(user_obj, "id", &user_id_j);
		json_object_object_get_ex(user_obj, "name", &user_name_j);
		json_object_object_get_ex(user_obj, "status", &user_status_j);
		struct usr_entry *new_usr = malloc(sizeof(struct usr_entry)); 
		new_usr->usr = malloc(sizeof(struct user_st));
		new_usr->usr->id = json_object_get_string(user_id_j);
		new_usr->usr->name = json_object_get_string(user_name_j);
		new_usr->usr->status = json_object_get_string(user_status_j);
		if (strcmp(userid, new_usr->usr->id) == 0) {
			current_user = new_usr->usr;
		}
		STAILQ_INSERT_TAIL(&user_st_list, new_usr, entries);
	}
	pthread_mutex_unlock(&glock);
	gdk_threads_add_idle(show_users, NULL);

}

static gboolean add_user_to_list(void *usr_v)
{
	struct user_st *usr = (struct user_st *) usr_v;
	usr->label = gtk_label_new(usr->name);
	gtk_widget_show(usr->label);
	gtk_container_add(GTK_CONTAINER(user_list), usr->label);
	return FALSE;
}

static void handle_user_connected(struct json_object *req)
{
	struct json_object *user, *id_j, *name_j, *status_j;
	json_bool error = 0;
	test_set_prop(&error, req, "user", &user);
	if (error) {
		printf("Unable to parse user\n");
		return;
	}
	test_set_prop(&error, user, "id", &id_j);
	test_set_prop(&error, user, "name", &name_j);
	test_set_prop(&error, user, "status", &status_j);
	if (error) {
		printf("Unable to identify user\n");
		return;
	}
	struct usr_entry *new_usr = malloc(sizeof(struct usr_entry)); 
	new_usr->usr = malloc(sizeof(struct user_st));
	new_usr->usr->id = json_object_get_string(id_j);
	new_usr->usr->name = json_object_get_string(name_j);
	new_usr->usr->status = json_object_get_string(status_j);
	pthread_mutex_lock(&glock);
	STAILQ_INSERT_TAIL(&user_st_list, new_usr, entries);
	pthread_mutex_unlock(&glock);
	gdk_threads_add_idle(add_user_to_list, new_usr->usr);
}


static void change_user_status(const char *id, const char *new_status)
{
	struct usr_entry *np;
	pthread_mutex_lock(&glock);
	STAILQ_FOREACH(np, &user_st_list, entries) {
		if (strcmp(np->usr->id, id) == 0) {
			// Change user status on
			np->usr->status = new_status;
			break;
		}
	}
	pthread_mutex_unlock(&glock);
}

static void handle_user_changed_status(struct json_object *req)
{
	struct json_object *user, *id_j, *status_j;
	json_bool error = 0;
	test_set_prop(&error, req, "user", &user);
	if (error) {
		printf("Unable to parse user\n");
		return;
	}
	test_set_prop(&error, user, "id", &id_j);
	test_set_prop(&error, user, "status", &status_j);
	if (error) {
		printf("Unable to identify user\n");
		return;
	}
	const char *id = json_object_get_string(id_j);
	const char *new_status = json_object_get_string(status_j);
	change_user_status(id, new_status);
}

static void handle_user_disconnected(struct json_object *req) 
{
    struct usr_entry *np;
    struct json_object *user, *id_j;
    json_bool error = 0;
    test_set_prop(&error, req, "user", &user);
    if (error) {
        printf("Unable to parse user\n");
        return;
    }
    test_set_prop(&error, user, "id", &id_j);
    if (error) {
        printf("Unable to identify user\n");
        return;
    }
    const char* user_id = json_object_get_string(id_j);
    pthread_mutex_lock(&glock);
    STAILQ_FOREACH(np, &user_st_list, entries) {
        if (strcmp(np->usr->id, user_id) == 0) {
            STAILQ_REMOVE(&user_st_list, np, usr_entry, entries);
            gtk_widget_destroy(np->usr->label);
            free(np);
        }
    }
    pthread_mutex_unlock(&glock);
}

static void handle_action(struct json_object *action_j, struct json_object *req)
{
	const char *action = json_object_get_string(action_j);
	if (strcmp(action, "USER_CONNECTED") == 0) {
		handle_user_connected(req);
	}

	if (strcmp(action, "CHANGED_STATUS") == 0) {
		handle_user_changed_status(req);
	}

    if (strcmp(action, "USER_DISCONNECTED") == 0) {
        handle_user_disconnected(req);
    }
}

static void *socket_connect(void *data)
{
	char msg_buffer[BUFFER_SIZE];
	struct json_object *ok_j, *status_j, *usr_data, *id_j, *name_j, *usr_status_j;
	struct sockaddr_in server_addr;
	struct cnc_det *conn = (struct cnc_det *) data;
	sfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sfd < 0) {
		// Send error message here :p
		handle_error("Error creating socket");
		return NULL;
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(conn->ip);
	server_addr.sin_port = htons(conn->port);
	int sock_stat = connect(sfd, (struct sockaddr *) &server_addr, sizeof(server_addr));

	if (sock_stat < 0) {
		handle_error("Error connecting to server");
		return NULL;
	}
	// Send handshake
	json_object *handshake_j = json_object_new_object();
	json_object *host_j = json_object_new_string(conn->ip);
	json_object *origin_j = json_object_new_string("chat-client");
	json_object *user_j = json_object_new_string(conn->username);
	json_object_object_add(handshake_j, "host", host_j);
	json_object_object_add(handshake_j, "origin", origin_j);
	json_object_object_add(handshake_j, "user", user_j);
	const char *handshake = json_object_to_json_string(handshake_j);
	pthread_mutex_lock(&socket_lock);
	int bytes_wrt = write(sfd, handshake, strlen(handshake));
	pthread_mutex_unlock(&socket_lock);
	if (bytes_wrt == -1) {
		handle_error("Unable to write to socket");
		return NULL;
	}
	int bytes_read = read(sfd, msg_buffer, BUFFER_SIZE);
	if (bytes_read == -1) {
		handle_error("Error reading handshake");
		return NULL;
	}

	printf("Message recieved: %s\n", msg_buffer);
	ok_j = json_tokener_parse(msg_buffer);
	json_object_object_get_ex(ok_j, "status", &status_j);
	const char *status = json_object_get_string(status_j);
	if (strcmp(status, "OK") == 0) {
		gdk_threads_add_idle(login_user, NULL);
	}
	json_bool error;
	// Set my user identity
	test_set_prop(&error, ok_j, "user", &usr_data);

	if (error) {
		handle_error("Invalid server response");
		return NULL;
	}

	test_set_prop(&error, usr_data, "id", &id_j);
	test_set_prop(&error, usr_data, "name", &name_j);
	test_set_prop(&error, usr_data, "status", &usr_status_j);
	if (error) {
		handle_error("Invalid user object");
		return NULL;      
	}

	const char *userid = json_object_get_string(id_j);
	// Fetch users
	fetch_users(userid);
	int rdbytes;
	struct json_object *req, *action_prop;
	while (1) {
		rdbytes= read(sfd, msg_buffer, BUFFER_SIZE);
		if (rdbytes == 0) {
			show_error("Server disconnected\n");
			close(sfd);
			return NULL;
		}
		req = json_tokener_parse(msg_buffer);
		test_set_prop(&error, req, "action", &action_prop);
		if (error) {
			show_error("Error checking for action\n");
		} else {
			handle_action(action_prop, req);
		}
	}
	free(conn);
	return NULL;
}


static void *request_user_status_change(void *nstat)
{
	char *new_stat = (char *) nstat;
	char msg_buffer[BUFFER_SIZE];
	struct json_object *req_j, *action_j, *ok_j, *status_j;
	struct json_object *server_resp, *user_list_json, *user_obj;
	struct json_object *user_id_j, *user_name_j, *user_status_j;

	int error, rdbytes = 0;
	req_j = json_object_new_object();
	action_j = json_object_new_string("CHANGE_STATUS");
	user_id_j = json_object_new_string(current_user->id);
	user_status_j = json_object_new_string(new_stat);
	json_object_object_add(req_j, "action", action_j);
	json_object_object_add(req_j, "user", user_id_j);
	json_object_object_add(req_j, "status", user_status_j);
	const char *req = json_object_to_json_string(req_j);
	pthread_mutex_lock(&socket_lock);
	error = write(sfd, req, strlen(req));
	if (error == -1) {
		show_error("Error writing to socket\n");
		return NULL;
	}
	// Wait for the ok
	rdbytes = read(sfd, msg_buffer, BUFFER_SIZE);
	if (rdbytes == -1) {
		show_error("Error reading from server\n");
		return NULL;
	}
	ok_j = json_tokener_parse(msg_buffer);
	json_object_object_get_ex(ok_j, "status", &status_j);
	const char *status = json_object_get_string(status_j);
	if (strcmp(status, "OK") == 0) {

	} else {
		show_error("Server error");
	}
	pthread_mutex_unlock(&socket_lock);
	return NULL;
}

static void on_user_change_status(GtkComboBox *widget, gpointer user_data)
{
	gtk_widget_set_sensitive(GTK_WIDGET(widget), FALSE);
	gchar *new_status = gtk_combo_box_text_get_active_text(
			GTK_COMBO_BOX_TEXT(widget));
	pthread_t thread;
	pthread_create(&thread, NULL, request_user_status_change, new_status);
	if (pthread_detach(thread) != 0) {
		g_print("Unable to detach pthread\n");
	}
	printf("New Status %s\n", new_status);

}

static void connect_to_server(GtkButton *button, gpointer user_data)
{
	gtk_widget_hide(error_label);
	gtk_widget_set_sensitive(cnct_btn, FALSE);
	gtk_spinner_start(GTK_SPINNER(spinner));
	pthread_t thread;
	struct cnc_det *connection = malloc(sizeof(struct cnc_det));
	connection->username = gtk_entry_get_text(GTK_ENTRY(name_in));
	connection->ip = gtk_entry_get_text(GTK_ENTRY(ip_in));
	connection->port = (int) strtol(gtk_entry_get_text(GTK_ENTRY(port_in)), NULL, 0);
	if (pthread_create(&thread, NULL, socket_connect, connection) != 0) {
		gtk_spinner_stop(GTK_SPINNER(spinner));
		gtk_widget_show(error_label);
	}
	if (pthread_detach(thread) != 0) {
		show_error("Unable to detach pthread\n");
	}
}

static void on_user_item_click(GtkListBox *box, GtkListBoxRow *row, gpointer user_data)
{
	if (!logged_in) {
		// Show chat interface
		gtk_stack_set_visible_child_name(GTK_STACK(chat_stack), "chat-box");
		logged_in = TRUE;
	}
	gint index = gtk_list_box_row_get_index(row);
	pthread_mutex_lock(&glock);
	struct user_st *usr = find_user_by_index(index);
	pthread_mutex_unlock(&glock);
	gtk_header_bar_set_title(GTK_HEADER_BAR(header_bar), usr->name);
	gtk_header_bar_set_subtitle(GTK_HEADER_BAR(header_bar), usr->status);
}

static void button_clicked(GtkWidget *widget, gpointer user_data)
{	
	g_print("clicked");
}



// Function to initialize chat gui
static void activate(GtkApplication *app, gpointer user_data)
{


	GError *error = NULL;

	window = gtk_application_window_new(app);
	gtk_window_set_title(GTK_WINDOW(window), "Chat Client");
	gtk_window_set_default_size(GTK_WINDOW(window), 800, 800);

	// Setup the header bar
	header_bar = gtk_header_bar_new();
	status_combo = gtk_combo_box_text_new();
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(status_combo), NULL, "active");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(status_combo), NULL, "busy");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(status_combo), NULL, "inactive");
	gtk_header_bar_set_title(GTK_HEADER_BAR(header_bar), "Chat Client");
	gtk_header_bar_set_subtitle(GTK_HEADER_BAR(header_bar), "v1.0");
	gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header_bar), TRUE);
	gtk_container_add(GTK_CONTAINER(header_bar), status_combo);
	gtk_combo_box_set_active(GTK_COMBO_BOX(status_combo), 0);
	// initial interface
	uvg_logo = gdk_pixbuf_new_from_file("assets/chat.png", &error);
	if (uvg_logo == NULL) {
		g_printerr("Error loading file #%d %s\n", error->code, error->message);
	}
	welcome_image = gtk_image_new_from_pixbuf(uvg_logo);
	chat_stack = gtk_stack_new();
	// Login View
	welcome_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	login_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	name_label = gtk_label_new("Username:");
	ip_label = gtk_label_new("Server Ip:");
	port_label = gtk_label_new("Port:");
	name_in = gtk_entry_new();
	ip_in = gtk_entry_new();
	port_in = gtk_entry_new();
	spinner = gtk_spinner_new();
	cnct_btn = gtk_button_new_with_label("Connect");
	error_label = gtk_label_new("Error connecting to server");

	gtk_box_pack_start(GTK_BOX(welcome_box), welcome_image, TRUE, TRUE, 20);

	gtk_box_pack_start(GTK_BOX(login_box), name_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(login_box), name_in, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(login_box), ip_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(login_box), ip_in, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(login_box), port_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(login_box), port_in, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(login_box), cnct_btn, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(login_box), spinner, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(login_box), error_label, FALSE, FALSE, 0);
	
	gtk_box_pack_end(GTK_BOX(welcome_box), login_box, FALSE, FALSE, 0);
	gtk_widget_set_valign(welcome_box, GTK_ALIGN_CENTER);
	gtk_widget_set_halign(welcome_box, GTK_ALIGN_CENTER);
	gtk_stack_add_named(GTK_STACK(chat_stack), welcome_box, "welcome");

	// Chat View
	chat_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10); 
	form_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	chat_text = gtk_text_view_new();
	msg_input = gtk_entry_new();
	send_msg_button = gtk_button_new_with_label("Send");

	// Add chat textbox
	gtk_text_view_set_editable(GTK_TEXT_VIEW(chat_text), FALSE);
	gtk_box_pack_start(GTK_BOX(chat_box), chat_text, TRUE, TRUE, 0);

	// Prepare send message form
	gtk_box_pack_start(GTK_BOX(form_box), msg_input, TRUE, TRUE, 0);
	gtk_box_pack_end(GTK_BOX(form_box), send_msg_button, FALSE, FALSE, 0); 
	gtk_box_pack_end(GTK_BOX(chat_box), form_box, FALSE, FALSE, 0);
	gtk_stack_add_named(GTK_STACK(chat_stack), chat_box, "chat-box");
	// List related
	user_list = gtk_list_box_new();


	g_signal_connect(cnct_btn, "clicked", G_CALLBACK(connect_to_server), NULL);
	g_signal_connect(user_list, "row-activated", G_CALLBACK(on_user_item_click), NULL);
	g_signal_connect(status_combo, "changed", G_CALLBACK(on_user_change_status), NULL);

	// send button clicked
	g_signal_connect(send_msg_button, "clicked", G_CALLBACK(button_clicked), NULL);

	// Setup chat interface
	paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_paned_pack1(GTK_PANED(paned), user_list, FALSE, FALSE);
	gtk_paned_pack2(GTK_PANED(paned), chat_stack, FALSE, FALSE);


	gtk_container_add(GTK_CONTAINER(window), paned);
	gtk_window_set_titlebar(GTK_WINDOW(window), header_bar);
	gtk_widget_show_all(window);


	// Default values
	gtk_entry_set_text(GTK_ENTRY(name_in), "chromz");
	gtk_entry_set_text(GTK_ENTRY(ip_in), "127.0.0.1");
	gtk_entry_set_text(GTK_ENTRY(port_in), "8000");

	// Hide error message
	gtk_widget_hide(error_label);
	gtk_widget_hide(status_combo);
}


int main(int argc, char *argv[]) 
{
	GtkApplication *app;
	int status;
	if (pthread_mutex_init(&glock, NULL) != 0) { 
		handle_error("Failed to initialize mutex\n"); 
	}

	if (pthread_mutex_init(&socket_lock, NULL) != 0) { 
		handle_error("Failed to initialize mutex\n"); 
	}
	app = gtk_application_new("gt.uvg.chat", G_APPLICATION_FLAGS_NONE);
	g_signal_connect(app, "activate", G_CALLBACK (activate), NULL);
	status = g_application_run(G_APPLICATION (app), argc, argv);
	g_object_unref(app);
	pthread_mutex_destroy(&glock);
	/* free_user_list(); */
	return status;
}
