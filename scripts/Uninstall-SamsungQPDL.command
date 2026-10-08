#!/bin/bash
#
# Uninstall SamsungQPDL: removes the driver files and the printers that
# were set up with it. Double-click to run, or: bash Uninstall-SamsungQPDL.command
#

PREFIX=/Library/Printers/SamsungQPDL
PPD=/Library/Printers/PPDs/Contents/Resources/Samsung-ML-1675-QPDL.ppd.gz
PKGID=io.github.keremaydinli.samsungqpdl
NICKNAME="open QPDL driver"

echo
echo "SamsungQPDL uninstaller"
echo "======================="
echo

if [ ! -e "$PREFIX" ] && [ ! -e "$PPD" ]; then
    echo "SamsungQPDL is not installed. Nothing to do."
    exit 0
fi

# Printers that use this driver
queues=()
for f in /etc/cups/ppd/*.ppd; do
    [ -e "$f" ] || continue
    if grep -q "$NICKNAME" "$f" 2>/dev/null; then
        queues+=("$(basename "$f" .ppd)")
    fi
done

echo "This will delete the driver files:"
echo "  $PREFIX"
echo "  $PPD"
echo
if [ ${#queues[@]} -gt 0 ]; then
    echo "and these printers, which use the driver:"
    printf '  %s\n' "${queues[@]}"
    echo
fi

read -r -p "Continue? [y/N] " answer
case "$answer" in
    [yY]*) ;;
    *) echo "Nothing was changed."; exit 0 ;;
esac

echo
echo "Enter your Mac password if asked (it stays invisible while you type)."
sudo -v || { echo "No password given. Nothing was changed."; exit 1; }

for q in "${queues[@]}"; do
    sudo lpadmin -x "$q" && echo "Removed printer $q"
done
sudo rm -rf "$PREFIX" && echo "Removed $PREFIX"
sudo rm -f "$PPD" && echo "Removed $PPD"
sudo pkgutil --forget "$PKGID" >/dev/null 2>&1

echo
echo "SamsungQPDL has been removed. You can close this window."
