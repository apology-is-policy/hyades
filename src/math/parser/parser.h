#pragma once
#include "math/ast.h"
#include "utils/error.h"

Ast *parse_math(const char *src, ParseError *err);