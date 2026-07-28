# `ui/` — what the player sees

OpenCV-based rendering, sprite animation, mouse input, and the networked client.
This is the only layer that draws anything, and the only one with any
platform-specific code at all.

Depends on `logic` and `protocol`. `logic` never depends on it, which is why the
whole test suite runs without OpenCV on the include path.

## What is in here

| Folder | Responsibility |
|--------|----------------|
| `primitives/` | `Img` — the wrapper around `cv::Mat`. Load, resize, crop, clone, draw text, and alpha-blend one image onto another. Almost everything else goes through this rather than touching OpenCV directly. |
| `animation/` | Per-piece animation: sprite sets loaded from disk, eased interpolation between cells, and a registry that advances every piece each frame. |
| `rendering/` | Drawing the pieces, the HUD (score and move log), the board layout, and the banners for intro, game over, searching and the disconnect countdown. |
| `input/` | `MouseInputAdapter` (OpenCV clicks → `Controller`) and `ScreenMapper` (window pixels → canvas pixels, recomputed live so clicks stay correct after a resize). |
| `net/` | `ServerLink` — the networked `IGameView`. Same interface as the local game, so the renderer cannot tell the difference. |
| `app/` | `GameSession` — owns whichever backend the command line selects, local or networked. |
| `dialogs/` | The room dialog and message boxes, behind `IRoomPrompt`. |
| `audio/` | The sound backend, behind `ISoundPlayer`. |
| `platform/` | Screen size and DPI awareness. |

## Local and networked are interchangeable

Both `Game` (local) and `ServerLink` (networked) implement `IGameView`, so the
renderer, the animator and the HUD never learn which one they are driving. The
difference is where the truth lives: locally the board simulates in-process,
while over the network the server alone decides what happened and the client's
board is a mirror updated only by `BoardUpdate`.

To keep animation smooth anyway, the server broadcasts a `MotionStarted` the
moment a move begins and the client predicts that motion locally — but a
prediction never moves a piece. The real arrival does, and it silently corrects
whatever was predicted.

## The alpha blend

`Img::draw_on` is the render loop's hottest arithmetic, and two details there
are deliberate:

- It uses `Mat::mul` (element-wise), **never** `operator*` between two `Mat`s —
  in OpenCV that means matrix multiplication, which is not per-pixel blending.
- It works in `CV_32F`, not `CV_64F`: inputs and outputs are 8-bit, so float
  carries far more precision than needed while halving the memory traffic.

Images with no real transparency take a plain `copyTo` fast path instead.

## Platform-specific code

Exactly two things here cannot be written once and compiled everywhere. Both sit
behind an interface, and the **only** `#ifdef` in the client picks the
implementation:

| Interface | Windows | Everywhere else |
|-----------|---------|-----------------|
| `IRoomPrompt` (dialogs) | `win_room_prompt.cpp` — real native controls via Win32 | `opencv_room_prompt.cpp` — drawn with OpenCV, which is already a dependency, so nothing new is needed |
| `ISoundPlayer` (audio) | `win_sound_player.cpp` — `PlaySound` via winmm | `silent_sound_player.cpp` — silent for now; drop in [miniaudio](https://github.com/mackron/miniaudio) to give it sound |
| screen size / DPI | `SetProcessDPIAware` + `GetSystemMetrics` | a 1080p default — the window is resizable and every layout decision reads its actual size each frame |

`main.cpp` contains no `#include <windows.h>` and no `#ifdef`. It asks the
factories for a prompt and a sound player and uses them.

## Sound cue names

The four `.wav` files the game plays are listed in
[assets/README.md](../assets/README.md).
