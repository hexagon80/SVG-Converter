# SVG Converter

This mod allows you to import **.svg files as objects** directly into the editor of Geometry Dash.

It is made for creators who want to import artwork made in graphic design software.

---

## Usage

Press **Ctrl + S** (or change the keybind in settings) to open the import menu.

1. Select an `.svg` file
2. Choose the **editor layer**
3. Adjust the **quality**
4. Press **Import**

The mod will fastly place the objects in the editor.

---

## Unsupported Features

SVG files containing the following elements may produce **bugs or broken import**:

* Unsupported tags such as `image`, `animate`, `switch`
* **Gradients**, absolutly
* **Holes** inside shapes (for example text outlines)

Shapes with holes will be imported **without the hole**, filling the entire area.

---

### Important Notes

* This mod is currently in **beta**.
* maximum of **50,000 objects**.

This mod is intended for importing **vector artwork**, not assets optimized for the web such as:

* icons
* highly optimized SVGs
* extremely complex files

Those may produce poor results or exceed object limits.

### Colors

The mod generates the exact colors of the svg. You can modify those channels as normal, but naturally, the svg will change.

### When exporting

For better results:

* Export SVGs in the **simplest possible format**
* Avoid optimizations or complex features
* Use flat colors
* Avoid gradients and filters

### Transformations

After importing, you can safely use:

* **Warp**
* **Flip X / Flip Y**

<cr>**Do NOT rotate**</c> the imported objects, some objects may disappear.

### Size Tips

Smaller SVGs usually import cleaner.
Try importing scaled down SVGs and scaling them up in the editor!

---

## Credits

Mod by Hexagon80

### Libraries

* **nanosvg** — parsing library
* [Allium](mod:alk.allium) reference for geometry, and main motivation of the proyect
* **earcut.hpp** — triangulation library

### Assets

Logo taken from: [svgrepo](https://www.svgrepo.com/svg/374110/svg)
Copyright (c) vscode-icons
Licensed under the **MIT License**
