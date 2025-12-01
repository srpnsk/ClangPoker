// enum или #define для цветов и значений
typedef enum { RED, YELLOW, GREEN, BLUE, WILD_COLOR } CardColor;
typedef enum {
    ZERO, ONE, TWO, THREE, FOUR, FIVE, SIX, SEVEN, EIGHT, NINE,
    SKIP, REVERSE, DRAW_TWO, WILD, WILD_FOUR, WILD_VALUE
} CardValue;

typedef struct {
    CardColor color;
    CardValue value;
} Card;
