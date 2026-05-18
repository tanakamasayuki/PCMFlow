// Single translation unit that emits dr_mp3's implementation.
// Declarations are pulled in normally from `Mp3Decoder.cpp`; this file
// exists only to satisfy DR_MP3_IMPLEMENTATION's one-definition rule.

#define DR_MP3_IMPLEMENTATION
#define DR_MP3_NO_STDIO
#include "dr_mp3.h"
