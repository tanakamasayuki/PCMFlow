# Vendored decoder libraries — credits and licenses

PCMFlow vendors two single-header decoder libraries from the **dr_libs** project. We are deeply grateful to the authors for releasing this work under permissive terms that allow projects like PCMFlow to build on it.

## Acknowledgements

**dr_mp3 / dr_flac**
[David Reid](mailto:mackron@gmail.com) — author and maintainer of [dr_libs](https://github.com/mackron/dr_libs).
A collection of single-file audio libraries that have become the de facto
choice for embeddable, dependency-free codec implementations.

**minimp3** (the foundation that dr_mp3 builds upon)
[Lion (lieff)](https://github.com/lieff) — author of [minimp3](https://github.com/lieff/minimp3).
As the dr_mp3 header itself states: *"Based on minimp3 — which is where the real work was done."* PCMFlow's MP3 support exists thanks to this lineage.

Neither author has any affiliation with PCMFlow. Issues with the codec
implementations themselves should be reported upstream to dr_libs (or
minimp3 for MP3-decoder-specific concerns), not to PCMFlow.

## Vendored files

| File in this directory | Upstream project | Upstream license | Version note |
|----------------------|------------------|------------------|--------------|
| `dr_mp3.h`           | [mackron/dr_libs](https://github.com/mackron/dr_libs) | Public Domain (Unlicense) **or** MIT-0 (author's choice) | Pulled verbatim from upstream `master`. |
| `dr_flac.h`          | [mackron/dr_libs](https://github.com/mackron/dr_libs) | Public Domain (Unlicense) **or** MIT-0 (author's choice) | Pulled verbatim from upstream `master`. |

The files are kept **unmodified** so that the upstream license blocks, version banners, and revision history at the top and bottom of each header remain intact. To update them, replace the files in this directory with the latest upstream copies.

## License options

The authors offer each file under your choice of either of the two
licenses below. Both are compatible with PCMFlow's MIT license, and
neither imposes an attribution requirement on downstream users. We
nevertheless credit the authors here as a matter of respect.

### Option 1 — Unlicense (public domain dedication)

> This is free and unencumbered software released into the public domain.
>
> Anyone is free to copy, modify, publish, use, compile, sell, or distribute this software, either in source code form or as a compiled binary, for any purpose, commercial or non-commercial, and by any means.
>
> In jurisdictions that recognize copyright laws, the author or authors of this software dedicate any and all copyright interest in the software to the public domain. We make this dedication for the benefit of the public at large and to the detriment of our heirs and successors. We intend this dedication to be an overt act of relinquishment in perpetuity of all present and future rights to this software under copyright law.
>
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
>
> For more information, please refer to <http://unlicense.org/>

### Option 2 — MIT No Attribution (MIT-0)

> Copyright 2023 David Reid
>
> Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so.
>
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

The full, authoritative license text for each library is reproduced at
the **bottom of `dr_mp3.h` / `dr_flac.h`**; this file is a summary for
convenience and credit.
