Native Text Atlas Generator

Thank you for downloading the Native Text Atlas Generator for Adobe Photoshop.
This tool consists of two parts:

1. The Photoshop Plugin (UI)

2. The Native Engine (Backend)

Both parts must be running for the tool to work.


INSTALLATION GUIDE

STEP 1: Install the Photoshop Plugin

- Locate the file named "NativeTextAtlas.ccx" in this folder.

- Double-click it.

- Adobe Creative Cloud will open and ask for confirmation. Click "Install" or "Install Locally".

- Once installed, you will see it in Photoshop under the "Plugins" menu.


STEP 2: Start the Native Engine

- Open the "NativeEngine" folder included in this zip.

- Double-click "TextAtlasEngine.exe".

- A black console window will appear saying "Listening on port 4567...".

KEEP THIS WINDOW OPEN while using the plugin. <<(You can minimize it, but do not close it).


STEP 3: Usage

- Open Adobe Photoshop.

- Go to Plugins > Native Text Atlas Generator.

- The panel will open and the status should turn GREEN ("Engine Connected").

- Type your text, load a font, and click "Create Doc" or "Export Files".


TROUBLESHOOTING

Q: The status stays purple ("Sim Mode" / "Engine Offline").
A: Ensure you have "TextAtlasEngine.exe" running. If it is running, check if your firewall blocked "port 4567" (Localhost).

Q: I get a "System Error" or "Missing DLL" when opening the Engine.
A: Ensure you have extracted the ENTIRE zip file. Do not move the .exe out of the NativeEngine folder; it needs the .dll files next to it.

Q: My custom font isn't loading.
A: Ensure the font file (.ttf or .otf) is not corrupted and you have read permissions for that folder.


REQUIREMENTS

- Windows 10/11 (64-bit)
- Adobe Photoshop CC 2022 (v23.0) or higher