# SamsungQPDL: macOS driver for the Samsung ML-1670 / ML-1675

An open-source printer driver that lets the **Samsung ML-1670 / ML-1675**
USB laser printers work on current macOS, including Apple Silicon Macs.

Samsung's own Mac drivers (now distributed by HP) stopped supporting these
models years ago. The newer Samsung Universal Print Driver doesn't speak
their dialect either: every page comes out as

> INTERNAL ERROR - Please use the proper driver

This driver speaks the printer's actual language: QPDL v5 with JBIG-compressed
bands (Samsung "algorithm 0x15").

## Supported printers

| Model     | Status                     |
|-----------|----------------------------|
| ML-1675   | Tested, works              |
| ML-1670   | Should work (same engine)  |

macOS usually reports both printers as **"Samsung ML-1670 Series"**.

If you test another model, please open an issue with the result.

Requirements: macOS 11 Big Sur or newer, Apple Silicon or Intel.

## Install

### Option A: installer package (easiest)

> [!IMPORTANT]
> macOS will block the installer the first time, with a message like
> **"Apple could not verify SamsungQPDL-x.y.z.pkg is free of malware"**.
> This happens because the package isn't notarized by Apple (that needs a
> paid developer account), not because something is wrong with it.
> **Click Done, not Move to Trash**, then follow step 3 below.

1. Download `SamsungQPDL-x.y.z.pkg` from the
   [Releases](../../releases) page.
2. Double-click it. When macOS blocks it, click **Done**.
3. Open **System Settings → Privacy & Security**, scroll down to the message
   about SamsungQPDL and click **Open Anyway**. Confirm with your password
   or Touch ID.
4. Follow the installer: **Continue → Install**, enter your password,
   then **Close**.
5. [Add the printer](#add-the-printer).

### Option B: build from source

You need the Xcode Command Line Tools (`xcode-select --install`).
No other dependencies.

```sh
git clone https://github.com/keremaydinli/SamsungQPDL.git
cd SamsungQPDL
make
sudo make install
```

Then [add the printer](#add-the-printer), or let the Makefile do it
(finds the printer on USB and creates a queue using this driver):

```sh
sudo make setup-queue
```

## Add the printer

1. Connect the printer over USB and switch it on.
2. **If you already added this printer before** (for example with HP's
   Samsung driver, which prints "INTERNAL ERROR" pages), remove it first:
   **System Settings → Printers & Scanners**, select the printer, click
   **−** and confirm.
3. Click **Add Printer, Scanner or Fax…** and select
   **Samsung ML-1670 Series** (USB). The ML-1675 reports itself under
   that name too.
4. Under **Use**, choose **Select Software…**, type **QPDL** in the filter
   box and pick **Samsung ML-1670/1675 Series (open QPDL driver)**.
5. Click **Add**, then print any document to test.

## Print options

| Option        | Values                                             |
|---------------|----------------------------------------------------|
| Paper Size    | A4, Letter, Legal, Executive, A5, JIS B5, envelopes |
| Paper Source  | Automatic, Manual Feeder                           |
| Toner Save    | Off, On                                            |
| Toner Density | Lightest … Darkest (darker helps with a low cartridge) |
| Halftone      | Photo (dithered grays), Text (pure black and white) |

For documents with mostly text, **Halftone → Text** gives the crispest letters.

## Uninstall

The uninstaller removes the driver files and any printers set up with this
driver. Nothing else is left behind.

**If you used the installer package:**

1. In Finder, choose **Go → Go to Folder…** (⇧⌘G), type
   `/Library/Printers/SamsungQPDL` and press Return.
2. Double-click **Uninstall-SamsungQPDL.command**. A Terminal window opens,
   lists what will be removed and asks you to confirm.
3. Type `y`, press Return and enter your Mac password (it stays invisible
   while you type).

The same script is also attached to every release as
`Uninstall-SamsungQPDL.zip`. Since it's downloaded, macOS blocks it the
first time just like the installer: use **Privacy & Security → Open Anyway**.

**From a source checkout:** `bash scripts/Uninstall-SamsungQPDL.command`

**By hand,** in Terminal:

```sh
sudo rm -rf /Library/Printers/SamsungQPDL
sudo rm -f /Library/Printers/PPDs/Contents/Resources/Samsung-ML-1675-QPDL.ppd.gz
sudo pkgutil --forget io.github.keremaydinli.samsungqpdl
```

then remove the printer in **System Settings → Printers & Scanners**.

## Troubleshooting

- **Nothing prints.** Check the job in the print queue window, or run
  `tail -50 /var/log/cups/error_log` in Terminal.
- **Printer drivers are deprecated** (a warning from `lpadmin`): Apple shows
  this for every classic driver. It still works.
- **Raw protocol test.** `make testpage && ./testpage > test.qpdl`, then
  `lp -d Samsung_ML_1670_Series -o raw test.qpdl` prints a test page
  without going through the macOS print system.

## How it works

```
App → PDF → macOS renderer (cgpdftoraster) → 8-bit gray raster
    → rastertoqpdl → dithered 1-bit page → 128-line bands → JBIG
    → QPDL v5 stream with PJL header → USB → printer
```

- `src/qpdl.c`: the QPDL v5 / JBIG encoder
- `src/rastertoqpdl.c`: the CUPS filter
- `ppd/Samsung-ML-1675-QPDL.ppd`: the printer description macOS reads
- `src/testpage.c`: standalone test page generator
- `scripts/Uninstall-SamsungQPDL.command`: the uninstaller

## Credits

- The QPDL protocol details come from [SpliX](https://github.com/valdikss/splix)
  by Aurélien Croc and contributors, including Leonardo Hamada's work on the
  JBIG 0x15 format. This driver is a separate, much smaller implementation of
  that format for macOS.
- JBIG compression uses [JBIG-KIT](https://www.cl.cam.ac.uk/~mgk25/jbigkit/)
  by Markus Kuhn (`third_party/jbigkit`).

## License

GPL-2.0. See [LICENSE](LICENSE). The bundled JBIG-KIT files are under
GPL-2.0-or-later.

Samsung is a trademark of Samsung Electronics. This project is not affiliated
with Samsung or HP.
