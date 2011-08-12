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
   dialog "Install snap package" $ok
   open_file_selector $package "Snap package file" "Snap package files (*.zip)|*.zip"
   end_dialog
   if $ok then
      if $package != '' then
         log("Installing $package\n")
         $command = "\"$snap_path/install_snap_package.bat\"  \"$package\""
         $result = debuglogrun($command)
         log($result)
      endif
   endif
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

