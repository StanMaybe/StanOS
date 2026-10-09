#include "terminal.h"

#define VBE_INFO_ADDRESS 0x9000
#define FONT_ADDRESS     0xA000
#define FONT_WIDTH       8
#define FONT_HEIGHT      16

static volatile uint8_t *framebuffer;
static const uint8_t *font;

static uint32_t screen_width;
static uint32_t screen_height;
static uint32_t framebuffer_pitch;
static uint8_t bits_per_pixel;
static uint8_t red_position;
static uint8_t green_position;
static uint8_t blue_position;

static uint32_t columns;
static uint32_t rows;
static uint32_t cursor;

static uint8_t current_foreground = COLOR_WHITE;
static uint8_t current_background = COLOR_BLACK;
static uint8_t cursor_visible = 1;
static uint32_t blink_counter;

static char screen_chars[128 * 48];
static uint8_t screen_colors[128 * 48];

static const uint8_t palette[16][3] = {
    {  0,   0,   0},
    {  0,   0, 170},
    {  0, 170,   0},
    {  0, 170, 170},
    {170,   0,   0},
    {170,   0, 170},
    {170,  85,   0},
    {170, 170, 170},
    { 85,  85,  85},
    { 85,  85, 255},
    { 85, 255,  85},
    { 85, 255, 255},
    {255,  85,  85},
    {255,  85, 255},
    {255, 255,  85},
    {255, 255, 255}
};

static uint32_t make_pixel(uint8_t color)
{
    return ((uint32_t)palette[color][0] << red_position) |
           ((uint32_t)palette[color][1] << green_position) |
           ((uint32_t)palette[color][2] << blue_position);
}

static void put_pixel(uint32_t x, uint32_t y, uint32_t pixel)
{
    if (x >= screen_width || y >= screen_height)
        return;

    uint32_t offset = y * framebuffer_pitch +
                      x * (bits_per_pixel / 8);

    framebuffer[offset] = (uint8_t)(pixel & 0xFF);
    framebuffer[offset + 1] = (uint8_t)((pixel >> 8) & 0xFF);
    framebuffer[offset + 2] = (uint8_t)((pixel >> 16) & 0xFF);

    if (bits_per_pixel == 32)
        framebuffer[offset + 3] = (uint8_t)((pixel >> 24) & 0xFF);
}

static void fill_rect(uint32_t x, uint32_t y,
                      uint32_t width, uint32_t height,
                      uint32_t color)
{
    for (uint32_t py = 0; py < height; py++)
        for (uint32_t px = 0; px < width; px++)
            put_pixel(x + px, y + py, color);
}

static void draw_cell(uint32_t index)
{
    if (index >= columns * rows)
        return;

    uint32_t cell_x = (index % columns) * FONT_WIDTH;
    uint32_t cell_y = (index / columns) * FONT_HEIGHT;

    uint8_t ch = (uint8_t)screen_chars[index];
    uint8_t colors = screen_colors[index];

    uint32_t foreground = make_pixel(colors & 0x0F);
    uint32_t background = make_pixel((colors >> 4) & 0x0F);

    const uint8_t *glyph = font + ((uint32_t)ch * FONT_HEIGHT);

    for (uint32_t y = 0; y < FONT_HEIGHT; y++)
    {
        uint8_t bits = glyph[y];

        for (uint32_t x = 0; x < FONT_WIDTH; x++)
        {
            uint32_t color = (bits & (0x80 >> x))
                           ? foreground : background;

            put_pixel(cell_x + x, cell_y + y, color);
        }
    }
}

static void draw_cursor(void)
{
    if (!cursor_visible || cursor >= columns * rows)
        return;

    uint32_t x = (cursor % columns) * FONT_WIDTH;
    uint32_t y = (cursor / columns) * FONT_HEIGHT;

    fill_rect(x, y + FONT_HEIGHT - 2, FONT_WIDTH, 2,
              make_pixel(COLOR_WHITE));
}

static void redraw_screen(void)
{
    for (uint32_t i = 0; i < columns * rows; i++)
        draw_cell(i);

    draw_cursor();
}

void terminal_set_color(enum terminal_color foreground,
                        enum terminal_color background)
{
    current_foreground = (uint8_t)foreground & 0x0F;
    current_background = (uint8_t)background & 0x0F;
}

void terminal_clear(void)
{
    cursor = 0;
    cursor_visible = 1;
    blink_counter = 0;

    uint8_t colors = (current_background << 4) |
                      current_foreground;

    for (uint32_t i = 0; i < columns * rows; i++)
    {
        screen_chars[i] = ' ';
        screen_colors[i] = colors;
    }

    redraw_screen();
}

