######################################################################
# Functions relating to coordinate files
######################################################################
# Function to process the coordinate file and coordinates

$_crdfile_is_snap = true
$_tested_job_file = ''

function BackupCoordinateFile()
  $backup_file = false
  if $coordinate_file then
     $backup_file = SetExtension($coordinate_file, "~crd")
     if $backup_file != $coordinate_file then
        CopyFile($coordinate_file, $backup_file)
     else
        $backup_file = false 
     endif
  endif
  return $backup_file
end_function

function CheckCoordinateFile( $needsnap )
  if FileExists($coordinate_file) then 
     if $needsnap and not CoordinateFileIsSnap() then
        Message("This function can only be used with SNAP format coordinate files","Coordinate format error")
        return false
     endif
     return true 
  endif
  $message = "The coordinate file $coordinate_file is missing\n"
  if not $coordinate_file then
      $message = "The coordinate file is not defined in the command file\n"
  endif
  Message($message,"Coordinate error")
  return false
end_function
    
function EditCoordinateFile()
    start($_editor,$coordinate_file)
end_function

function CoordinateFileIsSnap()
   if $coordinate_file and $job_file != $_tested_job_file then
      $csvcrdfile = ReadFile($job_file,'(?i)^\s*coordinate_file\s+\S+\s+csv(?:\s|$)',1)
      $_tested_job_file = $job_file
      $_crdfile_is_snap = true
      if $csvcrdfile then $_crdfile_is_snap = false endif
   endif
   return $_crdfile_is_snap
end_function

function CoordFileCoordSys()
    $x=ReadFile($coordinate_file,'\S',5)
    $c=Match($x,'\n\s*(\S+)',1)
    $d=Match($x,'(?i)\n\s*options\s+(?:.*\s)?(degrees)(?:\s|$)',1)
    if $d == '' then $d='dms' endif
    return $c,$d
end_function

function SetCoordFileCoordSys( $coordsys, $convdate, $degformat )
    $backupfile = BackupCoordinateFile($coordinate_file)
    $relname = Filename($coordinate_file,"relative",$job_path)
    if not $backupfile then 
       return false,"Could not create backup file of $relname"
    endif
    if $convdate != "" then $convdate="-y $convdate" endif
    $degopt='-h'
    if $degformat == 'degrees' then $degopt='-d' endif
    $output = debuglogrun("\"$snap_path/snapconv\" $convdate $degopt \"$backupfile\" $coordsys \"$coordinate_file\"")
    $output = "$output"
    $result=false
    if $output == "true" then $output = ""  $result=true endif
    if ReadFile($coordinate_file,'.',5) != ReadFile($backupfile,'.',5) then
       $result=true
       $output="$output\n$relname coordinate system changed to $coordsys"
    endif
    return $result, $output
end_function

#######################################################################
# Function to change the coordinate system of the coordinate file

function ChangeCoordinateSystem()
    if not CheckCoordinateFile(true) then exit endif
    $coordsys,$degformat=CoordFileCoordSys()

    $relname = Filename($coordinate_file,"relative",$job_path)
    
    # Would be much nicer if knew what it is in coordinate file
    $convdate=""
    dialog "Change coordinate system"
       label "Update the coordinate system of the station file"
       label "Coordinate file: $relname"
       spacer
       dropdown_selector $coordsys "Select the new coordinate system" $coordsys_list
       spacer 
       textbox $convdate "Conversion epoch (if required, YYYY-MM-DD)" '10:1:~0123456789:/-~^((19|20)\d\d([-/:])[01]\d\3[0123]\d)?$~Date must be formatted as YYYY-MM-DD'
       spacer 
       radio_selector $degformat "Angle format: " "~degrees~Decimal degrees~dms~Deg/Min/Sec"
       validator
           $result,$message=SetCoordFileCoordSys($coordsys,$convdate,$degformat)
	   if $result then 
	   	Log($message)
	   else
	   	Message($message,"Error")
	   endif
	   return $result
    end_dialog
end_function

#######################################################################
# Function to add geoid heights

function AddGeoidHeights()
    if not CheckCoordinateFile(true) then exit endif
    $relname = Filename($coordinate_file,"relative",$job_path)
    $geoid = "nzgeoid09"
    
    # Would be much nicer if knew what it is in coordinate file
    $coordsys = "NZGD2000"
    dialog "Add geoid heights and deflections"
       label "Add geoid heights and deflections to the station file"
       label "Coordinate file: $relname"
       spacer
       radio_selector $geoid "Select the new geoid to use" "~nzgeoid09~New Zealand Geoid 2009~egm96~EGM96"
    end_dialog
    $backupfile = BackupCoordinateFile($coordinate_file)
    if not $backupfile then 
       message("Could not create backup file of $relname","Error")
       exit
    endif
   $output = debuglogrun("\"$snap_path/snapgeoid\" -c -g $geoid \"$backupfile\" \"$coordinate_file\"")
   log("\n$output\nGeoid heights added to $relname")
end_function

#######################################################################
# Function to add calculate missing stations

