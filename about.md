# SVG Converter

This mod allows you to import **.svg files as objects**.
Press Ctrl+I in the editor to choose a file, then open the menu, and press Import.

## Important to know

This mod is on beta, if you see any errors dm me to @pacoweb. in discord or create an issue on the repository, but please read this first!

* This mod was made for own vector artwork, a lot of files found online can break because of optimization, same as tool made (ej. from images) or complex files in general.
Do not dm me for those.

* The generated objects are a werid for the game because of really small scales, **NEVER rotate or use rotate triggers. Scale triggers also break.** Scale in editor works as long as is not too small. Moves and move triggers work fine (although the amount of objects can cause lag).

* This mod generates a lot of objects, and it will create 1:1 colors. If your file has 700 colors, that's 700 new colors in your level!

* Don't skip the editor while generating! the process will stop and creates even more colors.

* Gradients are naturally impossible.

* Maximum of 50.000 objects.

* Recommended to export in a non-optimize, flat format.

## Credits

Mod by Hexagon80

* **nanosvg** — parsing library
* [Allium mod](mod:alk.allium) — reference for geometry, and big motivation for the proyect
* **earcut.hpp** — triangulation library

Logo taken from: [svgrepo](https://www.svgrepo.com/svg/374110/svg)
Copyright (c) vscode-icons
Licensed under the **MIT License**