void terminal_initialize(void)
{
    volatile uint8_t *info =
        (volatile uint8_t *)VBE_INFO_ADDRESS;

    framebuffer_pitch =
        *(volatile uint16_t *)(VBE_INFO_ADDRESS + 16);

    screen_width =
        *(volatile uint16_t *)(VBE_INFO_ADDRESS + 18);

    screen_height =
        *(volatile uint16_t *)(VBE_INFO_ADDRESS + 20);

    bits_per_pixel = info[25];

    red_position = info[32];
    green_position = info[34];
    blue_position = info[36];

    uint32_t framebuffer_address =
        *(volatile uint32_t *)(VBE_INFO_ADDRESS + 40);

    framebuffer = (volatile uint8_t *)(uintptr_t)
                  framebuffer_address;

    font = (const uint8_t *)FONT_ADDRESS;

    columns = screen_width / FONT_WIDTH;
    rows = screen_height / FONT_HEIGHT;

    if (columns > 128)
        columns = 128;

    if (rows > 48)
        rows = 48;

    current_foreground = COLOR_WHITE;
    current_background = COLOR_BLACK;

    terminal_clear();
}

static void scroll_if_needed(void)
{
    uint32_t count = columns * rows;

    if (cursor < count)
        return;

    for (uint32_t y = 0; y + 1 < rows; y++)
    {
        for (uint32_t x = 0; x < columns; x++)
        {
            uint32_t dst = y * columns + x;
            uint32_t src = (y + 1) * columns + x;

            screen_chars[dst] = screen_chars[src];
            screen_colors[dst] = screen_colors[src];
        }
    }

    uint32_t start = (rows - 1) * columns;
    uint8_t colors = (current_background << 4) |
                      current_foreground;

    for (uint32_t i = start; i < count; i++)
    {
        screen_chars[i] = ' ';
        screen_colors[i] = colors;
    }

    cursor = start;
    redraw_screen();
}

void terminal_put_char(char c)
{
    uint32_t old_cursor = cursor;
    uint32_t count = columns * rows;

    if (c == '\n')
    {
        cursor = ((cursor / columns) + 1) * columns;
    }
    else if (c == '\r')
    {
        cursor = (cursor / columns) * columns;
    }
    else if (c == '\b')
    {
        if (cursor > 0)
        {
            cursor--;
            screen_chars[cursor] = ' ';
            screen_colors[cursor] =
                (current_background << 4) | current_foreground;
        }
    }
    else
    {
        if (cursor < count)
        {
            screen_chars[cursor] = c;
            screen_colors[cursor] =
                (current_background << 4) | current_foreground;
            cursor++;
        }
    }

    scroll_if_needed();

    if (old_cursor < count)
        draw_cell(old_cursor);

    if (cursor < count)
        draw_cell(cursor);

    draw_cursor();
}

static int tag_is(const char *text, const char *tag)
{
    while (*tag)
    {
        if (*text == '\0' || *text != *tag)
            return 0;

        text++;
        tag++;
    }

    return 1;
}

void terminal_write(const char *text)
{
    while (*text != '\0')
    {
        if (tag_is(text, "<blue>"))
        {
            terminal_set_color(COLOR_LIGHT_BLUE, COLOR_BLACK);
            text += 6;
        }
        else if (tag_is(text, "<green>"))
        {
            terminal_set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
            text += 7;
        }
        else if (tag_is(text, "<red>"))
        {
            terminal_set_color(COLOR_LIGHT_RED, COLOR_BLACK);
            text += 5;
        }
        else if (tag_is(text, "<yellow>"))
        {
            terminal_set_color(COLOR_YELLOW, COLOR_BLACK);
            text += 8;
        }
        else if (tag_is(text, "<end>"))
        {
            terminal_set_color(COLOR_WHITE, COLOR_BLACK);
            text += 5;
        }
        else
        {
            terminal_put_char(*text);
            text++;
        }
    }
}

void terminal_write_line(const char *string)
{
    terminal_write(string);
    terminal_put_char('\n');
}

void terminal_cursor_tick(void)
{
    blink_counter++;

    if (blink_counter >= 500000)
    {
        blink_counter = 0;
        cursor_visible = !cursor_visible;

        if (cursor < columns * rows)
        {
            draw_cell(cursor);
            draw_cursor();
        }
    }
}