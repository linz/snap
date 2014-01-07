
function FindConfigFiles( $cfgdir, $ext )
   $cfgfiles=""
   $area="Job"
   if $job_path then
      foreach $file in Directory($job_path,"*.$ext") do
          if not $file then continue endif
	  $filepath=Filename($file,"absolute",$job_path)
	  $cfile=Filename($file,"name")
	  $cfgfiles = $cfgfiles."\n$cfile ($area) $filepath"
      endforeach
   endif
   $area="User"
   foreach $config_path in $user_config_path, $system_config_path do
      $packagedir=Filename("package","absolute",$config_path)
      foreach $pkgdir in Directory($packagedir) do
         if not $pkgdir then continue endif
      	 $pkgcfg="$packagedir/$pkgdir/config/$cfgdir"
	 foreach $file in Directory($pkgcfg,"*.$ext") do
             if not $file then continue endif
	     $filepath=Filename($file,"absolute",$pkgcfg)
	     $cfile=Filename($file,"name")
	     $cfgfiles = $cfgfiles."\n$cfile ($area/$pkgdir) $filepath"
	 endforeach
      endforeach
      $pkgcfg="$config_path/$cfgdir"
      foreach $file in Directory($pkgcfg,"*.$ext") do
          if not $file then continue endif
	  $filepath=Filename($file,"absolute",$pkgcfg)
	  $cfile=Filename($file,"name")
	  $cfgfiles = $cfgfiles."\n$cfile ($area) $filepath"
      endforeach
      $area="System"
   endforeach
   return $cfgfiles
end_function

function ConfigFileListBoxOptions( $cfgfiles )
   $options=''
   foreach $cfile in $cfgfiles do
      # debuglog($cfile)
      if $cfile then
      	$options=$options.'|'.Match($cfile,'^[^\)]*\)')
      endif
   endforeach
   return $options
end_function

function ConfigFileListFileName( $cfgfiles, $option )
   $filename=""
   foreach $cfile in $cfgfiles do
      $opt=Match($cfile,'^[^\)]*\)')
      debuglog("Trying |$opt|$option|")
      if $opt == $option then
         debuglog("Found $cfile")
         $filename=Replace($cfile,'^[^\)]*\)\s*','')
	 break
      endif
   endforeach
   return $filename
end_function

function EditConfigFile( $name, $description, $cfgdir, $ext )
   $cfgfiles=FindConfigFiles($cfgdir,$ext)
   $copydirs=""
   $copydir=$user_config_path
   if $job_path then 
       $copydirs = $copydirs . "|$job_path|Job directory" 
       $copydir = $job_path
   endif
   $copydirs=$copydirs."|$user_config_path/$cfgdir|User configuration"
   $copydirs=$copydirs."|$system_config_path/$cfgdir|System configuration"
   dialog "Edit $name" $result "cancel"
      label $description
      label "Note: you may need to run as an Administrator to edit system configuration files"
      spacer
      new_row
      listbox $srcopt "Current files" ConfigFileListBoxOptions($cfgfiles)
      spacer
      button "Edit file"
        $srcfile=ConfigFileListFileName($cfgfiles,$srcopt)
        if not $srcfile then
	   Message("You must select a file to edit","Missing file name")
	else
	   start($_editor,$srcfile)
	endif
      new_column
      radio_selector $copydir "Copy file to" $copydirs
      textbox $copyname "Rename copy to" "15:1:~abcdefghijklmnopqrstuvwxyz_01234556789"
      spacer
      button "Copy and edit file"
        $srcfile=ConfigFileListFileName($cfgfiles,$srcopt)
        $copyfile="$copydir/$copyname.$ext"
	$doedit=true
        if not $srcfile then
	   Message("You must select a file to copy","Missing file name")
	   $doedit=false
	endif
        if $doedit and not $copyname then
	   Message("You must enter a name for the copied file","Missing file name")
	   $doedit=false
	endif
	if $doedit and FileExists($copyfile) then
	   dialog "Overwrite file" $doedit 
	      label "The file $copyfile already exists"
	      label "Do you want to overwrite the file?"
	   end_dialog
	endif
	if $doedit then
	   MakeDir(Filename($copyfile,"directory"))
	   $doedit=CopyFile($srcfile,$copyfile)
	   if not $doedit then
	      Message("Cannot copy the configuration file to $copyfile","Copy error")
	   endif
	endif
	if $doedit then
           $cfgfiles=FindConfigFiles($cfgdir,$ext)
	   start($_editor,$copyfile)
	endif

    end_dialog
end_function
