# Attribution and data notice

Castaway Lookout (formerly JohnnyCx64) is derived from Ralph Caraveo's Go/Raylib port at
`deckarep/Johnny-Castaway-2026-Public`, which is derived from `jc_reborn`.
Their contributions and Git history are preserved in the
[technical reference](https://github.com/DrWize/Castaway-Lookout/blob/main/docs/DEVELOPMENT.md).

The engine source and new modifications are distributed under GNU GPL version
3 or, at your option, any later version. See `LICENSE`.

Sierra/Dynamix artwork, scripts, archives, and sounds are not licensed by this
repository and are not included. Johnny Castaway and related original content
remain the intellectual property of their respective owners.

The ESP32 weather sidebar includes adapted 32x32 pixel weather icons by Dhole
from [weather-pixel-icons](https://github.com/Dhole/weather-pixel-icons),
licensed under [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/).
The imported masks are pinned to upstream commit
`8438acf418158290ac7ff361387a38d3e4983275`.
The adaptation converts the original monochrome XBM rows into compact C masks,
separates compound artwork into RGB565 colour layers, and renders every source
pixel as an exact 2x2 block. The adapted masks are distributed with this GPLv3
project under the one-way CC BY-SA 4.0 to GPLv3 compatibility mechanism while
retaining the original attribution and change notice.
