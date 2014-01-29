#######################################################################
# Functions for the tools menu
#######################################################################
# Function to run a command shell

function RunCommandShell() 
   start("\"$_commandshell\" $_commandparams")
end_function   
 
#######################################################################
# Function to clear the log window

function ClearLogWindow()
   ClearLog()
end_function
 
#######################################################################
# Function to install a snap package

function InstallSnapPackage()
   $perl = FindPerl()
   if not $perl then exit endif

   $package = ''
   $result = ''
   $allusers = ''
   dialog "Install snap package" $ok
   checkbox $allusers "Install for all users"
   open_file_selector $package "Snap package file" "Snap package files (*.zip)|*.zip"
   end_dialog
   if $ok then
      if $package != '' then
         $installdir="$user_config_path/package"
         if $allusers then $installdir="$system_config_path/package" endif
         log("Installing $package\n")
         $command = "install_snap_package.bat  \"$package\" \"$installdir\""
         $result = debuglogrun($command)
         log($result)
         $pkgdir="$installdir/$pkgdir"
         if DirectoryExists($pkgdir) then
             LoadPackage($pkgdir)
         endif
      endif
   endif
end_function
 
#######################################################################
# Function to list packages

function ListInstalledPackages()
   ClearLog()
   log("The following packages are currently installed:")
   $area="User"
   foreach $config_path in $user_config_path, $system_config_path do
      $packagedir=Filename("package","absolute",$config_path)
      foreach $pkgdir in Directory($packagedir) do
         if not $pkgdir then continue endif
         log("    ($area) $pkgdir")
      endforeach
      $area="System"
   endforeach
end_function
 
#######################################################################
# Function to setup configuration options

function SetConfigurationOptions()
   $editor = $_editor
   dialog "Configuration items" $ok
   open_file_selector $editor "Text file editor" "Executables (*.exe)|*.exe"
   end_dialog
   if $ok then
      if $editor != $_editor then 
         $_editor = $editor
         SetConfig("editor",$editor)
      endif
   endif
end_function

