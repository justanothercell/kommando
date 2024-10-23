#include "lib/enum.h"

ENUM(TypeT,
    TYPE_PRIMITIVE
);

typedef struct Type {
    TypeT type;
} Type;

typedef struct TypePrimitive ;