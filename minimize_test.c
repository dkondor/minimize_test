/*
 * xdg_activation_test.c
 * 
 * Simple GTK+ app to test passing xdg-activation tokens among two
 * running instances.
 * 
 */


#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkwayland.h>
#include <unistd.h>

#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"

#define CHILD_APP_ID "mt-child"

static GdkDisplay *dsp;
static struct wl_display *wl_dsp = NULL;


typedef struct zwlr_foreign_toplevel_handle_v1 wfthandle;
static struct zwlr_foreign_toplevel_manager_v1 *manager = NULL;
static int init_done = 0;
static wfthandle *child_handle = NULL;
static bool child_minimized = false;

static GtkWidget *win = NULL;
static GtkWidget *btn1 = NULL;
static GtkWidget *btnw = NULL;
static void set_minimize_pos(GtkWidget *btn);


static void title_cb(void*, wfthandle*, const char*) {
	/* don't care */
}

static void appid_cb(void*, wfthandle* handle, const char* app_id) {
	if(!(app_id && handle)) return;
	if(!strcmp(app_id, CHILD_APP_ID)) {
		child_handle = handle;
		gtk_widget_set_sensitive(btn1, TRUE);
	}
}

void output_dummy_cb(void*, wfthandle*, struct wl_output*) {
	/* don't care */
}

void state_cb(void*, wfthandle* handle, struct wl_array* state) {
	if(!(handle && state)) return;
	if(handle != child_handle) return;
	
	bool min = false;
	int i;
	uint32_t* stdata = (uint32_t*)state->data;
	for(i = 0; i*sizeof(uint32_t) < state->size; i++) {
		if(stdata[i] == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MINIMIZED) {
            min = true;
            break;
		}
	}
	
	child_minimized = min;
}

void done_cb(void*, wfthandle*) {
	/* don't care */
}

void closed_cb(void*, wfthandle* handle) {
	if(!handle) return;
	if(handle == child_handle) {
		child_handle = NULL;
		gtk_widget_set_sensitive(btn1, FALSE);
	}
	zwlr_foreign_toplevel_handle_v1_destroy(handle);
}

void parent_cb(void*, wfthandle*, wfthandle*) {
	/* don't care */
}

struct zwlr_foreign_toplevel_handle_v1_listener toplevel_handle_interface = {
    .title        = title_cb,
    .app_id       = appid_cb,
    .output_enter = output_dummy_cb,
    .output_leave = output_dummy_cb,
    .state        = state_cb,
    .done         = done_cb,
    .closed       = closed_cb,
    .parent       = parent_cb
};


/* register new toplevel */
static void new_toplevel(void*, struct zwlr_foreign_toplevel_manager_v1*, wfthandle *handle) {
	if(!handle) return;
	zwlr_foreign_toplevel_handle_v1_add_listener(handle, &toplevel_handle_interface, NULL);
}

/* sent when toplevel management is no longer available -- this will happen after stopping */
static void toplevel_manager_finished(void*, struct zwlr_foreign_toplevel_manager_v1 *manager) {
    zwlr_foreign_toplevel_manager_v1_destroy(manager);
}

static struct zwlr_foreign_toplevel_manager_v1_listener toplevel_manager_interface = {
    .toplevel = new_toplevel,
    .finished = toplevel_manager_finished,
};


static void registry_global_add_cb(void*, struct wl_registry *registry,
		uint32_t id, const char *interface, uint32_t version) {
	if(!strcmp(interface, zwlr_foreign_toplevel_manager_v1_interface.name)) {
		uint32_t v = zwlr_foreign_toplevel_manager_v1_interface.version;
		if(version < v) v = version;
		manager = wl_registry_bind(registry, id, &zwlr_foreign_toplevel_manager_v1_interface, v);
		if(manager) zwlr_foreign_toplevel_manager_v1_add_listener(manager, &toplevel_manager_interface, NULL);
		else { /* TODO: handle error */ }
	}
	init_done = 0;
}

