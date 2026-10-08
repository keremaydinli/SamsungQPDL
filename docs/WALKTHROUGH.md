# Walkthrough: from this page to your first print (and back)

This is the whole journey as a new user sees it, using the installer
package. No coding needed. For the short version, see the
[README](../README.md).

Meet Alex: they have a Samsung ML-1675, a Mac on a recent macOS, and no
interest in Terminal.

---

## Part 1: Install and print

### 0. The problem

Alex plugs in the printer. macOS only offers generic drivers, and HP's
Samsung driver package prints the same page every time:

> INTERNAL ERROR - Please use the proper driver

A search for "ML-1675 mac driver" leads them here.

### 1. The repo page (about 1 minute)

- The README's first paragraph quotes the exact error Alex has been seeing.
- The *Supported printers* table lists the ML-1675 as tested.
- Alex picks **Option A: installer package** and notices the highlighted
  note: *click Done, not Move to Trash*.

### 2. Download (about 30 seconds)

**Releases → latest version → `SamsungQPDL-x.y.z.pkg`**. The file (about
30 KB) lands in Downloads.

### 3. Gatekeeper (about 1 minute)

Alex double-clicks the package. macOS refuses to open it and says Apple
could not verify it is free of malware. This is expected: the package isn't
notarized by Apple, which requires a paid developer account.

1. Click **Done**.
2. Open **System Settings → Privacy & Security** and scroll down.
3. Next to the message about SamsungQPDL, click **Open Anyway** and confirm
   with your password or Touch ID.

### 4. Installer (about 30 seconds)

**Continue → Install**, enter the password, then **Close** when it says
*The installation was successful*.

Three things are installed, all under `/Library/Printers`: the driver
filter, the printer description (PPD), and an uninstaller. Nothing else on
the Mac changes.

### 5. Add the printer (about 1 minute)

1. Alex had already added the printer with HP's driver, so they remove it
   first: **System Settings → Printers & Scanners**, select the printer,
   click **−** and confirm.
2. Click **Add Printer, Scanner or Fax…** and select
   **Samsung ML-1670 Series** (USB). The ML-1675 reports itself under
   the ML-1670 name, so this is the right one.
3. Under **Use**, choose **Select Software…**, type **QPDL** in the filter
   box and pick **Samsung ML-1670/1675 Series (open QPDL driver)**.
   Click **OK**, then **Add**.

### 6. The first page (about 30 seconds)

Alex opens a document and presses **⌘P**. Besides the usual settings, the
print dialog now offers **Toner Save**, **Toner Density**, **Paper Source**
and **Halftone**. They click **Print**, the printer wakes up, and a real
page comes out.

Total: about 5 minutes.

---

## Part 2: Removing it (months later)

Alex gets a new printer and wants the old driver gone.

### 7. Find the uninstaller (about 30 seconds)

In Finder: **Go → Go to Folder…** (⇧⌘G), type
`/Library/Printers/SamsungQPDL` and press Return. The folder contains
`Filters` and **Uninstall-SamsungQPDL.command**.

### 8. Run it (about 30 seconds)

Alex double-clicks the script. A Terminal window opens and explains what it
will do before touching anything:

```
SamsungQPDL uninstaller
=======================

This will delete the driver files:
  /Library/Printers/SamsungQPDL
  /Library/Printers/PPDs/Contents/Resources/Samsung-ML-1675-QPDL.ppd.gz

and these printers, which use the driver:
  Samsung_ML_1670_Series

Continue? [y/N]
```

They type `y`, press Return and enter their password (it stays invisible
while typing):

```
Removed printer Samsung_ML_1670_Series
Removed /Library/Printers/SamsungQPDL
Removed /Library/Printers/PPDs/Contents/Resources/Samsung-ML-1675-QPDL.ppd.gz

SamsungQPDL has been removed. You can close this window.
```

### 9. Done

- The printer is gone from **Printers & Scanners**.
- The driver is gone from the **Select Software…** list.
- macOS has forgotten the package was ever installed.

Total: about 1 minute, without typing a single command.

---

## Other paths

- **Can't find the folder?** Download `Uninstall-SamsungQPDL.zip` from the
  [Releases](https://github.com/keremaydinli/SamsungQPDL/releases) page, unzip it and double-click the script.
  Because it was downloaded, macOS blocks it once: use
  **Privacy & Security → Open Anyway** again.
- **Changed your mind at the prompt?** Any answer other than `y` prints
  *Nothing was changed.* and stops.
- **Prefer building from source?** See *Option B* in the
  [README](../README.md#option-b-build-from-source).
