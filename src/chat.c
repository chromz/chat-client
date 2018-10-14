#include <gtk/gtk.h>
#include <json.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct user_st {
	const char *id;
	const char *name;
	const char *status;
};

static struct user_st **user_st_list = NULL;
static char *dummy_users = "{"
	"	\"action\": \"LIST_USER\","
	"	\"users\": ["
	"		{"
	"			\"id\": \"ASDzxfaFA=asd?\","
	"			\"name\": \"JM\","
	"			\"status\": \"active\""
	"		},"
	"		{"
	"			\"id\": \"ASDzxfaFA=asd?\","
	"			\"name\": \"JR\","
	"			\"status\": \"active\""
	"		},"
	"		{"
	"			\"id\": \"ASDzxfaFA=asd?\","
	"			\"name\": \"NM\","
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

static void on_user_item_click(GtkListBox *box, GtkListBoxRow *row, gpointer user_data)
{
	g_print("Holona\n");
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
		g_print("USER: %s\n", user_st_list[i]->name);
		users_labels[i] = gtk_label_new(user_st_list[i]->name);
	}
	users_labels[usramnt] = NULL;
	user_st_list[usramnt] = NULL;

	return users_labels;
}

// Function to initialize chat gui
static void activate(GtkApplication *app, gpointer user_data)
{
	GtkWidget *window;
	GtkWidget *header_bar;
	GtkWidget *paned;
	GtkWidget *user_list;
	GtkWidget *chat_stack;
	GtkWidget *welcome_image;

	// Dummy widgets 
	GtkWidget *user_1;

	GError *error = NULL;
	GdkPixbuf *uvg_logo;

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
	gtk_stack_add_named(GTK_STACK(chat_stack), welcome_image, "welcome");

	// List related
	user_list = gtk_list_box_new();

	// Create dummy users
	GtkWidget **users = fetch_users();

	// Add dummy users
	for (int i = 0; users[i]; ++i) {
		gtk_container_add(GTK_CONTAINER(user_list), users[i]);
	}
	free(users);
	g_signal_connect(user_list, "row-activated", G_CALLBACK(on_user_item_click), NULL);

	// Setup chat interface
	paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_paned_pack1(GTK_PANED(paned), user_list, FALSE, FALSE);
	gtk_paned_pack2(GTK_PANED(paned), chat_stack, FALSE, FALSE);


	gtk_container_add(GTK_CONTAINER(window), paned);
	gtk_window_set_titlebar(GTK_WINDOW(window), header_bar);
	gtk_widget_show_all(window);
}


int main(int argc, char *argv[]) 
{
	GtkApplication *app;
	int status;
	app = gtk_application_new("gt.uvg.chat", G_APPLICATION_FLAGS_NONE);
	g_signal_connect(app, "activate", G_CALLBACK (activate), NULL);
	status = g_application_run(G_APPLICATION (app), argc, argv);
	g_object_unref(app);
	free_user_list();
	return status;
}
