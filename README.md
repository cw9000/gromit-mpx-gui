# gromit-mpx-gui
a quick and dirty addition to gromit-mpx.  Memory leaks galore, but very usable. Never programmed using gtk3.0 in C before.

This project makes use of <a href="https://github.com/bk138/gromit-mpx">gromit-mpx</a>.  My only addition is the gui.

<img src="https://github.com/cw9000/gromit-mpx-gui/data/on_kde_x11_gui.png"/>

<img src="https://github.com/cw9000/gromit-mpx-gui/data/gui-menu.png"/>


# Install

Follow the instructions from gromit-mpx.
A simple
```
git clone
cd gromit-mpx-gui
mkdir build && cd build
cmake ../
make
sudo make install
```
should be sufficient

# Usage

You will have the same functionality as gromit-mpx out of the box.  If you want to open the gui either hit `Alt+F8` or go to the sys-tray and select the item for `Toggle Graphics Menu`.

Now you should have a gui.  You have 6 tools to switch to.  And each tool has a set of 7 pen types, each with its own memory for options.  Options survive closing through a file probably located at `/home/$USER/.gromit_config`

To cut down on the size of the tool the descriptions for the sliders are given from hovering over them as tooltips(this needs a better alternative).  Also, there are possibly more options for each tool type than I am letting you set.  The options change based on what tool type you are using.  Another thing to cut down on space is an overflow button right after the overall-size slider, if your tool needs the overflow.

Once you have selected your options for your tool, you can hit the colorful screen button at the bottom to start drawing.  When you do this, the gui menu will hide itself and you can draw as you want.  To get out of drawing mode, you can either use a right-click or you can hit `F9`.  Once you are back in normal mode you can hide the drawing using the slashed-through eye, or you can delete it entirely using the red trashcan.  You can Undo and Redo using the curved arrows below.  You can move the gui-menu by clicking the four arrows button and you can close the gui-window by either hitting `Alt+F8` or by hitting the circle red x button.  There is also an overall opacity slider below the 6th tool.

If you want more tools than 6, or less, you will have to edit `main.h` on line 67.  There you will find `#define GROMIT_NUMBER_OF_GUI_TOOLS 6`. Define it to how many tools you want and recompile and re-install.

If you want to set `Alt+F9` to both close and open Gromit-mpx-gui, do the following.  Set a global keyboard shortcut in your desktop-environment settings
and set it to run `/usr/local/bin/gromit-mpx --opentoggle`.  If you want it to start with the gui open set it to run `/usr/local/bin/gromit-mpx --opentoggle --start-gui`.  `--start-gui` doesn't make sense when you are closing gromit-mpx-gui, but it mostly ignores it in that case.

# Conclusion

I hope you are able to find this tool useful.  A feature I would find useful is the ability to move/copy/cut things, as well as making text boxes.  May fiddle with it more when I have more time.

Happy Trails.

