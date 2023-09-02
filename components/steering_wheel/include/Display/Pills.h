/**
 * @file InfoPill.h
 * @brief Header file for InfoPill display item implementation
 */


/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "Colors.h"
#include "FloatTypes.h"
#include "DisplayTypes.h"


/******************************************************************************
 *                               M A C R O S
 ******************************************************************************/

#define DECL_valuePillUpdate(p)    static void SNAKE(update, p)(struct s_ValuePill *pill, float val)
#define DECL_valuePill(pill, l, x_dim, y_dim, w, h, a, p, u) \
    [pill] =                                                 \
    {                                                        \
        .coords    = { .x = x_dim, .y = y_dim },             \
        .height    = h,                                      \
        .width     = w,                                      \
        .angle     = a,                                      \
        .label     = l,                                      \
        .bgColor   = 0U,                                     \
        .fgColor   = 0U,                                     \
        .value     = 0U,                                     \
        .precision = p,                                      \
        .unit      = u,                                      \
        .update    = &SNAKE(update, pill),                   \
    }                                                        \


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct s_ValuePill
{
    Coordinates_S coords;
    uint16_t      height;
    uint16_t      width;
    uint16_t      angle;

    char          label[20];

    void (*update)(struct s_ValuePill *pill, float32_t val);
    Color_t       bgColor;
    Color_t       fgColor;
    Color_t       labelColor;
    float32_t     value;
    uint8_t       precision;
    char          unit[4];
} ValuePill_S;


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

void render_ValuePill(ValuePill_S pill);
void render_ValuePills(ValuePill_S pills[], uint8_t count);
