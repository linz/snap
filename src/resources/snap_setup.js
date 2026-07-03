// Usage: cscript snap_setup.js <install|uninstall> <install directory>
//
// Ported from the old vdproj installer's MSI custom action snap_setup.vbs,
// which read these two values from Session.Property("CustomActionData") - an
// object that only exists inside an MSI custom action. Invoked from NSIS
// instead, so it takes them as plain command-line arguments.
//
// Written in JScript rather than VBScript because the VBScript engine is
// being phased out by Microsoft as an optional, removable Windows feature,
// and was already missing (registered extension with no ProgID) on a
// current Windows Sandbox test image. If this breaks again on a future
// Windows version, check whether JScript has followed VBScript down the
// same path (HKCR\.js should map to a ProgID like JSFile; `cscript
// //nologo test.js` with a trivial script is a quick way to confirm the
// engine itself still runs) before assuming the fix is elsewhere.

var action = WScript.Arguments(0).toLowerCase();
var snappath = WScript.Arguments(1);

// Installer-supplied directories may have a trailing backslash; strip it.
if (snappath.length > 1 && snappath.charAt(snappath.length - 1) === "\\") {
    snappath = snappath.substring(0, snappath.length - 1);
}

if (snappath !== "") {

    var sh = new ActiveXObject("WScript.Shell");

    // Registry key for the PATH environment variable
    var key = "HKLM\\SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment\\Path";

    var path = sh.RegRead(key);

    // Get rid of snappath from the existing path ... assume that it won't
    // be there more than once.
    var snappathLower = snappath.toLowerCase();
    var pathLower = path.toLowerCase();
    var snappathlen = snappath.length;

    if (pathLower === snappathLower) {
        path = "";
    } else if (pathLower.substring(0, snappathlen + 1) === snappathLower + ";") {
        path = path.substring(snappathlen + 1);
    } else if (pathLower.substring(pathLower.length - snappathlen - 1) === ";" + snappathLower) {
        path = path.substring(0, path.length - snappathlen - 1);
    } else {
        var idx = pathLower.indexOf(";" + snappathLower + ";");
        if (idx >= 0) {
            path = path.substring(0, idx + 1) + path.substring(idx + snappathlen + 2);
        }
    }

    // Add SNAP directory to the path if installing
    if (action === "install") {
        path = snappath + ";" + path;
    }

    // Write the key
    sh.RegWrite(key, path, "REG_SZ");
}
