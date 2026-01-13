#!/bin/bash
gcc -O2 -o wyn_new \
  src/main.c \
  src/lexer.c \
  src/parser.c \
  src/checker.c \
  src/codegen.c \
  src/memory.c \
  src/generics.c \
  src/traits.c \
  src/patterns.c \
  src/closures.c \
  src/error.c \
  src/optimization.c \
  src/safe_memory.c \
  src/platform.c \
  src/wyn_wrapper.c \
  src/wyn_interface.c \
  src/io.c \
  src/optional.c \
  src/result.c \
  src/arc_runtime.c \
  src/type_inference.c \
  src/arc_insertion.c \
  src/arc_operations.c \
  src/arc_optimization.c \
  src/weak_references.c \
  src/weak_codegen.c \
  src/cycle_detection.c \
  src/escape_analysis.c \
  -lm 2>&1 | head -30
