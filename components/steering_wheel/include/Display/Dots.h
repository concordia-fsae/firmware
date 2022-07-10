/**
 * @file Dots.h
 * @brief Header file for Info/Text Dot implementation
 */


/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "Colors.h"
#include "DisplayTypes.h"


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    INFO_REL_POS_ABOVE = 0U,
    INFO_REL_POS_BELOW,
    INFO_REL_POS_LEFT,
    INFO_REL_POS_RIGHT,
    INFO_REL_POS_COUNT,
} InfoRelPos_E;

/******************************************************************************
 *                               M A C R O S
 ******************************************************************************/

#define DECL_infoDotUpdater(d)                   static void SNAKE(update, d)(struct s_InfoDot *dot, bool state)
#define DECL_infoDotUpdaterGeneric(d)            DECL_infoDotUpdater(d){ update_infoDotGeneric(dot, state); }
#define DECL_infoTextUpdater(d)                  static void SNAKE(update, d)(struct s_InfoText *dot, bool state)

// *FORMAT-OFF*
#define DECL_infoDot(d, l, px, py, s, rp)    \
    [d] =                                    \
    {                                        \
        .dot =                               \
        {                                    \
            .coords = { .x = px, .y = py },  \
            .color  = WHITE,                 \
            .size   = s,                     \
        },                                   \
        .label =                             \
        {                                    \
            .relPos   = rp,                  \
            .color    = WHITE,               \
            .fontSize = 21U,                 \
            .text     = l,                   \
        },                                   \
        .update       = &SNAKE(update, d),   \
    }

#define DECL_infoText(dot, l, px, py, rp)     \
    [dot] =                                   \
    {                                         \
        .status =                             \
        {                                     \
            .coords   = { .x = px, .y = py }, \
            .color    = WHITE,                \
            .text     = "",                   \
            .fontSize = 21U,                  \
        },                                    \
        .label =                              \
        {                                     \
            .relPos   = rp,                   \
            .color    = WHITE,                \
            .text     = l,                    \
            .fontSize = 21U,                  \
        },                                    \
        .update       = &SNAKE(update, dot),  \
    }
// *FORMAT-ON*


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct s_InfoDot
{
    struct
    {
        Coordinates_S coords;
        Color_t       color;
        uint16_t      size;
    } dot;

    struct
    {
        InfoRelPos_E relPos;
        Color_t      color;
        char         text[10];
        uint16_t     fontSize;
    } label;

    void (*update)(struct s_InfoDot *dot, bool state);
} InfoDot_S;

typedef struct s_InfoText
{
    struct
    {
        Coordinates_S coords;
        Color_t       color;
        char          text[10];
        uint16_t      fontSize;
    } status;

    struct
    {
        InfoRelPos_E relPos;
        Color_t      color;
        char         text[10];
        uint16_t     fontSize;
    } label;

    void (*update)(struct s_InfoText *dot, bool state);
} InfoText_S;


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

void render_InfoDot(InfoDot_S dot);
void render_InfoDots(InfoDot_S dots[], uint8_t count);

void render_InfoText(InfoText_S dot);
void render_InfoTexts(InfoText_S dots[], uint8_t count);
