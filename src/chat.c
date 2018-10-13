#include <gtk/gtk.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


static void on_user_item_click(GtkListBox *box, GtkListBoxRow *row, gpointer user_data)
{
	g_print("Holona\n");
}

static GtkWidget** fetch_users()
{
	int usramnt = 10;
	GtkWidget **users = (GtkWidget **) malloc((usramnt + 1) * sizeof(GtkWidget*));
	for (int i = 0; i < 10; ++i) {
		char usr[20];
		sprintf(usr, "%s %d", "User", i);
		users[i] = gtk_label_new(usr);
	}
	users[usramnt] = NULL;

	return users;
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
	return status;
}
