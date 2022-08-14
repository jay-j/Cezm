A schedule planning tool for small teams, with:
- fast modal editing, 
- high density representation,
- instant scheduling as you type,
- plaintext file format readable by humans and version control systems.

# Setup
## Dependencies
SDL2
- `libsdl2-dev`
- `libsdl2-image-dev`
- `libsdl2-ttf-dev`

## Build & Run
Linux: `$ make`. Will generate an executable `main.bin`. Run this with the schedule filename as an required command line argument.

# Usage
The left pane is the `edit viewport`. The right pane is the `display viewport`. Switch modes with `TAB` (the default shortuct. See [keyboard_bindings.h](keyboard_bindings.h) to view and change).

In the `edit viewport`, describe the project in a json-like syntax (see [examples/demo1.json](examples/demo1.json)). The schedule will be solved as you type, and tasks will be plotted in the `display viewport`.]
- Rename a symbol with `F2`. This engages multicursor mode, exit it with `ESCAPE`.

The `display viewport` is used to explore the resulting schedule, perform some actions, and select specific tasks to edit. The right hand is used for navigation, the left hand for actions.
- Keyboard navigation: Deselect tasks with `SPACE`, navigate with `hjkl` (vim), select with `f`.
- Mouse navigation: Deselect tasks by `LMB` on the background. Select tasks by `LMB` on them.
- Split a task with `x`.
- **A**dd a successor (dependent) with `a`.
- Select single **S**uccessors (dependents) with `s`. Select all with `SHIFT+s`.
- Select single prerequisite with `w`. Select all with `SHIFT+w`.
