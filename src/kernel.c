
#include "terminal.h"
#include "keyboard.h"

#define COMMAND_SIZE 128

static char command[COMMAND_SIZE];
static int command_length = 0;

static int string_equals(const char *a, const char *b)
{
    int i = 0;

    while (a[i] != '\0' && b[i] != '\0')
    {
        if (a[i] != b[i])
            return 0;

        i++;
    }

    return a[i] == '\0' && b[i] == '\0';
}

static void execute_command(void)
{
    if (string_equals(command, "help"))
    {
        terminal_write("StanShell< commands:");
        terminal_write_line("  help  - Show commands");
        terminal_write_line("  about - About StanOS");
        terminal_write_line("  clear - Clear the screen");
        terminal_write_line("  echo  - Print text");
    }
    else if (string_equals(command, "about"))
    {
        terminal_write_line("StanOS - A hobby OS written in C.");
        terminal_write_line("Welcome to StanShell!");
    }
    else if (string_equals(command, "clear"))
    {
        terminal_clear();
    }
    else if (command_length >= 5 &&
             command[0] == 'e' &&
             command[1] == 'c' &&
             command[2] == 'h' &&
             command[3] == 'o' &&
             command[4] == ' ')
    {
        terminal_write_line(command + 5);
    }
    else if (command_length > 0)
    {
        terminal_write("Unknown command: ");
        terminal_write_line(command);
    }
}

void kernel_main(void)
{
    terminal_initialize();

    terminal_write_line("Welcome to StanOS!");
    terminal_write_line("StanShell 0.1");
    terminal_write_line("Type 'help' to get started.");
    terminal_put_char('\n');

    terminal_write("stan@stanos:~$ ");

    for (;;)
    {

        terminal_cursor_tick();
        char c = keyboard_poll_char();

        if (c == '\n')
        {
            
            terminal_put_char('\n');
            command[command_length] = '\0';

            execute_command();

            command_length = 0;
            command[0] = '\0';

            terminal_write("stan@stanos:~$ ");
        }
        else if (c == '\b')
        {
            if (command_length > 0)
            {
                command_length--;
                command[command_length] = '\0';
                terminal_put_char('\b');
            }
        }
        else if (c >= 32 && c <= 126)
        {
            if (command_length < COMMAND_SIZE - 1)
            {
                command[command_length++] = c;
                terminal_put_char(c);
            }
        }
    }
}



