#include <string.h>

#include "context.h"
#include "token.h"

bool token_text_equal(Context* context, Token* lhs, Token* rhs) {
  size_t len = token_text_len(lhs);
  if (len != token_text_len(rhs)) { return false; }

  return strncmp(context->source +lhs->start, context->source + rhs->start, len) == 0;
}
