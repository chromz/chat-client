#include <gtk/gtk.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// Function to initialize chat gui
static void activate(GtkApplication *app, gpointer user_data)
{
	GtkWidget *window;
	GtkWidget *header_bar;
	GtkWidget *paned;
	GtkWidget *side_bar;
	GtkWidget *chat_stack;
	GtkWidget *welcome_image;
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
	uvg_logo = gdk_pixbuf_new_from_file("assets/uvg.jpg", &error);
	if (uvg_logo == NULL) {
		g_printerr("Error loading file #%d %s\n", error->code, error->message);
	}
	welcome_image = gtk_image_new_from_pixbuf(uvg_logo);
	chat_stack = gtk_stack_new();
	gtk_stack_add_titled(GTK_STACK(chat_stack), welcome_image, "welcome", "Welcome!");
	// Side bar
	side_bar = gtk_stack_sidebar_new();
	gtk_stack_sidebar_set_stack(GTK_STACK_SIDEBAR(side_bar), GTK_STACK(chat_stack));

	// Setup chat interface
	paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_paned_pack1(GTK_PANED(paned), side_bar, FALSE, FALSE);
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
