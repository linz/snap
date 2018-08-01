#######################################################################
# Functions for the tools menu
#######################################################################
# Function to run a command shell

function RunCommandShell() 
   $command=$_commandshell
   if $_commandparams then
      $command="\"$command\" $_commandparams"
   endif
   start($command)
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
         $result = debuglogrun($perl,'-S','install_snap_package.bat',$package,$installdir)
         log($result)
         $pkgdir="$installdir/$package"
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
# Function to remove packages

function RemoveInstalledPackage()
   ClearLog()
   $packagelist=''
   $area="User"
   foreach $config_path in $user_config_path, $system_config_path do
      $packagedir=Filename("package","absolute",$config_path)
      foreach $pkgdir in Directory($packagedir) do
         if not $pkgdir then continue endif
	 $packagelist=$packagelist."~$packagedir/$pkgdir~$pkgdir ($area)"
      endforeach
      $area="System"
   endforeach
   dialog "Remove installed package" $ok
       label "Select the package to remove"
       dropdown_selector $delpkg "" $packagelist
   end_dialog
   if $ok then
       $pkgname=FileName($delpkg,"name")
       $ok=Ask("Are you sure you want to remove package $pkgname","Remove package")
       if $ok then
           if RemoveDir($delpkg) then
	   	log("Deleted package $pkgname")
	   	log("Please restart SNAP Manager")
	   endif
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

