/* HexChat
 * Copyright (C) 1998-2010 Peter Zelezny.
 * Copyright (C) 2009-2013 Berke Viktor.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#ifndef PCHAT_COMMONPLUGIN_H
#define PCHAT_COMMONPLUGIN_H

#ifdef PLUGIN_C
/* Define pchat_context only if not already defined */
#ifndef PCHAT_CONTEXT_DEFINED
#define PCHAT_CONTEXT_DEFINED
typedef struct session pchat_context;
#endif
struct _pchat_plugin
{
	/* Keep these in sync with pchat-plugin.h */
	/* !!don't change the order, to keep binary compat!! */
	pchat_hook *(*pchat_hook_command) (pchat_plugin *ph,
		    const char *name,
		    int pri,
		    int (*callback) (char *word[], char *word_eol[], void *user_data),
		    const char *help_text,
		    void *userdata);
	pchat_hook *(*pchat_hook_server) (pchat_plugin *ph,
		   const char *name,
		   int pri,
		   int (*callback) (char *word[], char *word_eol[], void *user_data),
		   void *userdata);
	pchat_hook *(*pchat_hook_print) (pchat_plugin *ph,
		  const char *name,
		  int pri,
		  int (*callback) (char *word[], void *user_data),
		  void *userdata);
	pchat_hook *(*pchat_hook_timer) (pchat_plugin *ph,
		  int timeout,
		  int (*callback) (void *user_data),
		  void *userdata);
	pchat_hook *(*pchat_hook_fd) (pchat_plugin *ph,
		   int fd,
		   int flags,
		   int (*callback) (int fd, int flags, void *user_data),
		   void *userdata);
	void *(*pchat_unhook) (pchat_plugin *ph,
	      pchat_hook *hook);
	void (*pchat_print) (pchat_plugin *ph,
	     const char *text);
	void (*pchat_printf) (pchat_plugin *ph,
	      const char *format, ...);
	void (*pchat_command) (pchat_plugin *ph,
	       const char *command);
	void (*pchat_commandf) (pchat_plugin *ph,
		const char *format, ...);
	int (*pchat_nickcmp) (pchat_plugin *ph,
	       const char *s1,
	       const char *s2);
	int (*pchat_set_context) (pchat_plugin *ph,
		   pchat_context *ctx);
	pchat_context *(*pchat_find_context) (pchat_plugin *ph,
		    const char *servname,
		    const char *channel);
	pchat_context *(*pchat_get_context) (pchat_plugin *ph);
	const char *(*pchat_get_info) (pchat_plugin *ph,
		const char *id);
	int (*pchat_get_prefs) (pchat_plugin *ph,
		 const char *name,
		 const char **string,
		 int *integer);
	pchat_list * (*pchat_list_get) (pchat_plugin *ph,
		const char *name);
	void (*pchat_list_free) (pchat_plugin *ph,
		 pchat_list *xlist);
	const char * const * (*pchat_list_fields) (pchat_plugin *ph,
		   const char *name);
	int (*pchat_list_next) (pchat_plugin *ph,
		 pchat_list *xlist);
	const char * (*pchat_list_str) (pchat_plugin *ph,
		pchat_list *xlist,
		const char *name);
	int (*pchat_list_int) (pchat_plugin *ph,
		pchat_list *xlist,
		const char *name);
	void * (*pchat_plugingui_add) (pchat_plugin *ph,
		     const char *filename,
		     const char *name,
		     const char *desc,
		     const char *version,
		     char *reserved);
	void (*pchat_plugingui_remove) (pchat_plugin *ph,
			void *handle);
	int (*pchat_emit_print) (pchat_plugin *ph,
			const char *event_name, ...);
	void *(*pchat_read_fd) (pchat_plugin *ph);
	time_t (*pchat_list_time) (pchat_plugin *ph,
		pchat_list *xlist,
		const char *name);
	char *(*pchat_gettext) (pchat_plugin *ph,
		const char *msgid);
	void (*pchat_send_modes) (pchat_plugin *ph,
		  const char **targets,
		  int ntargets,
		  int modes_per_line,
		  char sign,
		  char mode);
	char *(*pchat_strip) (pchat_plugin *ph,
	     const char *str,
	     int len,
	     int flags);
	void (*pchat_free) (pchat_plugin *ph,
	    void *ptr);
	int (*pchat_pluginpref_set_str) (pchat_plugin *ph,
		const char *var,
		const char *value);
	int (*pchat_pluginpref_get_str) (pchat_plugin *ph,
		const char *var,
		char *dest);
	int (*pchat_pluginpref_set_int) (pchat_plugin *ph,
		const char *var,
		int value);
	int (*pchat_pluginpref_get_int) (pchat_plugin *ph,
		const char *var);
	int (*pchat_pluginpref_delete) (pchat_plugin *ph,
		const char *var);
	int (*pchat_pluginpref_list) (pchat_plugin *ph,
		char *dest);
	pchat_hook *(*pchat_hook_server_attrs) (pchat_plugin *ph,
		   const char *name,
		   int pri,
		   int (*callback) (char *word[], char *word_eol[],
							pchat_event_attrs *attrs, void *user_data),
		   void *userdata);
	pchat_hook *(*pchat_hook_print_attrs) (pchat_plugin *ph,
		  const char *name,
		  int pri,
		  int (*callback) (char *word[], pchat_event_attrs *attrs,
						   void *user_data),
		  void *userdata);
	int (*pchat_emit_print_attrs) (pchat_plugin *ph, pchat_event_attrs *attrs,
									 const char *event_name, ...);
	pchat_event_attrs *(*pchat_event_attrs_create) (pchat_plugin *ph);
	void (*pchat_event_attrs_free) (pchat_plugin *ph,
									  pchat_event_attrs *attrs);

	/* PRIVATE FIELDS! */
	void *handle;		/* from dlopen */
	char *filename;	/* loaded from */
	char *name;
	char *desc;
	char *version;
	session *context;
	void *deinit_callback;	/* pointer to pchat_plugin_deinit */
	unsigned int fake:1;		/* fake plugin. Added by pchat_plugingui_add() */
	unsigned int free_strings:1;		/* free name,desc,version? */
};
#endif

GModule *module_load (char *filename);
char *plugin_load (session *sess, char *filename, char *arg);
int plugin_reload (session *sess, char *name, int by_filename);
void plugin_add (session *sess, char *filename, void *handle, void *init_func, void *deinit_func, char *arg, int fake);
int plugin_kill (char *name, int by_filename);
void plugin_kill_all (void);
void plugin_auto_load (session *sess);
int plugin_emit_command (session *sess, char *name, char *word[], char *word_eol[]);
int plugin_emit_server (session *sess, char *name, char *word[], char *word_eol[],
						time_t server_time);
int plugin_emit_print (session *sess, char *word[], time_t server_time);
int plugin_emit_dummy_print (session *sess, char *name);
int plugin_emit_keypress (session *sess, unsigned int state, unsigned int keyval, gunichar key);
GList* plugin_command_list(GList *tmp_list);
int plugin_show_help (session *sess, char *cmd);
void plugin_command_foreach (session *sess, void *userdata, void (*cb) (session *sess, void *userdata, char *name, char *usage));
session *plugin_find_context (const char *servname, const char *channel, server *current_server);

/* On macOS, G_MODULE_SUFFIX says "so" but meson uses "dylib"
 * https://github.com/mesonbuild/meson/issues/1160 */
#if defined(__APPLE__)
#  define PLUGIN_SUFFIX "dylib"
#else
#  define PLUGIN_SUFFIX G_MODULE_SUFFIX
#endif

#endif
