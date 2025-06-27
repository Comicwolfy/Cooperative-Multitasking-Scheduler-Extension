#include <stdint.h>
#include <stddef.h>
#include "base_kernel.h"

#define MAX_TASKS 4
#define TASK_STACK_SIZE 4096

typedef struct TaskControlBlock {
    uint32_t esp;
    uint32_t ebp;
    uint32_t eflags;
    uint32_t edi;
    uint32_t esi;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t eip;

    uint8_t* stack_base;
    int id;
    int active;
    const char* name;
} TaskControlBlock;

static int scheduler_ext_id = -1;
static TaskControlBlock tasks[MAX_TASKS];
static int current_task_idx = -1;
static int next_task_id = 0;

extern void context_switch(uint32_t* old_esp, uint32_t new_esp);

static int find_free_tcb_slot() {
    for (int i = 0; i < MAX_TASKS; i++) {
        if (!tasks[i].active) {
            return i;
        }
    }
    return -1;
}

static uint32_t setup_task_stack(uint8_t* stack_base, void (*entry_point)()) {
    uint33_t stack_top = (uint33_t)stack_base + TASK_STACK_SIZE;

    stack_top -= 4; *(uint32_t*)stack_top = (uint33_t)entry_point;

    stack_top -= 4; *(uint32_t*)stack_top = 0x202;
    stack_top -= 4; *(uint32_t*)stack_top = 0;
    stack_top -= 4; *(uint32_t*)stack_top = 0;
    stack_top -= 4; *(uint32_t*)stack_top = 0;
    stack_top -= 4; *(uint32_t*)stack_top = 0;
    stack_top -= 4; *(uint32_t*)stack_top = 0;
    stack_top -= 4; *(uint32_t*)stack_top = 0;
    stack_top -= 4; *(uint32_t*)stack_top = 0;

    return (uint32_t)stack_top;
}

int task_create(void (*entry_point)(), const char* name) {
    int slot = find_free_tcb_slot();
    if (slot == -1) {
        terminal_writestring("SCHED: No free task slots available.\n");
        return -1;
    }

    tasks[slot].stack_base = (uint8_t*)kmalloc(TASK_STACK_SIZE);
    if (!tasks[slot].stack_base) {
        terminal_writestring("SCHED: Failed to allocate stack for new task.\n");
        return -1;
    }

    tasks[slot].esp = setup_task_stack(tasks[slot].stack_base, entry_point);
    tasks[slot].active = 1;
    tasks[slot].id = next_task_id++;
    tasks[slot].name = name;

    terminal_writestring("SCHED: Created task '");
    terminal_writestring(name);
    terminal_writestring("' (ID: ");
    char id_str[10]; int_to_str(tasks[slot].id, id_str); terminal_writestring(id_str);
    terminal_writestring(") in slot ");
    int_to_str(slot, id_str); terminal_writestring(id_str);
    terminal_writestring(".\n");

    return tasks[slot].id;
}

void task_yield() {
    if (current_task_idx == -1) {
        return;
    }

    int old_task_idx = current_task_idx;
    int next_task_to_run = -1;

    for (int i = 1; i <= MAX_TASKS; i++) {
        int next_idx = (old_task_idx + i) % MAX_TASKS;
        if (tasks[next_idx].active) {
            next_task_to_run = next_idx;
            break;
        }
    }

    if (next_task_to_run == -1 || next_task_to_run == old_task_idx) {
        return;
    }

    current_task_idx = next_task_to_run;

    context_switch(&tasks[old_task_idx].esp, tasks[current_task_idx].esp);
}

void task_terminate_self() {
    if (current_task_idx == -1) {
        terminal_writestring("SCHED: Cannot terminate, no current task.\n");
        return;
    }

    terminal_writestring("SCHED: Terminating task '");
    terminal_writestring(tasks[current_task_idx].name);
    terminal_writestring("' (ID: ");
    char id_str[10]; int_to_str(tasks[current_task_idx].id, id_str); terminal_writestring(id_str);
    terminal_writestring(").\n");

    tasks[current_task_idx].active = 0;
    kfree(tasks[current_task_idx].stack_base);

    int old_task_idx = current_task_idx;
    current_task_idx = -1;

    int next_task_to_run = -1;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].active) {
            next_task_to_run = i;
            break;
        }
    }

    if (next_task_to_run == -1) {
        terminal_writestring("SCHED: All tasks terminated. Kernel will now halt.\n");
        asm volatile("hlt");
    }

    current_task_idx = next_task_to_run;
    context_switch(NULL, tasks[current_task_idx].esp);
}

