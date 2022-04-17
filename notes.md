# The Magic
A scheduling/planning application where the primary output is a 2D map; time x person-effort. And has clear 'flow' indicators to tie together dependencies.
- Target user is a small team project, where personnel availability is a tight constraint.

# Crucial Features
- Swoosh dependency indicators.
- Click on any node 
- Text markup language to describe everything
- Select any node(s), shows the text for that node. 
- Click any node(s), greys out notes not on that critical dependency chain
- Wait state tasks (vendor?) 
- Synchronized tasks (multi user, have to happen at the same time, independent dependencies)
- Use MEL (++) color status codes to help show what progress areas there are
- Cursor location in the text description is highlighted in the main view display
- Save file is plaintext; editable by hand, diff and share over git
- Load/save files.
- Super fast edit remap responsiveness
- Keyboard navigation in the tree, jump back and forth to the json editor. left hand bindings? move with AWSD, hit E to jump to editor side
- Differnce between the json being edited and the full true json 

# Bonus
- Group by subsystem instead of by person
- Side panel showing auto-calculated properties
- Side panel showing hints for properties the user could add
- Export huge png option
- put comments in the code
- Hints for certain key values (user, dependent_on, mel_color...)
- Resolution finer than 1 week. Weekend vs. weekday scheduling.
- Holidays. 
- Marking of tasks as completed or not 
- Highlighting critical path
- zero-duration milestones
- Resize the various sub-pieces
- Highlight things that are the same or related to what's under the cursor. Names, dates, dependencies...

# The Format
Use "hjson":  https://hjson.github.io/
- This is a C++ HJSON parser... https://github.com/hjson/hjson-cpp
- This is a partially complete C HJSON parser https://github.com/karl-zylinski/jzon-c

- Explicit root node optional!

```json
actuator_build : {
  user : Jay
  dependent_on : [actuator_design, actuator_review]
  mel_color : 4

  // need some portion of these
  duration : 5
  fixed_start : 2022-03-14
  fixed_end : 2022-05-01

}

actuator_checkout : {
  user: [Jay, Ara]
  dependent_on : [actuator_build]
  mel_color : 8

}
```

# Pieces
## Nuclear GUI
- This is not really the whole piece. Need to pick some other platform specific libraries? Try to SDL, since it is portable.
- https://github.com/vurtun/nuklear/issues/226
- specifically `sdl_opengl3` looks fine?
- how do I change to a nicer font? haha
- might need too many hacks to get good keyboard controls.. just use plain SDL2 instead? 
 
## Format Parsing
- Use the 'JZON' parser? 
- 
