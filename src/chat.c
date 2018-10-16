#include <arpa/inet.h>
#include <gtk/gtk.h>
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
};

struct cnc_det {
	const char *username;
	const char *ip;
	int port;
};
static GtkWidget *window;
static GtkWidget *header_bar;
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
static gboolean fetching = FALSE;
static struct user_st current_user;

static struct user_st **user_st_list = NULL;
static char *dummy_users = "{"
	"	\"action\": \"LIST_USER\","
	"	\"users\": ["
	"		{"
	"			\"id\": \"ASDzxfaFA=asd?\","
	"			\"name\": \"JR\","
	"			\"status\": \"active\""
	"		},"
	"		{"
	"			\"id\": \"ASDzxfaFA=asd?\","
	"			\"name\": \"NM\","
	"			\"status\": \"active\""
	"		},"
	"		{"
	"			\"id\": \"ASDzxfaFA=asd?\","
	"			\"name\": \"LV\","
	"			\"status\": \"active\""
	"		}"
	"	]"
	"}";

static void free_user_list(void)
{
	for (int i = 0; user_st_list[i]; ++i) {
		free(user_st_list[i]);
	}
	free(user_st_list);
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
	return FALSE;
}

static void test_set_prop(json_bool *err, json_object *obj, char *key, json_object **dest)
{
	if (*err) {
		return;
	}
	*err = !json_object_object_get_ex(obj, key, dest);
}

static void *socket_connect(void *data)
{
	int sfd;
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
	int bytes_wrt = write(sfd, handshake, strlen(handshake));
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

	current_user.id = json_object_get_string(id_j);
	current_user.name = json_object_get_string(name_j);
	current_user.status = json_object_get_string(usr_status_j);

	// Fetch users
	
	free(conn);
	return NULL;
}

static void connect_to_server(GtkButton *button, gpointer user_data)
{
	if (fetching) {
		return;
	}
	gtk_widget_hide(error_label);
	gtk_widget_set_sensitive(cnct_btn, FALSE);
	fetching = TRUE;
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
}

static void on_user_item_click(GtkListBox *box, GtkListBoxRow *row, gpointer user_data)
{
	if (!logged_in) {
		// Show chat interface
		gtk_stack_set_visible_child_name(GTK_STACK(chat_stack), "chat-box");
		logged_in = TRUE;
	}
	gint index = gtk_list_box_row_get_index(row);
	struct user_st *usr = user_st_list[index];
	gtk_header_bar_set_title(GTK_HEADER_BAR(header_bar), usr->name);
	gtk_header_bar_set_subtitle(GTK_HEADER_BAR(header_bar), usr->status);
}

static void button_clicked(GtkWidget *widget, gpointer user_data)
{	
	g_print("clicked");
}

static GtkWidget** fetch_users(void)
{
	int usramnt = 0;
	GtkWidget **users_labels;
	struct json_object *server_resp, *user_list_json, *user_obj;
	struct json_object *user_id_j, *user_name_j, *user_status_j;
	server_resp = json_tokener_parse(dummy_users);
	if (!json_object_object_get_ex(server_resp, "users", &user_list_json)) {
		return NULL;
	}

	usramnt = json_object_array_length(user_list_json);
	users_labels = malloc((usramnt + 1) * sizeof(GtkWidget*));
	user_st_list = malloc((usramnt + 1) * sizeof(struct user_st *));
	for (int i = 0; i < usramnt; ++i) {
		user_obj = json_object_array_get_idx(user_list_json, i);
		json_object_object_get_ex(user_obj, "id", &user_id_j);
		json_object_object_get_ex(user_obj, "name", &user_name_j);
		json_object_object_get_ex(user_obj, "status", &user_status_j);
		user_st_list[i] = malloc(sizeof(struct user_st));
		user_st_list[i]->id = json_object_get_string(user_id_j);
		user_st_list[i]->name = json_object_get_string(user_name_j);
		user_st_list[i]->status = json_object_get_string(user_status_j);
		users_labels[i] = gtk_label_new(user_st_list[i]->name);
	}
	users_labels[usramnt] = NULL;
	user_st_list[usramnt] = NULL;

	return users_labels;
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
	gtk_header_bar_set_title(GTK_HEADER_BAR(header_bar), "Chat Client");
	gtk_header_bar_set_subtitle(GTK_HEADER_BAR(header_bar), "v1.0");
	gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header_bar), TRUE);

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

	// Create dummy users
	/* GtkWidget **users = fetch_users(); */

	// Add dummy users
	/* for (int i = 0; users[i]; ++i) { */
	/* 	gtk_container_add(GTK_CONTAINER(user_list), users[i]); */
	/* } */
	/* free(users); */

	g_signal_connect(cnct_btn, "clicked", G_CALLBACK(connect_to_server), NULL);
	g_signal_connect(user_list, "row-activated", G_CALLBACK(on_user_item_click), NULL);

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
}


int main(int argc, char *argv[]) 
{
	GtkApplication *app;
	int status;
	app = gtk_application_new("gt.uvg.chat", G_APPLICATION_FLAGS_NONE);
	g_signal_connect(app, "activate", G_CALLBACK (activate), NULL);
	status = g_application_run(G_APPLICATION (app), argc, argv);
	g_object_unref(app);
	/* free_user_list(); */
	return status;
}
