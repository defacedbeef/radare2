/* radare - LGPL - Copyright 2014-2018 - pancake */

#include <r_core.h>

R_API void r_core_task_print (RCore *core, RCoreTask *task, int mode) {
	switch (mode) {
	case 'j':
		r_cons_printf ("{\"id\":%d,\"status\":\"%c\",\"text\":\"%s\"}",
				task->id, task->state, task->msg->text);
		break;
	default:
		r_cons_printf ("%2d  %8s  %s\n", task->id, r_core_task_status (task), task->msg->text);
		if (mode == 1) {
			if (task->msg->res) {
				r_cons_println (task->msg->res);
			} else {
				r_cons_newline ();
			}
		}
		break;
	}
}

R_API void r_core_task_list (RCore *core, int mode) {
	RListIter *iter;
	RCoreTask *task;
	if (mode == 'j') {
		r_cons_printf ("[");
	}
	r_list_foreach (core->tasks, iter, task) {
		r_core_task_print (core, task, mode);
		if (mode == 'j' && iter->n) {
			r_cons_printf (",");
		}
	}
	if (mode == 'j') {
		r_cons_printf ("]\n");
	}
}

R_API void r_core_task_join (RCore *core, RCoreTask *task) {
	RListIter *iter;
	if( task) {
		r_cons_break_push (NULL, NULL);
		r_th_wait (task->msg->th);
		r_cons_break_pop ();
	} else {
		r_list_foreach_prev (core->tasks, iter, task) {
			r_th_wait (task->msg->th);
		}
	}
}

static int r_core_task_thread(RCore *core, RCoreTask *task) {
	// TODO
	return 0;
}

R_API RCoreTask *r_core_task_new (RCore *core, const char *cmd, RCoreTaskCallback cb, void *user) {
	RCoreTask *task = R_NEW0 (RCoreTask);
	if (task) {
		task->msg = r_th_msg_new (cmd, r_core_task_thread);
		task->id = r_list_length (core->tasks)+1;
		task->state = 's'; // stopped
		task->core = core;
		task->user = user;
		task->cb = cb;
	}
	return task;
}

R_API void r_core_task_run(RCore *core, RCoreTask *_task) {
	RCoreTask *task;
	RListIter *iter;
	char *str;
	r_list_foreach_prev (core->tasks, iter, task) {
		if (_task && task != _task) {
			continue;
		}
		if (task->state != 's') {
			continue;
		}
		task->state = 'r'; // running
		str = r_core_cmd_str (core, task->msg->text);
		eprintf ("Task %d finished width %d byte(s): %s\n%s\n",
				task->id, (int)strlen (str), task->msg->text, str);
		task->state = 'd'; // done
		task->msg->done = 1; // done DUP!!
		task->msg->res = str;
		if (task->cb) {
			task->cb (task->user, str);
		}
	}
}

R_API void r_core_task_run_bg(RCore *core, RCoreTask *_task) {
	RCoreTask *task;
	RListIter *iter;
	char *str;
	r_list_foreach_prev (core->tasks, iter, task) {
		if (_task && task != _task) {
			continue;
		}
		task->state = 'r'; // running
		str = r_core_cmd_str (core, task->msg->text);
		eprintf ("Task %d finished width %d byte(s): %s\n%s\n",
				task->id, (int)strlen (str), task->msg->text, str);
		task->state = 'd'; // done
		task->msg->done = 1; // done DUP!!
		task->msg->res = str;
	}
}

R_API RCoreTask *r_core_task_add (RCore *core, RCoreTask *task) {
	//r_th_pipe_push (core->pipe, task->cb, task);
	if (core->tasks) {
		r_list_append (core->tasks, task);
		return task;
	}
	return NULL;
}

R_API const char *r_core_task_status (RCoreTask *task) {
	static char str[2] = {0};
	switch (task->state) {
	case 's':
		return "running";
	case 'd':
		return "dead";
	}
	str[0] = task->state;
	return str;
}

R_API RCoreTask *r_core_task_self (RCore *core) {
	RListIter *iter;
	RCoreTask *task;
	R_TH_TID tid = r_th_self ();
	r_list_foreach (core->tasks, iter, task) {
		if (!task || !task->msg || !task->msg->th) {
			continue;
		}
		// TODO: use r_th_equal // pthread_equal
		if (task->msg->th->tid == tid) {
			return task;
		}
	}
	return NULL;
}

R_API bool r_core_task_pause (RCore *core, RCoreTask *task, bool enable) {
	if (!core || !task) {
		return false;
	}
	r_th_pause (task->msg->th, enable);
	return true;
}

R_API void r_core_task_add_bg (RCore *core, RCoreTask *task) {
	//r_th_pipe_push (core->pipe, task->cb, task);
	r_list_append (core->tasks, task);
}

R_API int r_core_task_cat (RCore *core, int id) {
	RCoreTask *task = r_core_task_get (core, id);
	r_cons_println (task->msg->res);
	r_core_task_del (core, id);
	return true;
}

R_API int r_core_task_del (RCore *core, int id) {
	RCoreTask *task;
	RListIter *iter;
	if (id == -1) {
		r_list_free (core->tasks);
		core->tasks = r_list_new ();
		return true;
	}
	r_list_foreach (core->tasks, iter, task) {
		if (task->id == id) {
			r_list_delete (core->tasks, iter);
			return true;
		}
	}
	return false;
}

R_API RCoreTask *r_core_task_get (RCore *core, int id) {
	RCoreTask *task;
	RListIter *iter;
	r_list_foreach (core->tasks, iter, task) {
		if (task->id == id) {
			return task;
		}
	}
	return NULL;
}