static void registry_global_remove_cb(G_GNUC_UNUSED void *data,
		G_GNUC_UNUSED struct wl_registry *registry, G_GNUC_UNUSED uint32_t id) {
	/* don't care */
}

static const struct wl_registry_listener registry_listener = {
	registry_global_add_cb,
	registry_global_remove_cb
};


static void set_minimize_pos(GtkWidget *btn)
{
	if(!(win && btn && child_handle)) return;
	GdkWindow *window = gtk_widget_get_window(win);
	if(!window) return;
	struct wl_surface* surface = gdk_wayland_window_get_wl_surface(window);
	
	GtkAllocation area;
	gtk_widget_get_allocation(btn, &area);
	int w = area.width;
	if(btnw) w = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(btnw));
	zwlr_foreign_toplevel_handle_v1_set_rectangle(child_handle, surface, area.x, area.y, w, 1);// area.width, area.height);
}

static void toggle_min(void)
{
	if(child_minimized) {
		GdkSeat *seat = gdk_display_get_default_seat (dsp);
		struct wl_seat* wl_seat = gdk_wayland_seat_get_wl_seat (seat);
		zwlr_foreign_toplevel_handle_v1_activate (child_handle, wl_seat);
	}
	else zwlr_foreign_toplevel_handle_v1_set_minimized (child_handle);
}

static void toggle_simple(GtkButton*, void*)
{
	if(!child_handle) return;
	set_minimize_pos(btn1);
	toggle_min();
}



int main(int argc, char **argv)
{
	pid_t pid = fork();
	if (pid < 0) return -1;
	
	if (pid > 0)
	{
		gtk_init(&argc, &argv);
		
		dsp = gdk_display_get_default();
		if(!GDK_IS_WAYLAND_DISPLAY(dsp)) {
			fprintf(stderr, "Not running in a Wayland session!\n");
			return 1;
		}
		wl_dsp = gdk_wayland_display_get_wl_display (dsp);
		
		win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_window_set_title(GTK_WINDOW(win), "Parent");
		g_signal_connect(win, "destroy", gtk_main_quit, NULL);
		
		btn1 = gtk_button_new_with_label("Toggle child minimized");

		g_signal_connect(btn1, "clicked", G_CALLBACK(toggle_simple), NULL);
		gtk_widget_set_sensitive(btn1, FALSE);
		
		btnw = gtk_spin_button_new_with_range (1.0, 200, 1.0);
		
		GtkBox *box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 10));
		gtk_box_pack_start(box, btn1, TRUE, FALSE, 10);
		gtk_box_pack_start(box, gtk_label_new("Minimize area width:"), TRUE, FALSE, 5);
		gtk_box_pack_start(box, btnw, TRUE, FALSE, 5);
		gtk_container_add(GTK_CONTAINER(win), GTK_WIDGET(box));
		
		// connect to foreign toplevel protocol
		struct wl_registry* registry = wl_display_get_registry(wl_dsp);
		wl_registry_add_listener(registry, &registry_listener, NULL);
		do {
			init_done = 1;
			wl_display_roundtrip(wl_dsp);
		}
		while(!init_done);
		
		if(!manager) {
			fprintf(stderr, "wlr-toplevel-management interface not found!\n");
			return 1;
		}
		
		gtk_widget_show_all(win);
		gtk_main();
	}
	else
	{
		// child process
		gtk_init(&argc, &argv);
		g_set_prgname(CHILD_APP_ID); // for use as app-id
		
		GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_window_set_title(GTK_WINDOW(win), "Child");
		gtk_window_set_default_size(GTK_WINDOW(win), 500, 400);
		g_signal_connect(win, "destroy", gtk_main_quit, NULL);
		
		GtkWidget *lbl = gtk_label_new("Child window");
		gtk_container_add(GTK_CONTAINER(win), lbl);
		
		gtk_widget_show_all(win);
		
		gtk_main();
	}
	
	return 0;
}

