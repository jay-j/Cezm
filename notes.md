# The Magic
A scheduling/planning application where the primary output is a 2D map; time x person-effort. And has clear 'flow' indicators to tie together dependencies.
- Target user is a small team project, where personnel availability is a tight constraint.
- Super fast edit remap responsiveness
- Keyboard navigation in the tree, jump back and forth to the json editor. left hand bindings? move with AWSD, hit E to jump to editor side
- this UX! https://app.code2flow.com/ or http://viz-js.com/

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
- Difference between the json being edited and the full true json 

# Bonus
- Product/part architecture system with nesting
- Group by subsystem instead of by person
- Side panel showing auto-calculated properties
- Side panel showing hints for properties the user could add
- Hints for certain key values (user, dependent_on, mel_color...)
- Export huge png option
- put comments in the code
- Resolution finer than 1 day. Weekend vs. weekday scheduling.
- Holidays. 
- Marking of tasks as completed or not 
- Highlighting critical path
- zero-duration milestones
- Resize the various sub-pieces
- Highlight things that are the same or related to what's under the cursor. Names, dates, dependencies...
- Derived fields displayed in node focus are editable and change the source.
- Task groups / summary / rollup tasks; nested tasks. How does this work with partial edit mode? Also display the minimum/common parent information.
- Auto indenting/formatting of the file.
- Margin system. And/or schedule uncertainty system.
- Diff system - or just rely on git? Or have git UX elements within this system?
- Tab completion (for users, dependent-on task names..)
- Different scheduling constraint types? Earliest, latest, fixed start, fixed end....
- Priority or risk parameter for the solver to aid in choosing between solutions?
- Rearrange user columns to reduce dependency graph visual chaos; minimize total length of column changes

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
# Memory Conops
Allocate an array of Task_Node.
keep a counter for how many total are used; if exceeding the limit then grow allocation a lot
each node tracks whether it is in use or not. de-allocating a node is as simple as writing that that node is no longer in use?

create in sequential order? maybe there is a last_node_created_id tracker so that you can start guessing forwards from there. 

but need an efficient way to turn names into ids? use a hash table... punch in id, at location in hash table is stored index of where this thing is in memory?

what  is lifecycle of an activity vs. editing it? 
if/how to handle name changes?
treat as a breaking change; node created and deleted. have a special key for when renaming is desired. 
so anticipate create/delete  ops on nodes once per character entry even; definitely don't want to 

so then for each instant node creation need to go back into display mode to see the effect on schedule? NO, WANT INSTANT FEEDBACK!
auto create closing brackets so instant parsing doesn't get as screwed up?


## Memory Management
- Maintain a parallel array which is a pointer to open slots in the main array.
- Pull off the end of the pointer list, decrement its size when you need to allocate something. 
- Trashing function would be responsible for adding pointer onto the allocateable pointer list
- always store the location of the end of the list. so no need to parse it to add or remove entries from the end of that list
- Doesn't need to be a full list? Just store the end? 


## Plotting
- Use Bezier curves to draw depedency graph
- Just put a dot if tasks are adjacent and for the same user
- Single 'emitter' location for a given task
- Multiple 'entry' ports for a given task. will probably need an untangling mechanism if this is used
- partial transparency where a curve crosses an unrelated task
  - one time.. generate an alpha channel mask for the whole layer
- arrange columns to minimize total curve LR length. side effect will group people whose jobs are more related
- hold in memory a curves texture that is the size of the display viewport
- only update it if needed. this texture gets the alpha channel. how will this not overwrite everything? put it at the back? no, because want full color task blocks
- maybe mask is a secondary texture just for the alpha multiply
- should I artificially space out tasks to help spread out the dependency indicators? 
  - this could be static (every 5 days put 5 'blank' days)
  - this could by dynamic (end of every task put blank days. could this create problems for long running tasks with many in parallel alongside? would make time nonlinear vs. graphics
- color code the curves to match the source color. or black if there is no source color?

### Bezier Curves
- Use a 'cubic' curve; with four control points. 
- `B(t) = (1-t)^3 P[0] + 3*(1-t)^2 t P[1] + 3*(1-t) t^2 P[2] + t^3 P[3] ` for `0 <= t <= 1`
- control points P[0] and P[3] place directly on the task. Control points P[1] and P[2] place a bit offset to control the curve
- stroking the curve to give it width is going to be a problem

# Profiling
Edit makefile so `CFLAGS = -O0 -Wall -Wextra -g -pg`. 
Compile. Run program, force it to do a bunch of text parsing and rescheduling. 
The program will create a binary gmon.out file when it exits normally.
Run `$ gprof main.bin gmon.out > prof_output.txt` to convert that binary file into something human readable. 
