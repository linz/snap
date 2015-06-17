
#########################################################################
# Function to open a snap job

function OpenSnapJob()
    $file = GetOpenFileName("Select SNAP job file","Snap command files (*.snap, *.snp, *.cmd)|*.snap;*.snp;*.cmd|All Files (*.*)|*.*")
    if $file then LoadJob($file) endif
end_function

#######################################################################
# Function to edit command files

function EditCommandFile()
	start($_editor,$job_file)
end_function
 
#########################################################################
# Function to create a snap job

function CreateNewSnapJob()

if not UnloadJob() then return false endif

$_title="Untitled job"
$_coordsys = "NZGD2000"

#===================================================================
# Get title, command file, coordinate file, coordinate system

dialog "New SNAP job"
textbox $_title "Job title" "50"
save_file_selector $commandfile "SNAP command file" "Snap command files (*.snp)|*.snp|All files (*.*)|*.*"
validator
   if not $commandfile then
      message("You must enter a command file name","Input error")
      return false
   elseif fileexists($commandfile) then
      if ask("The command file already exists .. do you want to overwrite it","Overwrite?") then
         return true
      else
         return false
      endif
  endif
  return true
save_file_selector $coordinatefile "Coordinate file" "Snap coordinate files (*.crd)|*.crd|All files (*.*)|*.*"
radio_selector $crdfileoption "Coordinate file option" "~use~Use the selected file~copy~Copy the selected file to the same name as the command file~new~Create a new empty file with the same name as the command file"
validator
   if $crdfileoption == "use" or $crdfileoption == "copy" then
      if not fileexists($coordinatefile) then
         message("You must enter an existing coordinate file to use or copy it","Input error")
         return false
       endif
   endif
   if $crdfileoption == "new" or $crdfileoption == "copy" then
       $crdfilenew = SetExtension($commandfile,"crd")
       if FileExists($crdfilenew) then
          if not ask("The coordinate file $crdfilenew already exists\nDo you want to overwrite it","Overwrite?") then
             return false
          endif
       endif
   endif
          
   return true
dropdown_selector $_coordsys "Coordinate system" $coordsys_list
end_dialog


#=============================================================
# Set up the filename for the various files

$jobpath = Filename($commandfile,"path")

$coordinatefile_source = $coordinatefile
if $crdfileoption != "use" then $coordinatefile = SetExtension($commandfile,"crd") endif
if $coordinatefile == $commandfile then
   message("The coordinate file and command file cannot be the same")
   exit
endif


$datafiles = DataFileDialog($jobpath)

#=============================================================
# Create the job file

$_coordfile = Filename($coordinatefile,"relative",$jobpath)

$_datafilelist = $datafiles

if not CopyTemplate("default.snp",$commandfile) then 
   message("Cannot create the command file","Error")
   exit
endif  

#=============================================================
# Create the coordinate file file

if $crdfileoption == "copy" then
   debuglogrun("\"$snap_path/snapconv.exe\" \"$coordinatefile_source\" \"$_coordsys\" \"$coordinatefile\"")
elseif $crdfileoption == "new" then
   $crdfileok = CopyTemplate("default.crd",$coordinatefile)
endif

if not FileExists($coordinatefile) then
   message("Could not create coordinate file")
   exit
endif

#=============================================================
# Load the job

LoadJob($commandfile)

log("
The new job has been created. 
Now you need to edit the command file to choose which stations to fix
")

if $crdfileoption == "new" then
   log("You also need add stations to the coordinate file")
endif 

end_function

######################################################################
# Functions to read and write the job file

function ReadJobFile()
	return ReadFile( $job_file )
end_function
	
function UpdateJobFile( $newcontent )
	$updatedjobfile = TempFile()
	if not WriteFile($updatedjobfile,$newcontent) then
		message("Cannot create updated command file","Error")
		exit
	endif
	if not CopyFile($updatedjobfile, $job_file) then
		message("Cannot overwrite command file","Error")
		exit
	endif
	DeleteFile($updatedjobfile)
end_function

