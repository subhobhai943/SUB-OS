// SUB-OS Desktop Icon Set: 16x16 palette-indexed glyphs
//
// Each icon is 16 rows of 16 hex digits, where the digit indexes the shared
// palette below. Storing the art as text keeps it editable by hand, and the
// indirection lets every glyph pick up a theme change for free.

#include <gui/gui_icons.h>
#include <gui/gui_gfx.h>
#include <gui/gui_theme.h>

#define ICON_PALETTE_SIZE 16

static const uint32_t g_palette[ICON_PALETTE_SIZE] = {
    0x00000000, // 0 transparent
    0xFF0B0F19, // 1 outline / near-black
    0xFFF8FAFC, // 2 white
    0xFF94A3B8, // 3 muted grey
    0xFF475569, // 4 dark grey
    0xFF38BDF8, // 5 primary sky
    0xFF0284C7, // 6 primary dark
    0xFF34D399, // 7 success green
    0xFFFBBF24, // 8 warning amber
    0xFFF87171, // 9 danger red
    0xFF818CF8, // A accent indigo
    0xFFEC4899, // B pink
    0xFF1E293B, // C surface
    0xFF334155, // D elevated
    0xFF000000, // E pure black
    0xFFD9A441  // F folder tan
};

static const char* g_icons[GUI_ICON_COUNT][GUI_ICON_SIZE] = {
    // GUI_ICON_TERMINAL
    {
        "4111111111111114",
        "4CCCCCCCCCCCCCC4",
        "4CCCCCCCCCCCCCC4",
        "4C7CCCCCCCCCCCC4",
        "4C77CCCCCCCCCCC4",
        "4C777CCCCCCCCCC4",
        "4C7777CCCCCCCCC4",
        "4C777CCCCCCCCCC4",
        "4C77CCCCCCCCCCC4",
        "4C7CCCCCCCCCCCC4",
        "4CCCCCCCCCCCCCC4",
        "4CC77777777CCCC4",
        "4CCCCCCCCCCCCCC4",
        "4CCCCCCCCCCCCCC4",
        "4CCCCCCCCCCCCCC4",
        "4111111111111114",
    },
    // GUI_ICON_MONITOR
    {
        "4111111111111114",
        "4CCCCCCCCCCCCCC4",
        "4CCCCCCCCCCCC5C4",
        "4CCCCCCCCCCC5CC4",
        "4CCCCCCCCC55CCC4",
        "4CCCCCCCC5CCCCC4",
        "4CCCCCC55CCCCCC4",
        "4CCCC55CCCCCCCC4",
        "4CC55CCCCCCCCCC4",
        "4C5CCCCCCCCCCCC4",
        "4CCCCCCCCCCCCCC4",
        "4111111111111114",
        "0000444444440000",
        "0000004444000000",
        "0000444444440000",
        "0000000000000000",
    },
    // GUI_ICON_FOLDER
    {
        "0000000000000000",
        "0FFFFF0000000000",
        "0F888FF000000000",
        "0F88888FFFFFFF00",
        "0F8888888888F000",
        "0F8888888888F000",
        "0F8888888888F000",
        "0F8888888888F000",
        "0F8888888888F000",
        "0F8888888888F000",
        "0F8888888888F000",
        "0F8888888888F000",
        "0FFFFFFFFFFFF000",
        "0000000000000000",
        "0000000000000000",
        "0000000000000000",
    },
    // GUI_ICON_CALC
    {
        "4111111111111114",
        "4CCCCCCCCCCCCCC4",
        "4C777777777777C4",
        "4C777777777777C4",
        "4CCCCCCCCCCCCCC4",
        "4C2CC2CC2CC2CCC4",
        "4CCCCCCCCCCCCCC4",
        "4C2CC2CC2CC2CCC4",
        "4CCCCCCCCCCCCCC4",
        "4C2CC2CC2CC2CCC4",
        "4CCCCCCCCCCCCCC4",
        "4C2CC2CC2CC5CCC4",
        "4CCCCCCCCCCCCCC4",
        "4CCCCCCCCCCCCCC4",
        "4CCCCCCCCCCCCCC4",
        "4111111111111114",
    },
    // GUI_ICON_PAINT
    {
        "0000000000000000",
        "0000333333000000",
        "0003222222300000",
        "0032955992230000",
        "0329999999923000",
        "0329B99997923000",
        "0329999999923000",
        "0032999999230000",
        "0003299992300000",
        "0000322223000000",
        "0000033330000000",
        "0000004440000000",
        "0000004440000000",
        "0000004440000000",
        "0000000000000000",
        "0000000000000000",
    },
    // GUI_ICON_INFO
    {
        "0000055555000000",
        "0000555555550000",
        "0005566665550000",
        "0055666666655000",
        "0556662266665500",
        "0556662266665500",
        "0556666666665500",
        "0556662266665500",
        "0556622226665500",
        "0556666226665500",
        "0556666226665500",
        "0556662222265500",
        "0055666666655000",
        "0005566665550000",
        "0000555555550000",
        "0000055555000000",
    },
    // GUI_ICON_SETTINGS
    {
        "0000034443000000",
        "0000034443000000",
        "0003344444330000",
        "0034444444443000",
        "0344444444444300",
        "3444442224444430",
        "4444422222444444",
        "4444422222444444",
        "4444422222444444",
        "3444442224444430",
        "0344444444444300",
        "0034444444443000",
        "0003344444330000",
        "0000034443000000",
        "0000034443000000",
        "0000000000000000",
    },
    // GUI_ICON_TASKS
    {
        "0000000000000000",
        "0555555555555550",
        "0500000000000050",
        "0507000000000050",
        "0507700000000050",
        "0507770000000050",
        "0507777000000050",
        "0500000000000050",
        "0508880000000050",
        "0508888000000050",
        "0500000000000050",
        "0509990000000050",
        "0509999000000050",
        "0500000000000050",
        "0555555555555550",
        "0000000000000000",
    },
    // GUI_ICON_LOG
    {
        "0022222222222000",
        "0244444444442000",
        "0240000000042000",
        "0244444444442000",
        "0240000000042000",
        "0244444444442000",
        "0240000000042000",
        "0244444444442000",
        "0240000000042000",
        "0244444444442000",
        "0240000000042000",
        "0244444444442000",
        "0240000000042000",
        "0244444444442000",
        "0022222222222000",
        "0000000000000000",
    },
    // GUI_ICON_FLASK
    {
        "0000022222000000",
        "0000020002000000",
        "0000020002000000",
        "0000020002000000",
        "0000020002000000",
        "0000200000200000",
        "0000200000200000",
        "0002000000020000",
        "0002077777020000",
        "0020777777702000",
        "0020777777702000",
        "0207777777770200",
        "0207777777770200",
        "0027777777772000",
        "0002222222220000",
        "0000000000000000",
    },
    // GUI_ICON_CLOCK
    {
        "0000055555000000",
        "0005522222550000",
        "0052222222225000",
        "0522222222222500",
        "0522221222222500",
        "5222221222222250",
        "5222221222222250",
        "5222221222222250",
        "5222221111122250",
        "5222222222222250",
        "5222222222222250",
        "0522222222222500",
        "0522222222222500",
        "0052222222225000",
        "0005522222550000",
        "0000055555000000",
    },
    // GUI_ICON_FILE
    {
        "0022222222200000",
        "0244444444200000",
        "0244444444420000",
        "0244444444442000",
        "0244444444444200",
        "0244444444444200",
        "0244444444444200",
        "0244444444444200",
        "0244444444444200",
        "0244444444444200",
        "0244444444444200",
        "0244444444444200",
        "0244444444444200",
        "0244444444444200",
        "0222222222222200",
        "0000000000000000",
    },
    // GUI_ICON_POWER
    {
        "0000009900000000",
        "0000009900000000",
        "0000999999000000",
        "0009900009900000",
        "0099009900990000",
        "0990009900099000",
        "0990009900099000",
        "9900009900009900",
        "9900009900009900",
        "9900000000009900",
        "9900000000009900",
        "0990000000099000",
        "0990000000099000",
        "0099000000990000",
        "0009999999900000",
        "0000099999000000",
    },
    // GUI_ICON_WARNING
    {
        "0000008800000000",
        "0000008800000000",
        "0000088880000000",
        "0000088880000000",
        "0000881188000000",
        "0000881188000000",
        "0008811118800000",
        "0008811118800000",
        "0088111111880000",
        "0088111111880000",
        "0881111111188000",
        "0881118811188000",
        "8811118811118800",
        "8811118811118800",
        "8888888888888888",
        "8888888888888888",
    },
    // GUI_ICON_HEART
    {
        "0000000000000000",
        "0009BB9009BB9000",
        "009BBBB99BBBB900",
        "09BBBBBBBBBBBB90",
        "09BB2BBBBBBBBB90",
        "09B22BBBBBBBBB90",
        "09BB2BBBBBBBBB90",
        "09BBBBBBBBBBBB90",
        "009BBBBBBBBBB900",
        "0009BBBBBBBB9000",
        "00009BBBBBB90000",
        "000009BBBB900000",
        "0000009BB9000000",
        "0000000990000000",
        "0000000000000000",
        "0000000000000000",
    },
    // GUI_ICON_NETWORK
    {
        "0000000000000000",
        "0000005555000000",
        "0000556666550000",
        "0005666556665000",
        "0056666556666500",
        "0555555555555550",
        "0566666556666650",
        "5666666556666665",
        "5666666556666665",
        "0566666556666650",
        "0555555555555550",
        "0056666556666500",
        "0005666556665000",
        "0000556666550000",
        "0000005555000000",
        "0000000000000000",
    },
    // GUI_ICON_GLOBE
    {
        "0000055555500000",
        "0003556555655300",
        "0035565556555300",
        "0355556555655530",
        "0355565556555530",
        "0355556555655530",
        "3555565556555553",
        "3666666666666663",
        "3555565556555553",
        "0355556555655530",
        "0355565556555530",
        "0355556555655530",
        "0035565556555300",
        "0003556555655300",
        "0000055555500000",
        "0000000000000000",
    },};

