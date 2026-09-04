# OpenNFH Replay Format

Replay files are UTF-8 text and contain no decoded assets, screenshots, audio, or original script text.

The first line is the version header:

```text
version 1
```

Each later non-empty line is:

```text
tick action cursor_x cursor_y target
```

The supported action tokens are `pointer_click`, `scroll_left`, `scroll_right`, `scroll_up`, `scroll_down`, `center_woody`, `focus_neighbor`, `pause`, `screenshot`, `levelshot`, `quit`, `start_capture`, and `stop_capture`. Use `-` as `target` when an action has no target.

Replay validation rejects unknown versions, malformed integers, unknown action tokens, and missing fields with a line-numbered diagnostic.

Snapshot hashes cover only simulation tick, ordered entity state, and sorted quota pairs. Pixels and audio are intentionally excluded.
