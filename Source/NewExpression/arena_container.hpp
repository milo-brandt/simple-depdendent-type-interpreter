#ifndef EXPRESSION_ARENA_CONTAINER_HPP
#define EXPRESSION_ARENA_CONTAINER_HPP

#include "arena.hpp"

/*
  This file defines utilities for holding expressions.

  It is largely necessitated by the fact that it's difficult to
  make a good RAII container for expressions, since the arena
  to which they belong needs to be notified on destruction.
*/

#endif