static const char* g_icon_names[GUI_ICON_COUNT] = {
    "Terminal", "Monitor", "Folder", "Calculator", "Paint", "Info",
    "Settings", "Tasks", "Log", "Tests", "Clock", "File", "Power", "Warning",
    "Heart", "Network", "Globe"
};

static uint8_t hex_index(char c) {
    if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
    if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
    if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
    return 0;
}

void gui_icons_init(void) {
    // The glyph table is static const; nothing to build at runtime.
}

const char* gui_icon_name(gui_icon_id_t id) {
    if (id < 0 || id >= GUI_ICON_COUNT) return "Unknown";
    return g_icon_names[id];
}

void gui_icon_draw_scaled(gui_icon_id_t id, int x, int y, int scale) {
    if (id < 0 || id >= GUI_ICON_COUNT) return;
    if (scale < 1) scale = 1;

    for (int row = 0; row < GUI_ICON_SIZE; row++) {
        const char* line = g_icons[id][row];
        if (!line) continue;

        for (int col = 0; col < GUI_ICON_SIZE; col++) {
            uint32_t color = g_palette[hex_index(line[col])];
            if ((color >> 24) == 0) continue; // Transparent pixel

            if (scale == 1) {
                gui_gfx_draw_pixel(x + col, y + row, color);
            } else {
                gui_gfx_fill_rect(x + col * scale, y + row * scale, scale, scale, color);
            }
        }
    }
}

void gui_icon_draw(gui_icon_id_t id, int x, int y) {
    gui_icon_draw_scaled(id, x, y, 1);
}

void gui_icon_draw_tinted(gui_icon_id_t id, int x, int y, int scale, uint32_t tint) {
    if (id < 0 || id >= GUI_ICON_COUNT) return;
    if (scale < 1) scale = 1;

    // Every opaque pixel becomes the tint; the glyph acts purely as a stencil.
    for (int row = 0; row < GUI_ICON_SIZE; row++) {
        const char* line = g_icons[id][row];
        if (!line) continue;

        for (int col = 0; col < GUI_ICON_SIZE; col++) {
            if ((g_palette[hex_index(line[col])] >> 24) == 0) continue;

            if (scale == 1) {
                gui_gfx_draw_pixel(x + col, y + row, tint);
            } else {
                gui_gfx_fill_rect(x + col * scale, y + row * scale, scale, scale, tint);
            }
        }
    }
}
