# Combat integrations

`PrecisionAPI.h` is copied from [Precision](https://github.com/ersh1/Precision/blob/main/src/PrecisionAPI.h)
under its [MIT license](Precision-LICENSE.txt). The request helper uses
`REX::W32::GetModuleHandleA` for the pinned CommonLib version. Interface layout
and virtual function order are unchanged. YLIWF requests interface V1.

`Precision.cpp` registers a passive post-hit callback. Callback threads update
only counters for a bounded set of tracked actors; game-thread observations
cache collision length for 500 ms. No pre-hit, damage or collision-filter
callbacks are registered. Engine hit events and these contacts remain separate
metrics. Missing API or geometry falls back to ordinary reach estimates.
