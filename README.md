# **Native Text Atlas Generator for Photoshop**

A powerful, high-performance Photoshop plugin for generating professional bitmap fonts and text atlases. It combines a native C++ engine (FreeType, HarfBuzz, MaxRects packing) with a modern Adobe UXP panel to deliver correct text shaping, efficient packing, and seamless Photoshop integration.  
*(Add a screenshot of your plugin UI here)*

## **Features**

* **Native Rendering:** Uses **FreeType** and **HarfBuzz** for industry-standard text rendering and complex script shaping.  
* **Efficient Packing:** Implements the **MaxRects** bin-packing algorithm with Auto-Sizing logic (POT or Multiple of 4\) to minimize texture waste.  
* **Custom Fonts:** Load any .ttf or .otf file directly from your disk, or use system fonts.  
* **Effect Padding:** Automatically reserves extra space around glyphs to prevent clipping when applying Photoshop Layer Styles (Strokes, Shadows, Glows).  
* **Standard Export:** Exports .png atlases and .fnt (BMFont) metadata compatible with **Unity**, **Godot**, **Unreal Engine**, and other game frameworks.  
* **Photoshop Integration:** \* Creates a fully layered document from the generated atlas.  
  * Automatically slices glyphs into separate layers.  
  * Groups everything into a neatly organized Smart Object.  
  * Supports iterative updates (overwrite existing doc).  
* **Global Adjustments:** Fine-tune xAdvance, xOffset, and yOffset globally for perfect alignment.

## **Installation**

This tool consists of two parts: the **Native Engine** (C++) and the **UXP Plugin** (UI). Both must be running.

### **1\. Download & Extract**

Download the latest release and extract the zip file to a permanent location.

### **2\. Install the Plugin**

1. Locate the file NativeTextAtlas.ccx in the extracted folder.  
2. Double-click it. Adobe Creative Cloud will prompt you to install the plugin.  
3. Click **Install Locally**.

### **3\. Start the Native Engine**

1. Open the NativeEngine folder.  
2. Double-click TextAtlasEngine.exe.  
3. A console window will appear saying: Listening on port 4567...  
4. **Keep this window open** (minimize it) while using the plugin.

## **Usage Guide**

1. **Open Photoshop** and navigate to **Plugins \> Native Text Atlas**.  
2. **Check Connection:** The status indicator in the panel should turn **Green (Engine Connected)**.  
3. **Configure Font:**  
   * Select a system font or click the **Folder Icon** to load a custom .ttf/.otf file.  
   * Set the **Size** (pixels).  
4. **Enter Text:** Type the characters you want to pack in the **Glyphs** area.  
5. **Texture Settings:**  
   * **Sizing Method:** Choose Auto (POT) for game engines, or Fixed for specific needs.  
   * **Effect Padding:** Increase this if you plan to add Strokes or Drop Shadows in Photoshop.  
6. **Generate:**  
   * **Export Files:** Saves atlas.png and atlas.fnt to disk.  
   * **Create Doc:** Generates a new Photoshop document with sliced layers, ready for styling.

### **Applying Layer Styles (Workflow)**

1. Click **Create Doc**.  
2. In the new document, you will see separate layers for each glyph (Glyph\_65, Glyph\_66, etc.).
3. Apply your Layer Styles (Stroke, Gradient, Shadow) to these layers.
4. Your main atlas document is now updated with the effects\!
5. **Export Files** and **Overwrite** the newly created .png with layer style.

## **Troubleshooting**

**Q: The status remains "Offline" (Red/Grey).**

* Ensure TextAtlasEngine.exe is running.  
* Check if your firewall is blocking port 4567 on localhost.

**Q: "Missing DLL" error when starting the engine.**

* Ensure you have extracted *all* files. The .exe must stay in the same folder as the .dll files (zlib1, freetype, etc.).

**Q: Glyphs look cropped in my game engine.**

* You likely added a Stroke or Shadow in Photoshop but didn't increase **Effect Padding**.  
* Increase the **Effect Padding** value in the plugin (e.g., to 4 or 8\) and regenerate. This reserves empty space around the letters for your effects.

**Q: Textures have black edges/halos.**

* This is a premultiplied alpha issue. The engine now exports "bleeding white" transparent pixels to mitigate this. Ensure your game engine's texture import setting is set to **Straight Alpha** (or "Uncompressed" for pixel art).

## **Development / Building from Source**

### **Prerequisites**

* **Visual Studio 2022** (C++ Desktop Development)  
* **CMake**  
* **vcpkg** package manager  
* **Node.js** (optional, for UXP tools)

### **Build Instructions**

1. Clone the repository.  
2. Install dependencies via vcpkg:  
   `vcpkg install freetype harfbuzz nlohmann-json uwebsockets libuv zlib --triplet x64-windows`

3. Configure CMake:  
   `cmake -B build -S native-engine -DCMAKE_TOOLCHAIN_FILE=[path/to/vcpkg.cmake]`

4. Build:  
   `cmake --build build --config Release`