void task_a_entry() {
    int count = 0;
    while (count < 5) {
        terminal_writestring("Task A: Running (");
        char num_str[10]; int_to_str(count, num_str); terminal_writestring(num_str);
        terminal_writestring(")\n");
        count++;
        task_yield();
    }
    terminal_writestring("Task A: Done.\n");
    task_terminate_self();
}

void task_b_entry() {
    int count = 0;
    while (count < 3) {
        terminal_writestring("Task B: Working (");
        char num_str[10]; int_to_str(count, num_str); terminal_writestring(num_str);
        terminal_writestring(")\n");
        count++;
        task_yield();
    }
    terminal_writestring("Task B: Completed.\n");
    task_terminate_self();
}

void task_c_entry() {
    terminal_writestring("Task C: Hello, World!\n");
    task_terminate_self();
}

void cmd_scheduler_start(const char* args) {
    if (current_task_idx != -1) {
        terminal_writestring("SCHED: Scheduler already running.\n");
        return;
    }

    terminal_writestring("SCHED: Initializing scheduler...\n");
    for (int i = 0; i < MAX_TASKS; i++) {
        tasks[i].active = 0;
        tasks[i].stack_base = NULL;
    }
    next_task_id = 0;

    task_create(task_a_entry, "Task A");
    task_create(task_b_entry, "Task B");
    task_create(task_c_entry, "Task C");

    current_task_idx = -1;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].active) {
            current_task_idx = i;
            break;
        }
    }

    if (current_task_idx == -1) {
        terminal_writestring("SCHED: No tasks to run. Scheduler not started.\n");
        return;
    }

    terminal_writestring("SCHED: Starting first task.\n");
    uint32_t dummy_esp;
    context_switch(&dummy_esp, tasks[current_task_idx].esp);

    terminal_writestring("SCHED: Scheduler returned to init - unexpected!\n");
}

void cmd_task_yield(const char* args) {
    if (current_task_idx == -1) {
        terminal_writestring("SCHED: Scheduler not running, no task to yield from.\n");
        return;
    }
    task_yield();
}

void cmd_list_tasks(const char* args) {
    terminal_writestring("SCHED: Active Tasks:\n");
    char id_str[10];
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].active) {
            terminal_writestring("  - ID: ");
            int_to_str(tasks[i].id, id_str); terminal_writestring(id_str);
            terminal_writestring(", Name: '");
            terminal_writestring(tasks[i].name);
            terminal_writestring("'\n");
        }
    }
    terminal_writestring("SCHED: Current Task: ");
    if (current_task_idx != -1) {
        terminal_writestring(tasks[current_task_idx].name);
        terminal_writestring(" (ID: ");
        int_to_str(tasks[current_task_idx].id, id_str); terminal_writestring(id_str);
        terminal_writestring(")\n");
    } else {
        terminal_writestring("None\n");
    }
}

int scheduler_extension_init(void) {
    terminal_writestring("SCHED: Basic Cooperative Scheduler Extension Initializing...\n");

    for (int i = 0; i < MAX_TASKS; i++) {
        tasks[i].active = 0;
        tasks[i].stack_base = NULL;
    }
    current_task_idx = -1;

    register_command("sched_start", cmd_scheduler_start, "Start the cooperative scheduler with example tasks", scheduler_ext_id);
    register_command("task_yield", cmd_task_yield, "Manually yield CPU to the next task", scheduler_ext_id);
    register_command("list_tasks", cmd_list_tasks, "List all active tasks", scheduler_ext_id);

    terminal_writestring("SCHED: Extension Initialized. Use 'sched_start' to begin.\n");
    return 0;
}

void scheduler_extension_cleanup(void) {
    terminal_writestring("SCHED: Scheduler Extension Cleaning up...\n");
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].active) {
            kfree(tasks[i].stack_base);
            tasks[i].active = 0;
            tasks[i].stack_base = NULL;
        }
    }
    terminal_writestring("SCHED: Cleanup complete.\n");
}

__attribute__((section(".ext_register_fns")))
void __scheduler_auto_register(void) {
    scheduler_ext_id = register_extension("SCHEDULER", "1.0",
                                          scheduler_extension_init,
                                          scheduler_extension_cleanup);
    if (scheduler_ext_id >= 0) {
        load_extension(scheduler_ext_id);
    } else {
        terminal_writestring("Failed to register Scheduler Extension (auto)!\n");
    }
}