function CalculateMissingStations()
    if not CheckCoordinateFile(true) then exit endif
    $backupfile = BackupCoordinateFile($coordinate_file)
    if not $backupfile then 
       message("Could not create backup file of $relname","Error")
       exit
    endif
    $newfile = SetExtension($coordinate_file,"new")
    DeleteFile($newfile)
    $command = "\"$snap_path/dat2site\" \"$job_file\""
    $output = debuglogrun($command)
    log("\n")
    log($output)
    if FileExists($newfile) then
       CopyFile($newfile,$coordinate_file)
       log("\nCoordinate file updated with new coordinates")
    endif
end_function

#######################################################################
# Function to import stations from a coordinate file

function ImportCoordFile()
    $relname = Filename($coordinate_file,"relative",$job_path)
    $overwrite = false
    $onlyindata = true
    
    # Would be much nicer if knew what it is in coordinate file
    $coordsys = "NZGD2000"
    dialog "Import coordinates"
       label "Import coordinates from another coordinate file"
       label "Coordinate file: $relname"
       spacer
       open_file_selector 
           $importfile "Import file" 
           "SNAP coordinate file (*.crd)|*.crd|All files (*.*)|*.*" 
          "You must specify an import coordinate file"
       spacer
       checkbox $overwrite "Overwrite existing coordinates with values from source file"
       checkbox $onlyindata "Only import stations used in the job data files"

       validator
           if not FileExists($importfile) then
              Message("You must choose an existing coordinate file to import","Missing file")
              return false
           endif
           return true 
           end_dialog

    if not FileExists($coordinate_file) then
        CopyFile($importfile,$coordinate_file)
    endif

    $backupfile = BackupCoordinateFile($coordinate_file)
    if not $backupfile then 
       message("Could not create backup file of $relname","Error")
       exit
    endif
    
    $options = ""
    if $overwrite then $options = "-o" endif

    if $onlyindata then
       $listfile = TempFile()
       $command = "\"$snap_path/dat2site\" -l -o \"$listfile\" \"$job_file\""
       debuglogrun($command)
       $options = $options." -l \"$listfile\""
    endif
    
    $result = debuglogrun("\"$snap_path/snapmerge\" -q $options \"$coordinate_file\" \"$importfile\"")
    
    log("\nCoordinates imported from $importfile")
       
end_function

#######################################################################
# Function to import stations from the LINZ GDB

function ImportLINZGDB()
    $perl = FindPerl()
    if not $perl then exit endif

    if not CheckCoordinateFile(true) then exit endif
    $relname = Filename($coordinate_file,"relative",$job_path)
    
    # Get the overwrite option

    $overwrite = true
    $addmarktypes = true
    $clearorders = true
    dialog "Import LINZ GDB coordinates"
       label "Import coordinates from LINZ geodetic database for all station"
       label "referenced in the data files or the coordinate file"
       spacer
       label "Note: This function requires perl to be installed and internet"
       label "access to http://www.linz.govt.nz"
       spacer
       label "Coordinate file: $relname"
       spacer
       checkbox $addmarktypes "Include mark types in downloaded coordinates"
       checkbox $overwrite "Overwrite existing coordinates with values from GDB"
       checkbox $clearorders "Clear existing coordinate orders"
    end_dialog

    $backupfile = BackupCoordinateFile($coordinate_file)
    if not $backupfile then 
       message("Could not create backup file of $relname","Error")
       exit
    endif
    
    $options = ""
    if $overwrite then $options = "-o" endif

    # Look up station referenced in the data files not already in the
    # coordinate file
 
    $listfile = TempFile()
    $command = "\"$snap_path/dat2site\" -m -o \"$listfile\" \"$job_file\""
    debuglogrun($command)
    $missing = ReadFile($listfile)
    DeleteFile($listfile)
    
    # Construct an input file for the web script containing both the input stations
    # and any missing stations.  Not that the linz_gdb_stations script just uses the 
    # title line of the coordinate file and the first non-blank field of each subsequent
    # line, so doesn't need a proper station file as input

    $inputfile = TempFile()
    CopyFile($coordinate_file,$inputfile)
    AppendFile($inputfile,$missing)
    
    debugfile("Input file",$inputfile)

    # Run the perl script to retrieve the coordinates from the geodetic database

    $newcrdfile = TempFile()
    DeleteFile($newcrdfile)
    $options=""
    if $addmarktypes then $options="-m" endif
    $command = "perl -S linz_gdb_stations.bat $options \"$inputfile\" \"$newcrdfile\""
    $gdbresult = debuglogrun( $command )
    DeleteFile($inputfile)

    if not FileExists($newcrdfile) then
       Message("Unable to retrieve coordinates from the LINZ geodetic database\n$gdbresult","GDB error")
       exit
    endif

    debugfile("Output file from gdb",$newcrdfile)
    $check = ReadFile($newcrdfile)
    if not Match($check,'^.*\n(?:NZ|RSR)GD2000\n') then
       Message("Unable to retrieve coordinates from the LINZ geodetic database","GDB error")
       exit
    endif

    # Merge back in the old coordinate file.  If not overwriting then overwrite the new
    # coordinates from the GDB.  Do it this way round to preserve GDB coordinate system and
    # coordinate orders

    $options = ""
    if not $overwrite then $options = "-o" endif
    if $clearorders then $options = $options." -cd" endif

    $result = debuglogrun("\"$snap_path/snapmerge\" -q $options \"$newcrdfile\" \"$coordinate_file\"")
    RenameFile($newcrdfile,$coordinate_file)

    log("\nCoordinates imported from LINZ geodetic database")
end_function

